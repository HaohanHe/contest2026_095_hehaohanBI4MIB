/**
 * vendor/allwinnertech/apps/bt_instance/bt_factorytest.c
 * Application to run bt instance on OpenVela.
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.  The
 * ASF licenses this file to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance with the
 * License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
 * License for the specific language governing permissions and limitations
 * under the License.
 *
 * @file bt_start.c
 * @brief bt工厂测试
 * @version 1.0
 */

#include <getopt.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(__NuttX__)
#include <system/readline.h>
#endif

#include "bluetooth.h"
#include "bt_adapter.h"
#include "bt_start.h"
#include "utils.h"

static void usage(void);
#if 0
static int usage_cmd(void* handle, int argc, char** argv);
static int enable_cmd(void* handle, int argc, char** argv);
static int disable_cmd(void* handle, int argc, char** argv);
#endif

bt_instance_t* g_bt_inst = NULL;
static void* adapter_callback = NULL;
static bool g_cmd_had_inited = false;
static bool g_auto_accept_pair = true;
static bond_state_t g_bond_state = BOND_STATE_NONE;
static bt_adapter_state_t g_adapter_state = BT_ADAPTER_STATE_OFF;
static void profile_init(void* handle)
{
#ifdef CONFIG_BLUETOOTH_BLE_SCAN
    scan_command_init(handle); 
#endif
#ifdef CONFIG_BLUETOOTH_A2DP_SINK
    a2dp_sink_commond_init(handle);
#endif
#ifdef CONFIG_BLUETOOTH_A2DP_SOURCE
    a2dp_src_commond_init(handle);
#endif
#ifdef CONFIG_BLUETOOTH_AVRCP_CONTROL
    avrcp_control_commond_init(handle);
#endif
#ifdef CONFIG_BLUETOOTH_HFP_HF
    hfp_hf_commond_init(handle);
#endif
#ifdef CONFIG_BLUETOOTH_HFP_AG
    hfp_ag_commond_init(handle);
#endif
#ifdef CONFIG_BLUETOOTH_SPP
    spp_command_init(handle);
#endif
#ifdef CONFIG_BLUETOOTH_HID_DEVICE
    hidd_command_init(handle);
#endif
#ifdef CONFIG_BLUETOOTH_PAN
    pan_command_init(handle);
#endif
#ifdef CONFIG_BLUETOOTH_GATT
    gattc_command_init(handle);
    gatts_command_init(handle);
#endif
    g_cmd_had_inited = true;
}

static void profile_uninit(void* handle)
{
    if (!g_cmd_had_inited)
        return;

#ifdef CONFIG_BLUETOOTH_BLE_SCAN
    scan_command_uninit(handle);
#endif
#ifdef CONFIG_BLUETOOTH_A2DP_SINK
    a2dp_sink_commond_uninit(handle);
#endif
#ifdef CONFIG_BLUETOOTH_A2DP_SOURCE
    a2dp_src_commond_uninit(handle);
#endif
#ifdef CONFIG_BLUETOOTH_AVRCP_CONTROL
    avrcp_control_commond_uninit(handle);
#endif
#ifdef CONFIG_BLUETOOTH_HFP_HF
    hfp_hf_commond_uninit(handle);
#endif
#ifdef CONFIG_BLUETOOTH_HFP_AG
    hfp_ag_commond_uninit(handle);
#endif
#ifdef CONFIG_BLUETOOTH_SPP
    spp_command_uninit(handle);
#endif
#ifdef CONFIG_BLUETOOTH_HID_DEVICE
    hidd_command_uninit(handle);
#endif
#ifdef CONFIG_BLUETOOTH_PAN
    pan_command_uninit(handle);
#endif
#ifdef CONFIG_BLUETOOTH_GATT
    gattc_command_uninit(handle);
    gatts_command_uninit(handle);
#endif
    g_cmd_had_inited = false;
}

#if 0
static int enable_cmd(void* handle, int argc, char** argv)
{
    bt_adapter_enable(handle);
    return CMD_OK;
}

static int disable_cmd(void* handle, int argc, char** argv)
{
    bt_adapter_disable(handle);
    return CMD_OK;
}

static int get_state_cmd(void* handle, int argc, char** argv)
{
    PRINT("Adapter State: %d", bt_adapter_get_state(handle));
    return CMD_OK;
}
#endif

