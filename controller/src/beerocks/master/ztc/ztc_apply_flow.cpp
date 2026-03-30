/* SPDX-License-Identifier: BSD-2-Clause-Patent
 *
 * SPDX-FileCopyrightText: 2026 the prplMesh contributors (see AUTHORS.md)
 *
 * This code is subject to the terms of the BSD+Patent license.
 * See LICENSE file for more details.
 */

#include "ztc_apply_flow.h"

#include <algorithm>
#include <sstream>

#include <json-c/json.h>

#include <bcl/beerocks_defines.h>
#include <easylogging++.h>

namespace prplmesh {
namespace controller {
namespace ztc {

namespace {

constexpr const char *kSupportedSchemaVersion = "1.0";

// Data model paths we write to (existing NBAPI objects + new scaffolding object).
constexpr const char *kZtcStateObjectPath = DATAELEMENTS_ROOT_DM ".Network.ZeroTouchConfiguration";
constexpr const char *kAccessPointRoot    = DATAELEMENTS_ROOT_DM ".Network.AccessPoint";
constexpr const char *kControllerCfgPath  = CONTROLLER_ROOT_DM ".Configuration";

/**
 * @brief Join error strings into a single human-friendly message for ubus return.
 */
static std::string join_errors(const std::vector<std::string> &errors)
{
    std::ostringstream oss;
    for (size_t i = 0; i < errors.size(); i++) {
        if (i) {
            oss << "\n";
        }
        oss << errors[i];
    }
    return oss.str();
}

/**
 * @brief Helper to get a string property from a JSON object with type validation.
 */
static bool json_get_string(json_object *obj, const char *key, std::string &out,
                            bool required, std::vector<std::string> &errors)
{
    json_object *val = nullptr;
    if (!json_object_object_get_ex(obj, key, &val)) {
        if (required) {
            errors.emplace_back(std::string("Missing required field: ") + key);
            return false;
        }
        return true;
    }
    if (!json_object_is_type(val, json_type_string)) {
        errors.emplace_back(std::string("Field '") + key + "' must be a string");
        return false;
    }
    const char *s = json_object_get_string(val);
    out           = (s ? s : "");
    if (required && out.empty()) {
        errors.emplace_back(std::string("Field '") + key + "' must be non-empty");
        return false;
    }
    return true;
}

static bool json_get_bool(json_object *obj, const char *key, bool &out, bool required,
                          std::vector<std::string> &errors)
{
    json_object *val = nullptr;
    if (!json_object_object_get_ex(obj, key, &val)) {
        if (required) {
            errors.emplace_back(std::string("Missing required field: ") + key);
            return false;
        }
        return true;
    }
    if (!json_object_is_type(val, json_type_boolean)) {
        errors.emplace_back(std::string("Field '") + key + "' must be a boolean");
        return false;
    }
    out = json_object_get_boolean(val);
    return true;
}

static bool json_get_uint64(json_object *obj, const char *key, uint64_t &out, bool required,
                            std::vector<std::string> &errors)
{
    json_object *val = nullptr;
    if (!json_object_object_get_ex(obj, key, &val)) {
        if (required) {
            errors.emplace_back(std::string("Missing required field: ") + key);
            return false;
        }
        return true;
    }
    if (!json_object_is_type(val, json_type_int)) {
        errors.emplace_back(std::string("Field '") + key + "' must be an integer");
        return false;
    }
    int64_t v = json_object_get_int64(val);
    if (v < 0) {
        errors.emplace_back(std::string("Field '") + key + "' must be >= 0");
        return false;
    }
    out = static_cast<uint64_t>(v);
    return true;
}

static bool set_ztc_status(beerocks::nbapi::Ambiorix &ambiorix, const std::string &status,
                           const std::string &schema_version, uint64_t revision,
                           const std::string &last_error)
{
    bool ok = true;
    ok &= ambiorix.set(kZtcStateObjectPath, "LastApplyStatus", status);
    ok &= ambiorix.set(kZtcStateObjectPath, "LastAppliedSchemaVersion", schema_version);
    ok &= ambiorix.set(kZtcStateObjectPath, "LastAppliedRevision", revision);
    ok &= ambiorix.set(kZtcStateObjectPath, "LastError", last_error);

    // This writes RFC3339 string into datetime parameter.
    ok &= ambiorix.set_current_time(kZtcStateObjectPath, "LastAppliedTime");
    return ok;
}

static bool parse_bands_and_apply(beerocks::nbapi::Ambiorix &ambiorix, const std::string &ap_path,
                                  json_object *bands_array, std::vector<std::string> &errors)
{
    if (!bands_array || !json_object_is_type(bands_array, json_type_array)) {
        errors.emplace_back("wifi.access_points[].bands must be an array of strings");
        return false;
    }
    const int len = json_object_array_length(bands_array);
    if (len <= 0) {
        errors.emplace_back("wifi.access_points[].bands must contain at least one band");
        return false;
    }

    bool band24 = false, band5gl = false, band5gh = false, band6g = false;

    for (int i = 0; i < len; i++) {
        json_object *item = json_object_array_get_idx(bands_array, i);
        if (!item || !json_object_is_type(item, json_type_string)) {
            errors.emplace_back("wifi.access_points[].bands items must be strings");
            return false;
        }
        const std::string band = json_object_get_string(item);
        if (band == "2.4G") {
            band24 = true;
        } else if (band == "5GL") {
            band5gl = true;
        } else if (band == "5GH") {
            band5gh = true;
        } else if (band == "6G") {
            band6g = true;
        } else {
            errors.emplace_back(std::string("Unsupported band value: '") + band + "'");
            return false;
        }
    }

    // Apply to NBAPI AccessPoint object.
    bool ok = true;
    ok &= ambiorix.set(ap_path, "Band2_4G", band24);
    ok &= ambiorix.set(ap_path, "Band5GL", band5gl);
    ok &= ambiorix.set(ap_path, "Band5GH", band5gh);
    ok &= ambiorix.set(ap_path, "Band6G", band6g);

    if (!ok) {
        errors.emplace_back("Failed to write AccessPoint band flags to datamodel");
    }
    return ok;
}

static bool apply_access_point(beerocks::nbapi::Ambiorix &ambiorix, json_object *ap_obj,
                               std::vector<std::string> &errors)
{
    // Validate required fields.
    std::string ssid;
    if (!json_get_string(ap_obj, "ssid", ssid, true, errors)) {
        return false;
    }

    json_object *bands = nullptr;
    if (!json_object_object_get_ex(ap_obj, "bands", &bands)) {
        errors.emplace_back("Missing required field: bands");
        return false;
    }

    json_object *security = nullptr;
    if (!json_object_object_get_ex(ap_obj, "security", &security) ||
        !json_object_is_type(security, json_type_object)) {
        errors.emplace_back("wifi.access_points[].security must be an object");
        return false;
    }

    std::string mode_enabled;
    if (!json_get_string(security, "mode_enabled", mode_enabled, true, errors)) {
        return false;
    }

    // Optional fields.
    bool enable = true;
    (void)json_get_bool(ap_obj, "enable", enable, false, errors);

    std::string vap_type = "other";
    (void)json_get_string(ap_obj, "vap_type", vap_type, false, errors);

    std::string multi_ap_mode = "Fronthaul";
    (void)json_get_string(ap_obj, "multi_ap_mode", multi_ap_mode, false, errors);

    // Credentials fields (do not log actual values).
    std::string key_passphrase;
    std::string pre_shared_key;
    std::string sae_passphrase;

    (void)json_get_string(security, "key_passphrase", key_passphrase, false, errors);
    (void)json_get_string(security, "pre_shared_key", pre_shared_key, false, errors);
    (void)json_get_string(security, "sae_passphrase", sae_passphrase, false, errors);

    const bool needs_secret = (mode_enabled != "None");
    if (needs_secret && key_passphrase.empty() && pre_shared_key.empty() && sae_passphrase.empty()) {
        errors.emplace_back("security.mode_enabled requires a passphrase (key_passphrase/pre_shared_key/sae_passphrase)");
        return false;
    }

    auto mask_secret_present = [](const std::string &s) { return s.empty() ? "absent" : "present"; };

    LOG(INFO) << "[ZTC] Applying AccessPoint ssid='" << ssid << "'"
              << " enable=" << (enable ? "true" : "false")
              << " mode_enabled='" << mode_enabled << "'"
              << " key_passphrase=" << mask_secret_present(key_passphrase)
              << " pre_shared_key=" << mask_secret_present(pre_shared_key)
              << " sae_passphrase=" << mask_secret_present(sae_passphrase);

    // Create a new AccessPoint instance.
    const std::string ap_path = ambiorix.add_instance(kAccessPointRoot);
    if (ap_path.empty()) {
        errors.emplace_back("Failed to create NBAPI AccessPoint instance (add_instance returned empty)");
        return false;
    }

    bool ok = true;

    ok &= ambiorix.set(ap_path, "SSID", ssid);
    ok &= ambiorix.set(ap_path, "Enable", enable);
    ok &= ambiorix.set(ap_path, "X_PRPLWARE_VapType", vap_type);
    ok &= ambiorix.set(ap_path, "MultiApMode", multi_ap_mode);

    if (!parse_bands_and_apply(ambiorix, ap_path, bands, errors)) {
        return false;
    }

    // Apply security.
    const std::string sec_path = ap_path + ".Security";
    ok &= ambiorix.set(sec_path, "ModeEnabled", mode_enabled);

    // Allowlist: we set all three secrets if present; otherwise we fall back to KeyPassphrase.
    // Note: This is intentionally simple scaffolding. It does not attempt advanced mode-specific
    // constraints beyond minimal requirements.
    if (!pre_shared_key.empty()) {
        ok &= ambiorix.set(sec_path, "PreSharedKey", pre_shared_key);
    }
    if (!sae_passphrase.empty()) {
        ok &= ambiorix.set(sec_path, "SAEPassphrase", sae_passphrase);
    }
    if (!key_passphrase.empty()) {
        ok &= ambiorix.set(sec_path, "KeyPassphrase", key_passphrase);
    }

    if (!ok) {
        errors.emplace_back("Failed to write AccessPoint fields to datamodel");
        return false;
    }

    return true;
}

static bool apply_controller_configuration(beerocks::nbapi::Ambiorix &ambiorix,
                                          json_object *controller_cfg_obj,
                                          std::vector<std::string> &errors)
{
    if (!controller_cfg_obj || !json_object_is_type(controller_cfg_obj, json_type_object)) {
        errors.emplace_back("controller.configuration must be an object");
        return false;
    }

    // Allow-list mapping (minimal scaffolding).
    // IMPORTANT: Unknown keys are rejected to prevent the provisioning entrypoint becoming a generic backdoor.
    struct KeySpec {
        const char *key;
        enum class Type { BOOL, UINT32 } type;
    };

    const KeySpec allowlist[] = {
        {"DaisyChainingDisabled", KeySpec::Type::BOOL},
        {"ChannelSelectionTaskEnabled", KeySpec::Type::BOOL},
        {"DynamicChannelSelectionTaskEnabled", KeySpec::Type::BOOL},
        {"BackhaulOptimizationEnabled", KeySpec::Type::BOOL},
        {"LoadBalancingTaskEnabled", KeySpec::Type::BOOL},
        {"OptimalPathPreferSignalStrength", KeySpec::Type::BOOL},
        {"HealthCheckTaskEnabled", KeySpec::Type::BOOL},
        {"StatisticsPollingTaskEnabled", KeySpec::Type::BOOL},
        {"StatisticsPollingRateSec", KeySpec::Type::UINT32},
        {"LinkMetricsRequestIntervalSec", KeySpec::Type::UINT32},
    };

    // Reject unknown keys.
    json_object_object_foreach(controller_cfg_obj, key, val)
    {
        (void)val;
        const bool known = std::any_of(std::begin(allowlist), std::end(allowlist),
                                       [&](const KeySpec &k) { return std::string(k.key) == key; });
        if (!known) {
            errors.emplace_back(std::string("Unsupported controller.configuration key: '") + key + "'");
        }
    }
    if (!errors.empty()) {
        return false;
    }

    bool ok = true;

    for (const auto &spec : allowlist) {
        json_object *val = nullptr;
        if (!json_object_object_get_ex(controller_cfg_obj, spec.key, &val)) {
            continue; // optional
        }

        if (spec.type == KeySpec::Type::BOOL) {
            if (!json_object_is_type(val, json_type_boolean)) {
                errors.emplace_back(std::string("controller.configuration.") + spec.key + " must be a boolean");
                ok = false;
                continue;
            }
            const bool b = json_object_get_boolean(val);
            LOG(INFO) << "[ZTC] Setting controller config " << spec.key << "=" << (b ? "true" : "false");
            ok &= ambiorix.set(kControllerCfgPath, spec.key, b);
        } else {
            if (!json_object_is_type(val, json_type_int)) {
                errors.emplace_back(std::string("controller.configuration.") + spec.key + " must be an integer");
                ok = false;
                continue;
            }
            const int64_t i64 = json_object_get_int64(val);
            if (i64 < 0 || i64 > std::numeric_limits<uint32_t>::max()) {
                errors.emplace_back(std::string("controller.configuration.") + spec.key + " out of range");
                ok = false;
                continue;
            }
            const uint32_t u32 = static_cast<uint32_t>(i64);
            LOG(INFO) << "[ZTC] Setting controller config " << spec.key << "=" << u32;
            ok &= ambiorix.set(kControllerCfgPath, spec.key, u32);
        }
    }

    if (!ok && errors.empty()) {
        errors.emplace_back("Failed writing controller.configuration fields to datamodel");
    }
    return ok;
}

} // namespace

ApplyResult ZtcApplyFlow::apply(const ApplyRequest &request, const ApplyDependencies &deps)
{
    ApplyResult result;
    result.success = false;

    std::vector<std::string> errors;

    if (request.payload_json.empty()) {
        errors.emplace_back("payload_json is empty");
        result.errors  = errors;
        result.message = "Validation failed";
        return result;
    }

    json_object *root = json_tokener_parse(request.payload_json.c_str());
    if (!root) {
        errors.emplace_back("Invalid JSON: json_tokener_parse returned null");
        result.errors  = errors;
        result.message = "Validation failed";
        return result;
    }

    if (!json_object_is_type(root, json_type_object)) {
        errors.emplace_back("Payload root must be a JSON object");
        json_object_put(root);
        result.errors  = errors;
        result.message = "Validation failed";
        return result;
    }

    std::string schema_version;
    (void)json_get_string(root, "schema_version", schema_version, true, errors);
    result.schema_version = schema_version;

    if (schema_version != kSupportedSchemaVersion) {
        errors.emplace_back(std::string("Unsupported schema_version: '") + schema_version + "'");
    }

    uint64_t revision = 0;
    (void)json_get_uint64(root, "revision", revision, false, errors);
    result.revision = revision;

    // Monotonic revision check (only if revision > 0).
    if (revision > 0 && !request.force) {
        uint64_t last_rev = 0;
        if (!deps.ambiorix.read_param(kZtcStateObjectPath, "LastAppliedRevision", &last_rev)) {
            // If state object doesn't exist / not readable, do not hard fail (scaffolding behavior),
            // but log for debuggability.
            LOG(WARNING) << "[ZTC] Could not read " << kZtcStateObjectPath
                         << ".LastAppliedRevision. Proceeding without monotonic check.";
        } else if (revision <= last_rev) {
            errors.emplace_back("Revision is not monotonic: revision=" + std::to_string(revision) +
                                " last_applied_revision=" + std::to_string(last_rev) +
                                " (use force=true to override)");
        }
    }

    // wifi section
    json_object *wifi_obj = nullptr;
    json_object *aps      = nullptr;
    bool replace_aps      = false;

    if (json_object_object_get_ex(root, "wifi", &wifi_obj)) {
        if (!json_object_is_type(wifi_obj, json_type_object)) {
            errors.emplace_back("wifi must be an object");
        } else {
            json_object *replace_val = nullptr;
            if (json_object_object_get_ex(wifi_obj, "replace_access_points", &replace_val)) {
                if (!json_object_is_type(replace_val, json_type_boolean)) {
                    errors.emplace_back("wifi.replace_access_points must be a boolean");
                } else {
                    replace_aps = json_object_get_boolean(replace_val);
                }
            }

            if (json_object_object_get_ex(wifi_obj, "access_points", &aps)) {
                if (!json_object_is_type(aps, json_type_array)) {
                    errors.emplace_back("wifi.access_points must be an array");
                }
            }
        }
    }

    // controller section
    json_object *controller_obj = nullptr;
    json_object *controller_cfg = nullptr;
    if (json_object_object_get_ex(root, "controller", &controller_obj)) {
        if (!json_object_is_type(controller_obj, json_type_object)) {
            errors.emplace_back("controller must be an object");
        } else {
            if (json_object_object_get_ex(controller_obj, "configuration", &controller_cfg)) {
                if (!json_object_is_type(controller_cfg, json_type_object)) {
                    errors.emplace_back("controller.configuration must be an object");
                }
            }
        }
    }

    if (!errors.empty()) {
        json_object_put(root);
        result.errors  = errors;
        result.message = "Validation failed";
        return result;
    }

    // Dry-run ends after validation.
    if (request.dry_run) {
        json_object_put(root);
        result.success = true;
        result.message = "Dry-run: payload validated successfully (no changes applied)";
        return result;
    }

    LOG(INFO) << "[ZTC] Apply flow start"
              << " schema_version=" << result.schema_version
              << " revision=" << result.revision
              << " replace_access_points=" << (replace_aps ? "true" : "false")
              << " force=" << (request.force ? "true" : "false");

    // Update state: Applying (best-effort).
    (void)set_ztc_status(deps.ambiorix, "Applying", result.schema_version, result.revision, "");

    // Apply controller configuration first (lower risk).
    if (controller_cfg) {
        if (!apply_controller_configuration(deps.ambiorix, controller_cfg, errors)) {
            // fallthrough to failure handling
        }
    }

    // Apply Wi-Fi access points (DataElements surface).
    if (aps && errors.empty()) {
        if (replace_aps) {
            LOG(INFO) << "[ZTC] Removing all existing AccessPoint instances under " << kAccessPointRoot;
            if (!deps.ambiorix.remove_all_instances(kAccessPointRoot)) {
                errors.emplace_back("Failed to remove existing AccessPoint instances");
            }
        }

        const int ap_count = json_object_array_length(aps);
        for (int i = 0; i < ap_count && errors.empty(); i++) {
            json_object *ap = json_object_array_get_idx(aps, i);
            if (!ap || !json_object_is_type(ap, json_type_object)) {
                errors.emplace_back("wifi.access_points[] items must be objects");
                break;
            }
            if (!apply_access_point(deps.ambiorix, ap, errors)) {
                break;
            }
            result.applied_access_points++;
        }
    }

    // Trigger AccessPointCommit to translate AccessPoint objects into controller DB config + renew messages.
    if (errors.empty() && result.applied_access_points > 0) {
        if (deps.access_point_commit_cb && deps.network_object) {
            LOG(INFO) << "[ZTC] Triggering existing AccessPointCommit handler after applying AccessPoint objects";
            amxc_var_t commit_ret;
            amxc_var_init(&commit_ret);

            // access_point_commit ignores args/func; ret must be valid (it calls amxc_var_clean()).
            const amxd_status_t st =
                deps.access_point_commit_cb(deps.network_object, nullptr, nullptr, &commit_ret);

            amxc_var_clean(&commit_ret);

            if (st != amxd_status_ok) {
                errors.emplace_back("AccessPointCommit failed (amxd_status=" + std::to_string(st) + ")");
            }
        } else {
            LOG(WARNING) << "[ZTC] AccessPointCommit callback/network object not available; "
                            "AccessPoint objects were written but not committed into controller DB.";
        }
    }

    json_object_put(root);

    if (!errors.empty()) {
        const std::string err_joined = join_errors(errors);
        LOG(ERROR) << "[ZTC] Apply flow failed:\n" << err_joined;

        // Best-effort: write failure status (do not include secrets).
        (void)set_ztc_status(deps.ambiorix, "Failed", result.schema_version, result.revision, err_joined);

        result.success = false;
        result.errors  = errors;
        result.message = "Apply failed";
        return result;
    }

    // Best-effort: persist success state.
    (void)set_ztc_status(deps.ambiorix, "Success", result.schema_version, result.revision, "");

    LOG(INFO) << "[ZTC] Apply flow success"
              << " revision=" << result.revision
              << " applied_access_points=" << result.applied_access_points;

    result.success = true;
    result.message = "Apply succeeded";
    return result;
}

} // namespace ztc
} // namespace controller
} // namespace prplmesh
