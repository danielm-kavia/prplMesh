/* SPDX-License-Identifier: BSD-2-Clause-Patent
 *
 * SPDX-FileCopyrightText: 2026 the prplMesh contributors (see AUTHORS.md)
 *
 * This code is subject to the terms of the BSD+Patent license.
 * See LICENSE file for more details.
 */

#ifndef PRPLMESH_CONTROLLER_ONBOARDING_APPROVAL_ADAPTER_H_
#define PRPLMESH_CONTROLLER_ONBOARDING_APPROVAL_ADAPTER_H_

#include <easylogging++.h>

#include <chrono>
#include <cstring>
#include <sstream>
#include <string>

#include <errno.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/un.h>
#include <unistd.h>

namespace son {
namespace onboarding_approval {

/**
 * @brief Pending client telemetry message sent to a local on-box approval daemon.
 *
 * Contract:
 *  - Inputs:
 *    - client_mac: STA MAC as canonical string form "aa:bb:cc:dd:ee:ff"
 *    - bssid: BSSID where the STA associated (same canonical string format)
 *    - agent_al_mac: Agent AL-MAC that reported the event (canonical string format)
 *    - ssid: SSID string if known (may be empty)
 *    - stage: A short stage label ("assoc", "capability_report", ...)
 *    - assoc_frame: Optional association frame (may be empty). If present, should be a printable
 *      representation (e.g. hex dump string) to keep adapter dependency-free.
 *  - Side effects:
 *    - Performs a UDS connect/send/recv on the local host.
 *  - Errors:
 *    - All errors are reported via the returned DecisionResult and logs (throttled for connect failures).
 */
struct PendingClientEvent {
    std::string client_mac;
    std::string bssid;
    std::string agent_al_mac;
    std::string ssid;
    std::string stage;
    std::string assoc_frame;
};

/**
 * @brief Decision response from local on-box approval daemon.
 *
 * Contract:
 *  - daemon_reachable:
 *      true if connect(2) succeeded. false if no daemon is reachable at socket_path.
 *  - has_decision:
 *      true if the reply contained decision=="approve" or decision=="deny".
 *  - approved/denied:
 *      convenience flags (only meaningful when has_decision==true).
 *  - raw_reply:
 *      captured reply (trimmed to receive buffer size).
 *  - error:
 *      best-effort error string for diagnostics.
 */
struct DecisionResult {
    bool daemon_reachable = false;
    bool has_decision     = false;
    bool approved         = false;
    bool denied           = false;

    std::string raw_reply;
    std::string error;
};

class UdsJsonApprovalAdapter {
public:
    // PUBLIC_INTERFACE
    explicit UdsJsonApprovalAdapter(std::string socket_path = default_socket_path())
        : m_socket_path(std::move(socket_path))
    {
    }

    // PUBLIC_INTERFACE
    static std::string default_socket_path()
    {
        /**
         * Default location is intentionally short to stay within sockaddr_un::sun_path limits.
         * This value is meant to be stable and documented for the local daemon integration.
         */
        return "/tmp/prplmesh_onboarding_approval.sock";
    }