static const char* bond_state_to_string(bond_state_t state)
{
    switch (state) {
    case BOND_STATE_NONE:
        return "BOND_NONE";
    case BOND_STATE_BONDING:
        return "BONDING";
    case BOND_STATE_BONDED:
        return "BONDED";
    default:
        return "UNKNOWN";
    }
}

#if 0
static void device_dump(void* handle, bt_address_t* addr, bt_transport_t transport)
{
    char uuid_str[40] = { 0 };
    char name[64] = { 0 };
    bt_uuid_t* uuids = NULL;
    uint16_t uuid_cnt = 0;
    char addr_str[BT_ADDR_STR_LENGTH] = { 0 };

    bt_addr_ba2str(addr, addr_str);
    PRINT("device [%s]", addr_str);
    if (transport == BT_TRANSPORT_BREDR) {
        bt_device_get_name(handle, addr, name, 64);
        PRINT("\tName: %s", name);
        memset(name, 0, 64);
        bt_device_get_alias(handle, addr, name, 64);
        PRINT("\tAlias: %s", name);
        PRINT("\tClass: 0x%08" PRIx32 "", bt_device_get_device_class(handle, addr));
        PRINT("\tDeviceType: %d", bt_device_get_device_type(handle, addr));
        PRINT("\tIsConnected: %d", bt_device_is_connected(handle, addr, transport));
        PRINT("\tIsEnc: %d", bt_device_is_encrypted(handle, addr, transport));
        PRINT("\tIsBonded: %d", bt_device_is_bonded(handle, addr, transport));
        PRINT("\tBondState: %s", bond_state_to_string(bt_device_get_bond_state(handle, addr, transport)));
        PRINT("\tIsBondInitiateLocal: %d", bt_device_is_bond_initiate_local(handle, addr, transport));
        bt_device_get_uuids(handle, addr, &uuids, &uuid_cnt, bttool_allocator);
        if (uuid_cnt) {
            PRINT("\tUUIDs:[%d]", uuid_cnt);
            for (int i = 0; i < uuid_cnt; i++) {
                bt_uuid_to_string(uuids + i, uuid_str, 40);
                PRINT("\t\tuuid[%-2d]: %s", i, uuid_str);
            }
        }
        free(uuids);
    } else {
        PRINT("\tIsConnected: %d", bt_device_is_connected(handle, addr, transport));
        PRINT("\tIsEnc: %d", bt_device_is_encrypted(handle, addr, transport));
        PRINT("\tIsBonded: %d", bt_device_is_bonded(handle, addr, transport));
        PRINT("\tBondState: %s", bond_state_to_string(bt_device_get_bond_state(handle, addr, transport)));
        PRINT("\tIsBondInitiateLocal: %d", bt_device_is_bond_initiate_local(handle, addr, transport));
    }
}
#endif

static void on_adapter_state_changed_cb(void* cookie, bt_adapter_state_t state)
{
    PRINT("Context:%p, Adapter state changed: %d", cookie, state);
    g_adapter_state = state;
    if (state == BT_ADAPTER_STATE_ON) {
        sleep(2);
        char name[64 + 1];
        profile_init(g_bt_inst);
        /* get name */
        bt_adapter_get_name(g_bt_inst, name, 64);
        /* get io cap */
        bt_io_capability_t cap = bt_adapter_get_io_capability(g_bt_inst);
        /* get class */
        uint32_t class = bt_adapter_get_device_class(g_bt_inst);
        /* get scan mode */
        bt_scan_mode_t mode = bt_adapter_get_scan_mode(g_bt_inst);
        /* enable key derivation */
        bt_adapter_le_enable_key_derivation(g_bt_inst, true, true);
        bt_adapter_set_page_scan_parameters(g_bt_inst, BT_BR_SCAN_TYPE_INTERLACED, 0x400, 0x24);
        PRINT("Adapter Name: %s, Cap: %d, Class: 0x%08" PRIX32 ", Mode:%d", name, cap, class, mode);
        sleep(2);
        bt_adapter_set_scan_mode(g_bt_inst, 2, 1);
        PRINT("scan mode");
        sleep(2);
    } else if (state == BT_ADAPTER_STATE_TURNING_OFF) {
        /* code */
        profile_uninit(g_bt_inst);
    } else if (state == BT_ADAPTER_STATE_OFF) {
        /* do something */
    }
}

