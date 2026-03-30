/* SPDX-License-Identifier: BSD-2-Clause-Patent
 *
 * SPDX-FileCopyrightText: 2021 the prplMesh contributors (see AUTHORS.md)
 *
 * This code is subject to the terms of the BSD+Patent license.
 * See LICENSE file for more details.
 */

#include "client_association_task.h"
#include "../son_actions.h"
#include "../onboarding/onboarding_approval_adapter.h"

#include <bcl/beerocks_utils.h>
#include <bcl/beerocks_wifi_channel.h>
#include <bcl/son/son_assoc_frame_utils.h>
#include <tlvf/wfa_map/tlvClientAssociationEvent.h>
#include <tlvf/wfa_map/tlvClientCapabilityReport.h>
#include <tlvf/wfa_map/tlvClientInfo.h>

using namespace beerocks;
using namespace net;
using namespace son;

namespace {

// Pending gating defaults (Option A, local daemon integration).
// - pending block is applied only when daemon is reachable (to avoid lockouts when daemon is absent).
// - deny block is stronger/longer to enforce explicit denial.
constexpr int k_pending_block_sec = 60 * 60;      // 1 hour
constexpr int k_deny_block_sec    = 60 * 60 * 24; // 24 hours

// A single adapter instance per process.
// Socket path is the default stable contract (see adapter header).
onboarding_approval::UdsJsonApprovalAdapter &approval_adapter()
{
    static onboarding_approval::UdsJsonApprovalAdapter adapter;
    return adapter;
}

void apply_onboarding_decision(db &database, ieee1905_1::CmduMessageTx &cmdu_tx,
                               const std::string &sta_mac_str, const std::string &bssid_str,
                               const onboarding_approval::DecisionResult &decision)
{
    if (!decision.has_decision) {
        return;
    }

    if (decision.approved) {
        LOG(INFO) << "Onboarding approval: APPROVE for STA " << sta_mac_str
                  << " (bssid=" << bssid_str << ")";
        son_actions::unblock_sta(database, cmdu_tx, sta_mac_str);
        return;
    }

    if (decision.denied) {
        LOG(INFO) << "Onboarding approval: DENY for STA " << sta_mac_str << " (bssid=" << bssid_str
                  << "), applying block + disconnect";
        son_actions::block_sta(database, cmdu_tx, sta_mac_str, bssid_str, k_deny_block_sec);

        // Enforce immediately if currently connected to this BSSID.
        // This reuses existing disconnect enforcement flow.
        son_actions::disconnect_client(database, cmdu_tx, sta_mac_str, bssid_str,
                                       eDisconnect_Type_Disassoc, 0);
        return;
    }
}

} // namespace

client_association_task::client_association_task(db &database_, ieee1905_1::CmduMessageTx &cmdu_tx_,
                                                 task_pool &tasks_, const std::string &task_name_)
    : task(task_name_), m_database(database_), m_cmdu_tx(cmdu_tx_), m_tasks(tasks_)
{
}

void client_association_task ::work() {}

bool client_association_task::handle_ieee1905_1_msg(const sMacAddr &src_mac,
                                                    ieee1905_1::CmduMessageRx &cmdu_rx)
{
    switch (cmdu_rx.getMessageType()) {
    case ieee1905_1::eMessageType::TOPOLOGY_NOTIFICATION_MESSAGE: {
        verify_sta_association(src_mac, cmdu_rx);
        break;
    }
    case ieee1905_1::eMessageType::CLIENT_CAPABILITY_REPORT_MESSAGE: {
        return handle_cmdu_1905_client_capability_report_message(src_mac, cmdu_rx);
    }
    default: {
        return false;
    }
    }
    return true;
}

