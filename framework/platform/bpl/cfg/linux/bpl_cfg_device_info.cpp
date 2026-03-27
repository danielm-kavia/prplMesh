/* SPDX-License-Identifier: BSD-2-Clause-Patent
 *
 * SPDX-FileCopyrightText: 2026 the prplMesh contributors (see AUTHORS.md)
 *
 * This code is subject to the terms of the BSD+Patent license.
 * See LICENSE file for more details.
 */

#include <bcl/beerocks_version.h>
#include <bpl/bpl_cfg.h>

#include <fstream>
#include <string>

namespace beerocks {
namespace bpl {

namespace {

/**
 * @brief Read a key from /etc/os-release.
 *
 * This is a best-effort helper meant for native Linux builds where WHM/NBAPI (Ambiorix-based)
 * device-info retrieval is not available.
 */
static bool read_os_release_value(const std::string &key, std::string &value)
{
    std::ifstream file("/etc/os-release");
    if (!file.is_open()) {
        return false;
    }

    std::string line;
    while (std::getline(file, line)) {
        // Expect KEY=VALUE (VALUE might be quoted)
        if (line.rfind(key + "=", 0) != 0) {
            continue;
        }

        value = line.substr(key.size() + 1);

        // Strip optional quotes.
        if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
            value = value.substr(1, value.size() - 2);
        }
        return !value.empty();
    }

    return false;
}

} // namespace

bool get_serial_number(std::string &serial_number)
{
    // No stable serial number source for generic Linux desktops.
    // Keep behavior aligned with existing DM implementation: always succeed with a fallback.
    serial_number.assign("prplmesh12345");
    return true;
}

bool get_software_version(std::string &software_version)
{
    // Prefer prplMesh module version.
    software_version.assign(beerocks::version::get_module_version());
    if (software_version.empty()) {
        software_version.assign("unknown");
    }
    return true;
}

bool get_manufacturer(std::string &manufacturer)
{
    manufacturer.assign("prplMesh");
    return true;
}

bool get_model_name(std::string &model_name)
{
    // Provide a meaningful-but-generic model name for Linux hosts.
    if (!read_os_release_value("NAME", model_name)) {
        model_name.assign("Linux");
    }
    return true;
}

bool get_model_number(std::string &model_number)
{
    // Provide a meaningful-but-generic model "number" for Linux hosts.
    if (!read_os_release_value("VERSION_ID", model_number)) {
        model_number.assign("0");
    }
    return true;
}

bool get_ruid_chipset_vendor(const sMacAddr &ruid, std::string &chipset_vendor)
{
    (void)ruid;
    // No generic way to map radio UID -> chipset vendor on a host build.
    chipset_vendor.assign("unknown");
    return true;
}

bool get_max_prioritization_rules(uint32_t &max_prioritization_rules)
{
    // Keep aligned with DM implementation; see EasyMesh standard requirement notes there.
    max_prioritization_rules = 1;
    return true;
}

} // namespace bpl
} // namespace beerocks