static void on_discovery_state_changed_cb(void* cookie, bt_discovery_state_t state)
{
    PRINT("Discovery state: %s", state == BT_DISCOVERY_STATE_STARTED ? "Started" : "Stopped");
}

static void on_discovery_result_cb(void* cookie, bt_discovery_result_t* result)
{
    PRINT_ADDR("Inquiring: device [%s], name: %s, cod: %08" PRIx32 ", is HEADSET: %s, rssi: %d",
        &result->addr, result->name, result->cod, IS_HEADSET(result->cod) ? "true" : "false", result->rssi);
}

static void on_scan_mode_changed_cb(void* cookie, bt_scan_mode_t mode)
{
    PRINT("Adapter new scan mode: %d", mode);
}

static void on_device_name_changed_cb(void* cookie, const char* device_name)
{
    PRINT("Adapter update device name: %s", device_name);
}

static void on_pair_request_cb(void* cookie, bt_address_t* addr)
{
    if (g_auto_accept_pair)
        bt_device_pair_request_reply(g_bt_inst, addr, true);

    PRINT_ADDR("Incoming pair request from [%s] %s", addr, g_auto_accept_pair ? "auto accepted" : "please reply");
}

#define LINK_TYPE(trans_) (trans_ == BT_TRANSPORT_BREDR ? "BREDR" : "LE")

static void on_pair_display_cb(void* cookie, bt_address_t* addr, bt_transport_t transport, bt_pair_type_t type, uint32_t passkey)
{
    uint8_t ret = 0;
    char buff[128] = { 0 };
    char buff1[64] = { 0 };
    char addr_str[BT_ADDR_STR_LENGTH] = { 0 };

    bt_addr_ba2str(addr, addr_str);
    sprintf(buff, "Pair Display [%s][%s]", addr_str, LINK_TYPE(transport));
    switch (type) {
    case PAIR_TYPE_PASSKEY_CONFIRMATION:
        if (!g_auto_accept_pair) {
            sprintf(buff1, "[SSP][CONFIRM][%" PRIu32 "] please reply:", passkey);
            break;
        }
        ret = bt_device_set_pairing_confirmation(g_bt_inst, addr, transport, true);
        sprintf(buff1, "[SSP][CONFIRM] Auto confirm [%" PRIu32 "] %s", passkey, ret == BT_STATUS_SUCCESS ? "SUCCESS" : "FAILED");
        break;
    case PAIR_TYPE_PASSKEY_ENTRY:
        sprintf(buff1, "[SSP][ENTRY][%" PRIu32 "], please reply:", passkey);
        break;
    case PAIR_TYPE_CONSENT:
        sprintf(buff1, "[SSP][CONSENT]");
        break;
    case PAIR_TYPE_PASSKEY_NOTIFICATION:
        sprintf(buff1, "[SSP][NOTIFY][%" PRIu32 "]", passkey);
        break;
    case PAIR_TYPE_PIN_CODE:
        sprintf(buff1, "[PIN] please reply:");
        break;
    }
    strcat(buff, buff1);
    PRINT("%s", buff);
}

static void on_connect_request_cb(void* cookie, bt_address_t* addr)
{
    bt_device_connect_request_reply(g_bt_inst, addr, true);
    PRINT_ADDR("Incoming connect request from [%s], auto accepted", addr);
}

static void on_connection_state_changed_cb(void* cookie, bt_address_t* addr, bt_transport_t transport, connection_state_t state)
{
    PRINT_ADDR("Device [%s][%s] connection state: %d", addr, LINK_TYPE(transport), state);
}

static void on_bond_state_changed_cb(void* cookie, bt_address_t* addr, bt_transport_t transport,
    bond_state_t previous_state, bond_state_t current_state, bool is_ctkd)
{
    g_bond_state = current_state;
    PRINT_ADDR("Device [%s][%s] bond state: %s -> %s, is_ctkd: %d", addr, LINK_TYPE(transport),
        bond_state_to_string(previous_state), bond_state_to_string(current_state), is_ctkd);
}

