/* SPDX-License-Identifier: BSD-2-Clause-Patent
 *
 * SPDX-FileCopyrightText: 2026 the prplMesh contributors
 *
 * This code is subject to the terms of the BSD+Patent license.
 * See LICENSE file for more details.
 */

#include <bwl/base_wlan_hal.h>

#include <gtest/gtest.h>

#include <atomic>
#include <list>
#include <string>

namespace {

/**
 * @brief Minimal concrete HAL implementation for unit-testing non-platform-specific logic in
 * bwl::base_wlan_hal (internal event queue handling + small helper methods).
 */
class TestWlanHal final : public bwl::base_wlan_hal {
public:
    explicit TestWlanHal(hal_event_cb_t cb, const bwl::hal_conf_t &conf = {})
        : bwl::base_wlan_hal(bwl::HALType::Monitor, "test0", bwl::IfaceType::Intel, cb, conf)
    {
    }

    // Stubs for abstract interface - not used by these tests.
    bwl::HALState attach(bool /*block*/ = false) override { return bwl::HALState::Operational; }
    bool detach() override { return true; }
    bool refresh_radio_info() override { return true; }
    bool ping() override { return true; }
    bool reassociate() override { return true; }
    bool refresh_vaps_info(int /*id*/ = beerocks::IFACE_RADIO_ID) override { return true; }
    bool get_vap_status(
        const std::list<son::wireless_utils::sBssInfoConf> & /*bss_info_conf_list*/) override
    {
        return true;
    }
    bool update_mld_status(
        const std::list<son::wireless_utils::sBssInfoConf> & /*bss_info_conf_list*/) override
    {
        return true;
    }
    bool process_ext_events(int /*fd*/ = 0) override { return true; }
    bool process_nl_events() override { return true; }
    bool get_channel_utilization(uint8_t &channel_utilization) override
    {
        channel_utilization = 0;
        return true;
    }
    std::string get_radio_mac() override { return {}; }
    sMacAddr get_bsta_mld_mac() override { return {}; }
    sMacAddr get_ap_mld_mac() override { return {}; }

    // Test helpers exposing protected base functionality/state.
    bool push_event(int opcode) { return event_queue_push(opcode, nullptr); }

    bool is_bss_monitored_public(const std::string &bssid) { return is_BSS_monitored(bssid); }

    void set_filtered_events(std::set<std::string> events) { m_filtered_events = std::move(events); }

protected:
    bool set(const std::string & /*param*/, const std::string & /*value*/, int /*vap_id*/) override
    {
        return true;
    }
};

// Suppress cppcheck syntax error for gtest TEST macro
// cppcheck-suppress syntaxError
TEST(base_wlan_hal_internal_events_test, process_int_events_is_capped_per_iteration)
{
    std::atomic<int> cb_count{0};

    auto cb = [&cb_count](bwl::base_wlan_hal::hal_event_ptr_t /*evt*/) -> bool {
        ++cb_count;
        return true;
    };

    TestWlanHal hal(cb);

    // Verify MAX_EVENTS_PER_ITERATION guardrail (defined in base_wlan_hal.cpp).
    constexpr int events_to_push = 300;
    for (int i = 0; i < events_to_push; ++i) {
        ASSERT_TRUE(hal.push_event(i));
    }

    ASSERT_TRUE(hal.process_int_events());
    EXPECT_EQ(cb_count.load(), 250) << "Expected processing to stop at MAX_EVENTS_PER_ITERATION";

    ASSERT_TRUE(hal.process_int_events());
    EXPECT_EQ(cb_count.load(), events_to_push);

    // When the queue is empty, process_int_events() returns true (with a warning).
    ASSERT_TRUE(hal.process_int_events());
    EXPECT_EQ(cb_count.load(), events_to_push);
}

TEST(base_wlan_hal_internal_events_test, helper_semantics_are_stable)
{
    TestWlanHal hal([](bwl::base_wlan_hal::hal_event_ptr_t /*evt*/) { return true; });

    // radio_state_from_string() is case-sensitive and maps unknown strings to UNKNOWN.
    EXPECT_EQ(hal.radio_state_from_string("ACS"), bwl::eRadioState::ACS);
    EXPECT_EQ(hal.radio_state_from_string("acs"), bwl::eRadioState::UNKNOWN);
    EXPECT_EQ(hal.radio_state_from_string("NOT_A_STATE"), bwl::eRadioState::UNKNOWN);

    // is_filtered_event(): empty filter set => returns true (meaning "pass" in current semantics).
    EXPECT_TRUE(hal.is_filtered_event("ANY_OPCODE"));

    hal.set_filtered_events({"OP1", "OP2"});
    EXPECT_TRUE(hal.is_filtered_event("OP1"));
    EXPECT_FALSE(hal.is_filtered_event("OP3"));

    // is_BSS_monitored(): empty monitored list => monitor all.
    EXPECT_TRUE(hal.is_bss_monitored_public("11:22:33:44:55:66"));

    bwl::hal_conf_t conf;
    conf.monitored_BSSs = {"aa:bb:cc:dd:ee:ff"};
    TestWlanHal hal2([](bwl::base_wlan_hal::hal_event_ptr_t /*evt*/) { return true; }, conf);
    EXPECT_TRUE(hal2.is_bss_monitored_public("aa:bb:cc:dd:ee:ff"));
    EXPECT_FALSE(hal2.is_bss_monitored_public("11:22:33:44:55:66"));
}

} // namespace