bool client_association_task::verify_sta_association(const sMacAddr &src_mac,
                                                     ieee1905_1::CmduMessageRx &cmdu_rx)
{
    auto sta_assoc_tlv = cmdu_rx.getClass<wfa_map::tlvClientAssociationEvent>();

    if (!sta_assoc_tlv) {
        return false;
    }

    auto ambiorix_dm = m_database.get_ambiorix_obj();

    if (!ambiorix_dm) {
        LOG(ERROR) << "Failed to get Ambiorix datamodel";
        return false;
    }

    auto station = m_database.get_station(sta_assoc_tlv->client_mac());

    if (!station) {
        LOG(ERROR) << "station " << sta_assoc_tlv->client_mac() << " not found";
        return false;
    }

    if (sta_assoc_tlv->association_event() ==
        wfa_map::tlvClientAssociationEvent::eAssociationEvent::CLIENT_HAS_JOINED_THE_BSS) {
        station->assoc_timestamp = ambiorix_dm->get_datamodel_time_format();
        dm_add_sta_association_event(sta_assoc_tlv->client_mac(), sta_assoc_tlv->bssid());

        // Option A integration point:
        // Emit a "pending" event to a local on-box daemon and apply approve/deny enforcement.
        const auto sta_mac_str   = tlvf::mac_to_string(sta_assoc_tlv->client_mac());
        const auto bssid_str     = tlvf::mac_to_string(sta_assoc_tlv->bssid());
        const auto agent_al_str  = tlvf::mac_to_string(src_mac);
        const auto ssid          = m_database.get_bss_ssid(sta_assoc_tlv->bssid());
        const std::string stage  = "assoc";
        const std::string frame  = ""; // not available yet at this stage

        onboarding_approval::PendingClientEvent pending;
        pending.client_mac   = sta_mac_str;
        pending.bssid        = bssid_str;
        pending.agent_al_mac = agent_al_str;
        pending.ssid         = ssid;
        pending.stage        = stage;
        pending.assoc_frame  = frame;

        auto decision = approval_adapter().send_pending_client_event(pending);

        // Apply pending gating only when daemon is reachable (fail-open when absent).
        if (decision.daemon_reachable) {
            son_actions::block_sta(m_database, m_cmdu_tx, sta_mac_str, bssid_str, k_pending_block_sec);
        }

        apply_onboarding_decision(m_database, m_cmdu_tx, sta_mac_str, bssid_str, decision);

        /*
         * Even though client capabilities exist in a legacy vendor
         * specific TLV, send a client capability query anyway to
         * treat prplMesh and non-prplMesh agents the same way. This
         * also allows us to get the association frame from the
         * client capability response.
         */
        if (!send_sta_capability_query(src_mac, cmdu_rx)) {
            LOG(ERROR) << "Failed to send Client Capability Query.";
            return false;
        }
        return true;
    }
    return false;
}

bool client_association_task::send_sta_capability_query(const sMacAddr &src_mac,
                                                        ieee1905_1::CmduMessageRx &cmdu_rx)
{
    auto client_association_event_tlv = cmdu_rx.getClass<wfa_map::tlvClientAssociationEvent>();

    if (!client_association_event_tlv) {
        return false;
    }
    if (!m_cmdu_tx.create(0, ieee1905_1::eMessageType::CLIENT_CAPABILITY_QUERY_MESSAGE)) {
        LOG(ERROR) << "cmdu creation of type CLIENT_CAPABILITY_QUERY_MESSAGE, has failed";
        return false;
    }

    auto client_info_tlv = m_cmdu_tx.addClass<wfa_map::tlvClientInfo>();

    if (!client_info_tlv) {
        LOG(ERROR) << "addClass wfa_map::tlvClientInfo has failed";
        return false;
    }
    client_info_tlv->client_mac() = client_association_event_tlv->client_mac();
    client_info_tlv->bssid()      = client_association_event_tlv->bssid();
    son_actions::send_cmdu_to_agent(src_mac, m_cmdu_tx, m_database);
    return true;
}