static void on_le_sc_local_oob_data_got_cb(void* cookie, bt_address_t* addr, bt_128key_t c_val, bt_128key_t r_val)
{
    PRINT_ADDR("Generate local oob data for le secure connection pairing with [%s]:", addr);

    printf("\tConfirmation value: ");
    for (int i = 0; i < sizeof(bt_128key_t); i++) {
        printf("%02x", c_val[i]);
    }
    printf("\n");

    printf("\tRandom value: ");
    for (int i = 0; i < sizeof(bt_128key_t); i++) {
        printf("%02x", r_val[i]);
    }
    printf("\n");
}

static void on_remote_name_changed_cb(void* cookie, bt_address_t* addr, const char* name)
{
    PRINT_ADDR("Device [%s] name changed: %s", addr, name);
}

static void on_remote_alias_changed_cb(void* cookie, bt_address_t* addr, const char* alias)
{
    PRINT_ADDR("Device [%s] alias changed: %s", addr, alias);
}

static void on_remote_cod_changed_cb(void* cookie, bt_address_t* addr, uint32_t cod)
{
    PRINT_ADDR("Device [%s] class changed: 0x%08" PRIx32 "", addr, cod);
}

static void on_remote_uuids_changed_cb(void* cookie, bt_address_t* addr, bt_uuid_t* uuids, uint16_t size)
{
    char uuid_str[40] = { 0 };

    PRINT_ADDR("Device [%s] uuids changed", addr);

    if (size) {
        PRINT("UUIDs:[%d]", size);
        for (int i = 0; i < size; i++) {
            bt_uuid_to_string(uuids + i, uuid_str, 40);
            PRINT("\tuuid[%-2d]: %s", i, uuid_str);
        }
    }
}

const static adapter_callbacks_t g_adapter_cbs = {
    .on_adapter_state_changed = on_adapter_state_changed_cb,
    .on_discovery_state_changed = on_discovery_state_changed_cb,
    .on_discovery_result = on_discovery_result_cb,
    .on_scan_mode_changed = on_scan_mode_changed_cb,
    .on_device_name_changed = on_device_name_changed_cb,
    .on_pair_request = on_pair_request_cb,
    .on_pair_display = on_pair_display_cb,
    .on_connect_request = on_connect_request_cb,
    .on_connection_state_changed = on_connection_state_changed_cb,
    .on_bond_state_changed_extra = on_bond_state_changed_cb,
    .on_le_sc_local_oob_data_got = on_le_sc_local_oob_data_got_cb,
    .on_remote_name_changed = on_remote_name_changed_cb,
    .on_remote_alias_changed = on_remote_alias_changed_cb,
    .on_remote_cod_changed = on_remote_cod_changed_cb,
    .on_remote_uuids_changed = on_remote_uuids_changed_cb,
};

static int bt_inst_register(void)
{
    pthread_setschedprio(pthread_self(), CONFIG_BLUETOOTH_SERVICE_LOOP_THREAD_PRIORITY);
    g_bt_inst = bluetooth_create_instance();
    if (g_bt_inst == NULL) {
        PRINT("create instance error\n");
        return -1;
    }

    adapter_callback = bt_adapter_register_callback(g_bt_inst, &g_adapter_cbs);
    if (bt_adapter_get_state(g_bt_inst) == BT_ADAPTER_STATE_ON)
        profile_init(g_bt_inst);

    return 0;
}

static void bt_inst_unregister(void)
{
    profile_uninit(g_bt_inst);
    bt_adapter_unregister_callback(g_bt_inst, adapter_callback);
    bluetooth_delete_instance(g_bt_inst);
    g_bt_inst = NULL;
    adapter_callback = NULL;
}

int check_bt_valid(void)
{
    if((BT_ADAPTER_STATE_BLE_ON == g_adapter_state) 
    || (BT_ADAPTER_STATE_ON == g_adapter_state))
    {
        return CMD_OK;
    }
    return CMD_ERROR;
}

int enable_bt(void)
{
    PRINT("enable_bt**********");
    bt_adapter_enable(g_bt_inst);
    while(1)
    {
        sleep(1);
    } 
}

int bt_start_main(int argc, char** argv)
{
    PRINT("btinstance_main CONFIG_LIBUV_EXTENSION**********");
    bt_inst_register();
    bt_adapter_enable(g_bt_inst);
    while(1)
    {
        sleep(1);
    }

    bt_inst_unregister();
    return 0;
}