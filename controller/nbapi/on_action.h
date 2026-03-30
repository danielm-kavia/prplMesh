/* SPDX-License-Identifier: BSD-2-Clause-Patent
 *
 * SPDX-FileCopyrightText: 2020 the prplMesh contributors (see AUTHORS.md)
 *
 * This code is subject to the terms of the BSD+Patent license.
 * See LICENSE file for more details.
 */

#ifndef ON_ACTION_H
#define ON_ACTION_H

#include "ambiorix_impl.h"

#include "../src/beerocks/master/db/db.h"
#include "../src/beerocks/master/son_actions.h"

namespace prplmesh {
namespace controller {
namespace actions {

std::vector<beerocks::nbapi::sActionsCallback> get_actions_callback_list(void);
std::vector<beerocks::nbapi::sEvents> get_events_list(void);
std::vector<beerocks::nbapi::sFunctions> get_func_list(void);
beerocks::nbapi::ambiorix_func_ptr get_access_point_commit(void);

/**
 * @brief Apply Zero-Touch Configuration payload (JSON text) to NBAPI objects.
 *
 * This function is registered as:
 *   Device.WiFi.DataElements.Network.ApplyZeroTouchConfig(...)
 *
 * The implementation is in controller/nbapi/ztc_ubus.cpp and delegates to the reusable
 * flow in controller/src/beerocks/master/ztc/.
 */
amxd_status_t apply_zero_touch_config(amxd_object_t *object, amxd_function_t *func, amxc_var_t *args,
                                     amxc_var_t *ret);

extern son::db *g_database;

/**
* dwell time (40 milliseconds)
*/
constexpr int PREFERRED_DWELLTIME_MS = 40;

} // namespace actions
} // namespace controller
} // namespace prplmesh
#endif // ON_ACTION_H