bool client_association_task::handle_cmdu_1905_client_capability_report_message(
    const sMacAddr &src_mac, ieee1905_1::CmduMessageRx &cmdu_rx)
{
    auto mid                          = cmdu_rx.getMessageId();
    auto client_capability_report_tlv = cmdu_rx.getClass<wfa_map::tlvClientCapabilityReport>();
    if (!client_capability_report_tlv) {
        LOG(ERROR) << "getClass wfa_map::tlvClientCapabilityReport has failed";
        return false;
    }

    std::string result_code =
        (client_capability_report_tlv->result_code() == wfa_map::tlvClientCapabilityReport::SUCCESS)
            ? "SUCCESS"
            : "FAILURE";

    auto client_info_tlv = cmdu_rx.getClass<wfa_map::tlvClientInfo>();
    if (!client_info_tlv) {
        LOG(ERROR) << "getClass wfa_map::tlvClientInfo failed";
        return false;
    }

    auto sta_mac = client_info_tlv->client_mac();

    //log the details so it can be checked in the test_flows
    LOG(INFO) << "Received CLIENT_CAPABILITY_REPORT_MESSAGE, mid=" << std::hex << int(mid)
              << ", Result Code= " << result_code << ", client MAC= " << sta_mac
              << ", BSSID= " << client_info_tlv->bssid();

    /*
     * The remote agent reports no client capability data so return here.
     */
    if (client_capability_report_tlv->result_code() !=
        wfa_map::tlvClientCapabilityReport::SUCCESS) {
        return true;
    }

    std::string re_assoc_frame =
        beerocks::utils::dump_buffer(client_capability_report_tlv->association_frame(),
                                     client_capability_report_tlv->association_frame_length());
    LOG(DEBUG) << "(Re)Association Request frame= " << re_assoc_frame;

    // Option A integration point (telemetry enrichment):
    // After we have the association frame, emit a second pending telemetry update so the daemon
    // can make a more informed approve/deny decision (and so we can apply it).
    {
        const auto sta_mac_str   = tlvf::mac_to_string(sta_mac);
        const auto bssid_str     = tlvf::mac_to_string(client_info_tlv->bssid());
        const auto agent_al_str  = tlvf::mac_to_string(src_mac);
        const auto ssid          = m_database.get_bss_ssid(client_info_tlv->bssid());
        const std::string stage  = "capability_report";

        onboarding_approval::PendingClientEvent pending;
        pending.client_mac   = sta_mac_str;
        pending.bssid        = bssid_str;
        pending.agent_al_mac = agent_al_str;
        pending.ssid         = ssid;
        pending.stage        = stage;
        pending.assoc_frame  = re_assoc_frame;

        auto decision = approval_adapter().send_pending_client_event(pending);
        apply_onboarding_decision(m_database, m_cmdu_tx, sta_mac_str, bssid_str, decision);
    }

    /*
     * Client capability data is latest assoc/reassoc request frame data
     * so assume unknown assoc frame type to cover both cases.
     */
    auto assoc_frame =
        assoc_frame::AssocReqFrame::parse(client_capability_report_tlv->association_frame(),
                                          client_capability_report_tlv->association_frame_length());

    if (!assoc_frame) {
        LOG(ERROR) << "Failed to parse Association Request frame.";
        return false;
    }

    if (!m_database.set_sta_association_frame(
            sta_mac,
            std::vector<uint8_t>(client_capability_report_tlv->association_frame(),
                                 client_capability_report_tlv->association_frame() +
                                     client_capability_report_tlv->association_frame_length()))) {
        LOG(ERROR) << "Failed to save association frame for STA " << sta_mac;
        return false;
    }

    beerocks::message::sRadioCapabilities capabilities = {};
    auto result = son::assoc_frame_utils::get_station_capabilities_from_assoc_frame(assoc_frame,
                                                                                    capabilities);
    if (!result) {
        LOG(ERROR) << "Failed to parse station capabilities.";
        return false;
    }
    son::wireless_utils::print_station_capabilities(capabilities);

    // Save latest station capabilities to Station object
    auto sta_mac_str = tlvf::mac_to_string(sta_mac);
    result           = m_database.set_sta_capabilities(sta_mac_str, capabilities);
    if (!result) {
        LOG(ERROR) << "Failed to save station capabilities.";
        return false;
    }

    // Save station capabilities into DM AssocEvent object
    dm_add_sta_association_event_caps(client_info_tlv->client_mac(), client_info_tlv->bssid());

    // Save ClientCapabilities to Station and AssocEvent object
    result = m_database.set_client_capabilities(sta_mac, re_assoc_frame, m_database);
    if (!result) {
        LOG(ERROR) << "Failed to save client capabilities.";
        return false;
    }

    // Update the station's link bw with the received caps
    WifiChannel sta_wifi_channel = m_database.get_sta_wifi_channel(sta_mac_str);
    if (sta_wifi_channel.is_empty()) {
        LOG(WARNING) << "empty wifi channel of " << sta_mac_str << " in DB";
    }
    auto client_bw     = sta_wifi_channel.get_bandwidth();
    auto client_bw_max = client_bw;
    if (son::wireless_utils::get_station_max_supported_bw(capabilities, client_bw_max)) {
        if (client_bw_max < client_bw) {
            m_database.update_sta_wifi_channel_bw(sta_mac, client_bw_max);
        }
    }

    return true;
}

bool client_association_task::dm_add_sta_association_event(const sMacAddr &sta_mac,
                                                           const sMacAddr &bssid)
{
    // Add AssociationEventData data model object
    auto station = m_database.get_station(sta_mac);
    station->assoc_event_path =
        m_database.dm_add_association_event(bssid, sta_mac, station->assoc_timestamp);

    if (station->assoc_event_path.empty()) {
        LOG(ERROR) << "Failed to add AssociationEventData for sta: " << sta_mac;
        return false;
    }

    return true;
}

bool client_association_task::dm_add_sta_association_event_caps(const sMacAddr &sta_mac,
                                                                const sMacAddr &bssid)
{
    auto station = m_database.get_station(sta_mac);
    if (!station) {
        return false;
    }

    auto assoc_event_path = station->assoc_event_path;
    if (assoc_event_path.empty()) {
        return false;
    }

    auto sta_mac_str = tlvf::mac_to_string(sta_mac);

    auto parent_radio = m_database.get_bss_parent_radio(tlvf::mac_to_string(bssid));
    if (parent_radio.empty()) {
        return false;
    }

    auto wifi_channel = m_database.get_radio_wifi_channel(tlvf::mac_from_string(parent_radio));
    if (wifi_channel.is_empty()) {
        LOG(ERROR) << "WifiChannel is empty";
        return false;
    }

    /* if station caps are available here
     * 1) caps were retrieved from VS field in BSS_JOIN notification
     * 2) station was previously associated to same freq band, so caps won't change
     * Otherwise, controller has to query agent for client capabilities
     */
    auto capabilities = m_database.get_sta_capabilities(sta_mac_str, wifi_channel.get_freq_type());
    if (!capabilities || !capabilities->valid) {
        return false;
    }

    return m_database.dm_add_assoc_event_sta_caps(assoc_event_path, *capabilities);
}