    // PUBLIC_INTERFACE
    DecisionResult send_pending_client_event(const PendingClientEvent &event,
                                             int recv_timeout_ms = k_default_timeout_ms) const
    {
        DecisionResult result;

        if (m_socket_path.empty()) {
            result.error = "socket path is empty";
            return result;
        }

        const auto json_line = build_pending_json(event) + "\n";

        int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
        if (fd < 0) {
            result.error = std::string("socket() failed: ") + std::strerror(errno);
            return result;
        }

        // Ensure fd is closed on all exits
        auto fd_guard = [&fd]() {
            if (fd >= 0) {
                ::close(fd);
                fd = -1;
            }
        };

        // Configure short recv timeout so we don't block controller flow.
        struct timeval tv;
        tv.tv_sec  = recv_timeout_ms / 1000;
        tv.tv_usec = (recv_timeout_ms % 1000) * 1000;
        (void)::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        (void)::setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

        sockaddr_un addr;
        std::memset(&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX;
        // strncpy is safe here because we enforce null termination.
        std::strncpy(addr.sun_path, m_socket_path.c_str(), sizeof(addr.sun_path) - 1);

        if (::connect(fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) != 0) {
            // Throttle connect-failure logs to avoid spamming in steady state when daemon is absent.
            static auto last_log = std::chrono::steady_clock::time_point::min();
            const auto now       = std::chrono::steady_clock::now();
            if (now - last_log > std::chrono::seconds(30)) {
                last_log = now;
                LOG(DEBUG) << "OnboardingApprovalAdapter: daemon not reachable at '" << m_socket_path
                           << "', connect() failed: " << std::strerror(errno);
            }

            result.daemon_reachable = false;
            result.error            = std::string("connect() failed: ") + std::strerror(errno);
            fd_guard();
            return result;
        }

        result.daemon_reachable = true;

        ssize_t sent = ::send(fd, json_line.data(), json_line.size(), MSG_NOSIGNAL);
        if (sent < 0) {
            result.error = std::string("send() failed: ") + std::strerror(errno);
            fd_guard();
            return result;
        }

        // Best-effort attempt to read a decision immediately.
        // If there's no data (timeout), we treat it as "reachable but no decision yet".
        char buf[4096];
        std::memset(buf, 0, sizeof(buf));
        ssize_t rcv = ::recv(fd, buf, sizeof(buf) - 1, 0);
        if (rcv <= 0) {
            if (rcv < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                // No decision yet.
                fd_guard();
                return result;
            }
            if (rcv < 0) {
                result.error = std::string("recv() failed: ") + std::strerror(errno);
            }
            fd_guard();
            return result;
        }

        result.raw_reply.assign(buf, static_cast<size_t>(rcv));

        // Parse minimal JSON fields without adding a JSON dependency.
        // Expected daemon reply example:
        //   {"type":"decision","client_mac":"aa:bb:...","decision":"approve"}
        const auto decision = extract_json_string_value(result.raw_reply, "decision");
        if (decision == "approve") {
            result.has_decision = true;
            result.approved     = true;
        } else if (decision == "deny") {
            result.has_decision = true;
            result.denied       = true;
        }

        fd_guard();
        return result;
    }

private:
    static constexpr int k_default_timeout_ms = 50;

    static std::string json_escape(const std::string &in)
    {
        std::ostringstream out;
        for (const char c : in) {
            switch (c) {
            case '\\':
                out << "\\\\";
                break;
            case '"':
                out << "\\\"";
                break;
            case '\n':
                out << "\\n";
                break;
            case '\r':
                out << "\\r";
                break;
            case '\t':
                out << "\\t";
                break;
            default:
                out << c;
                break;
            }
        }
        return out.str();
    }

    static std::string build_pending_json(const PendingClientEvent &e)
    {
        // Intentionally stable wire contract: newline-delimited JSON objects.
        // Use `type=pending` so daemon can dispatch quickly.
        std::ostringstream ss;
        ss << "{";
        ss << "\"type\":\"pending\"";
        ss << ",\"stage\":\"" << json_escape(e.stage) << "\"";
        ss << ",\"client_mac\":\"" << json_escape(e.client_mac) << "\"";
        ss << ",\"bssid\":\"" << json_escape(e.bssid) << "\"";
        ss << ",\"agent_al_mac\":\"" << json_escape(e.agent_al_mac) << "\"";
        ss << ",\"ssid\":\"" << json_escape(e.ssid) << "\"";
        ss << ",\"assoc_frame\":\"" << json_escape(e.assoc_frame) << "\"";
        ss << "}";
        return ss.str();
    }

    static std::string extract_json_string_value(const std::string &json, const std::string &key)
    {
        // Minimal extraction: finds `"key"` then reads the following string value.
        // This is not a general JSON parser; it is sufficient for the small on-box protocol.
        const std::string needle = "\"" + key + "\"";
        auto pos                 = json.find(needle);
        if (pos == std::string::npos) {
            return {};
        }
        pos = json.find(':', pos + needle.size());
        if (pos == std::string::npos) {
            return {};
        }
        // Skip whitespace
        while (pos < json.size() && (json[pos] == ':' || json[pos] == ' ' || json[pos] == '\t')) {
            pos++;
        }
        if (pos >= json.size() || json[pos] != '"') {
            return {};
        }
        pos++; // past opening quote

        std::ostringstream out;
        bool escape = false;
        for (; pos < json.size(); pos++) {
            const char c = json[pos];
            if (escape) {
                // Minimal unescape for common sequences
                switch (c) {
                case '"':
                    out << '"';
                    break;
                case '\\':
                    out << '\\';
                    break;
                case 'n':
                    out << '\n';
                    break;
                case 'r':
                    out << '\r';
                    break;
                case 't':
                    out << '\t';
                    break;
                default:
                    out << c;
                    break;
                }
                escape = false;
                continue;
            }
            if (c == '\\') {
                escape = true;
                continue;
            }
            if (c == '"') {
                break;
            }
            out << c;
        }
        return out.str();
    }

    std::string m_socket_path;
};

} // namespace onboarding_approval
} // namespace son

#endif // PRPLMESH_CONTROLLER_ONBOARDING_APPROVAL_ADAPTER_H_
