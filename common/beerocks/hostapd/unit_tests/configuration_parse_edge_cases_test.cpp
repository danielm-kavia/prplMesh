/* SPDX-License-Identifier: BSD-2-Clause-Patent
 *
 * SPDX-FileCopyrightText: 2026 the prplMesh contributors
 *
 * This code is subject to the terms of the BSD+Patent license.
 * See LICENSE file for more details.
 */

#include <fstream>
#include <string>
#include <vector>

#include <unistd.h> // close, unlink
#include <stdlib.h> // mkstemp

#include <hostapd/configuration.h>

#include <gtest/gtest.h>

namespace {

std::string create_temp_file_path()
{
    // mkstemp requires XXXXXX suffix and modifies the buffer in-place.
    std::string tmpl = "/tmp/prplmesh_hostapd_edge_XXXXXX";
    std::vector<char> buf(tmpl.begin(), tmpl.end());
    buf.push_back('\0');

    int fd = ::mkstemp(buf.data());
    if (fd == -1) {
        return {};
    }
    ::close(fd);
    return std::string(buf.data());
}

void write_file(const std::string &path, const std::string &content)
{
    std::ofstream out(path);
    ASSERT_TRUE(out.is_open()) << "Failed opening temp config file: " << path;
    out << content;
    out.flush();
    ASSERT_FALSE(out.fail()) << "Failed writing temp config file: " << path;
}

// Suppress cppcheck syntax error for gtest TEST macro
// cppcheck-suppress syntaxError
TEST(configuration_parse_edge_cases_test, load_tolerates_malformed_lines)
{
    const auto path = create_temp_file_path();
    ASSERT_FALSE(path.empty()) << "mkstemp() failed";

    // The current Configuration::load() implementation:
    // - skips whitespace-only lines
    // - does not reject malformed lines (e.g. without '=')
    // This test locks down that behavior and ensures parsing still succeeds when VAP markers exist.
    const std::string content =
        "driver=nl80211\n"
        "this_line_has_no_equals\n"
        "\n"
        "interface=wlan0\n"
        "ssid=front\n"
        "also_bad_line_no_equals\n"
        "bss=wlan0-1\n"
        "ssid=back\n";

    write_file(path, content);

    prplmesh::hostapd::Configuration conf(path);
    ASSERT_FALSE(conf) << conf;

    conf.load("bss=", "interface=");
    ASSERT_TRUE(conf) << conf;

    EXPECT_EQ(conf.get_head_value("driver"), "nl80211");
    EXPECT_EQ(conf.get_vap_value("wlan0", "ssid"), "front");
    EXPECT_EQ(conf.get_vap_value("wlan0-1", "ssid"), "back");

    // Store and reload to ensure the internal representation remains consistent.
    ASSERT_TRUE(conf.store()) << conf;

    prplmesh::hostapd::Configuration conf2(path);
    conf2.load("bss=", "interface=");
    ASSERT_TRUE(conf2) << conf2;

    EXPECT_EQ(conf2.get_head_value("driver"), "nl80211");
    EXPECT_EQ(conf2.get_vap_value("wlan0", "ssid"), "front");
    EXPECT_EQ(conf2.get_vap_value("wlan0-1", "ssid"), "back");

    ::unlink(path.c_str());
}

TEST(configuration_parse_edge_cases_test, duplicate_head_keys_first_occurrence_wins)
{
    const auto path = create_temp_file_path();
    ASSERT_FALSE(path.empty()) << "mkstemp() failed";

    // Duplicate head keys are not deduplicated during load(). get_head_value() returns
    // the first matching entry (std::find_if).
    const std::string content =
        "driver=nl80211\n"
        "dup=first\n"
        "dup=second\n"
        "interface=wlan0\n"
        "ssid=front\n"
        "bss=wlan0-1\n"
        "ssid=back\n";

    write_file(path, content);

    prplmesh::hostapd::Configuration conf(path);
    conf.load("bss=", "interface=");
    ASSERT_TRUE(conf) << conf;

    EXPECT_EQ(conf.get_head_value("dup"), "first");

    // set_create_head_value() removes only the first matching key=value line and appends the new
    // value at the end, therefore the remaining duplicate becomes the "first occurrence".
    ASSERT_TRUE(conf.set_create_head_value("dup", "new")) << conf;
    EXPECT_EQ(conf.get_head_value("dup"), "second");

    ASSERT_TRUE(conf.store()) << conf;

    ::unlink(path.c_str());
}

} // namespace
