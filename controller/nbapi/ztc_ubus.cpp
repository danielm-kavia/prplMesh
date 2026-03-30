/* SPDX-License-Identifier: BSD-2-Clause-Patent
 *
 * SPDX-FileCopyrightText: 2026 the prplMesh contributors (see AUTHORS.md)
 *
 * This code is subject to the terms of the BSD+Patent license.
 * See LICENSE file for more details.
 */

#include "on_action.h"

#include <sstream>

#include <easylogging++.h>

#include "ztc/ztc_apply_flow.h"

namespace prplmesh {
namespace controller {
namespace actions {

static std::string join_lines(const std::vector<std::string> &lines)
{
    std::ostringstream oss;
    for (size_t i = 0; i < lines.size(); i++) {
        if (i) {
            oss << "\n";
        }
        oss << lines[i];
    }
    return oss.str();
}

/**
 * @brief Apply Zero-Touch Configuration payload (JSON text) to existing NBAPI objects.
 *
 * ubus usage (example):
 *   ubus call Device.WiFi.DataElements.Network ApplyZeroTouchConfig \
 *     '{ "payload_json": "{...}", "dry_run": false, "force": false }'
 *
 * Notes:
 * - payload_json is validated and then mapped only onto allow-listed NBAPI objects.
 * - Secrets (passphrases) are never logged.
 * - Return value is a hashtable with: success, message, schema_version, revision, applied_access_points, errors (optional).
 */
amxd_status_t apply_zero_touch_config(amxd_object_t *object, amxd_function_t *func, amxc_var_t *args,
                                     amxc_var_t *ret)
{
    (void)func;

    amxc_var_clean(ret);
    amxc_var_set_type(ret, AMXC_VAR_ID_HTABLE);

    if (!g_database) {
        LOG(ERROR) << "[ZTC] g_database is null";
        amxc_var_add_key(bool, ret, "success", false);
        amxc_var_add_key(cstring_t, ret, "message", "Internal error: database not initialized");
        return amxd_status_unknown_error;
    }

    auto ambiorix_sp = g_database->get_ambiorix_obj();
    if (!ambiorix_sp) {
        LOG(ERROR) << "[ZTC] Ambiorix object is null";
        amxc_var_add_key(bool, ret, "success", false);
        amxc_var_add_key(cstring_t, ret, "message", "Internal error: ambiorix not available");
        return amxd_status_unknown_error;
    }

    const char *payload_json_c = GET_CHAR(args, "payload_json");
    if (!payload_json_c) {
        amxc_var_add_key(bool, ret, "success", false);
        amxc_var_add_key(cstring_t, ret, "message", "Missing mandatory argument: payload_json");
        return amxd_status_parameter_not_found;
    }

    prplmesh::controller::ztc::ApplyRequest req;
    req.payload_json = payload_json_c;
    req.dry_run      = GET_BOOL(args, "dry_run");
    req.force        = GET_BOOL(args, "force");

    prplmesh::controller::ztc::ApplyDependencies deps{*ambiorix_sp, object, get_access_point_commit()};

    auto res = prplmesh::controller::ztc::ZtcApplyFlow::apply(req, deps);

    amxc_var_add_key(bool, ret, "success", res.success);
    amxc_var_add_key(cstring_t, ret, "message", res.message.c_str());
    amxc_var_add_key(cstring_t, ret, "schema_version", res.schema_version.c_str());
    amxc_var_add_key(uint64_t, ret, "revision", res.revision);
    amxc_var_add_key(uint32_t, ret, "applied_access_points", res.applied_access_points);

    if (!res.errors.empty()) {
        const auto err_str = join_lines(res.errors);
        amxc_var_add_key(cstring_t, ret, "errors", err_str.c_str());
    }

    return res.success ? amxd_status_ok : amxd_status_invalid_value;
}

} // namespace actions
} // namespace controller
} // namespace prplmesh
