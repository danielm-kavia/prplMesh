/* SPDX-License-Identifier: BSD-2-Clause-Patent
 *
 * SPDX-FileCopyrightText: 2026 the prplMesh contributors (see AUTHORS.md)
 *
 * This code is subject to the terms of the BSD+Patent license.
 * See LICENSE file for more details.
 */

#ifndef PRPLMESH_CONTROLLER_ZTC_APPLY_FLOW_H
#define PRPLMESH_CONTROLLER_ZTC_APPLY_FLOW_H

#include <cstdint>
#include <string>
#include <vector>

#include <amxd/amxd_object.h>
#include <bcl/beerocks_logging.h>

#include <ambiorix_impl.h>

namespace prplmesh {
namespace controller {
namespace ztc {

/**
 * @brief Request object for the Zero-Touch Configuration apply flow.
 *
 * Contract:
 * - Inputs:
 *   - payload_json: JSON text matching schema_version "1.0" (see controller/ztc/schema/ztc-payload-v1.schema.json).
 *   - dry_run: If true, validate only; do not write to the datamodel and do not trigger commit.
 *   - force: If true, bypass monotonic revision checks.
 * - Outputs:
 *   - ApplyResult with success/message and optional errors.
 * - Errors:
 *   - Validation errors (schema mismatch, missing/invalid fields) => success=false with errors.
 *   - Apply errors (datamodel set failures, commit failures) => success=false with errors.
 * - Side effects (when dry_run=false):
 *   - Writes allow-listed fields to existing NBAPI objects:
 *     - Device.WiFi.DataElements.Network.AccessPoint.* (+ Security.*)
 *     - X_PRPLWARE-COM_Controller.Configuration.*
 *   - Updates ZeroTouchConfiguration status fields under Device.WiFi.DataElements.Network.ZeroTouchConfiguration
 *   - Optionally triggers AccessPointCommit via an injected callback.
 */
struct ApplyRequest {
    std::string payload_json;
    bool dry_run = false;
    bool force   = false;
};

/**
 * @brief Result object for the Zero-Touch Configuration apply flow.
 */
struct ApplyResult {
    bool success              = false;
    std::string message       = {};
    std::string schema_version = {};
    uint64_t revision         = 0;
    uint32_t applied_access_points = 0;
    std::vector<std::string> errors;
};

/**
 * @brief External dependencies for ZTC apply flow (keeps orchestration reusable/testable).
 */
struct ApplyDependencies {
    beerocks::nbapi::Ambiorix &ambiorix;
    amxd_object_t *network_object = nullptr; // Device.WiFi.DataElements.Network object instance

    // Existing NBAPI function callback used to "commit" AccessPoint configuration to controller DB.
    // This is injected to avoid hard-coupling the flow to a specific module.
    beerocks::nbapi::ambiorix_func_ptr access_point_commit_cb = nullptr;
};

class ZtcApplyFlow {
public:
    /**
     * @brief Validate and (optionally) apply a ZTC JSON payload to NBAPI objects.
     *
     * @param request Apply request.
     * @param deps Dependencies (Ambiorix reference + optional commit callback).
     * @return ApplyResult with a stable, debuggable summary and error list.
     */
    static ApplyResult apply(const ApplyRequest &request, const ApplyDependencies &deps);
};

} // namespace ztc
} // namespace controller
} // namespace prplmesh

#endif // PRPLMESH_CONTROLLER_ZTC_APPLY_FLOW_H
