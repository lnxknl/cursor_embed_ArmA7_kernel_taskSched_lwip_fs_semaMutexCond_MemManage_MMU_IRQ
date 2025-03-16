#include "ble_netconfig.h"
#include <stdlib.h>
#include <string.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/l2cap.h>
#include <bluetooth/att.h>
#include <bluetooth/gatt.h>

// BLE特性结构
typedef struct {
    uint16_t handle;
    uint8_t *value;
    size_t value_len;
    bool notify_enabled;
} ble_characteristic_t;

// BLE服务结构
typedef struct {
    uint16_t start_handle;
    uint16_t end_handle;
    ble_characteristic_t wifi_char;
    ble_characteristic_t status_char;
} ble_service_t;

// BLE连接结构
typedef struct {
    int socket;
    uint16_t mtu;
    bd_addr_t peer_addr;
    bool encrypted;
} ble_connection_t;

// BLE句柄结构
struct ble_nc_handle {
    int hci_socket;
    ble_service_t service;
    ble_connection_t *conn;
    ble_nc_config_t config;
    ble_nc_state_t state;
    uint8_t adv_data[31];
    uint8_t scan_rsp_data[31];
};

// 初始化HCI
static int init_hci(ble_nc_t *handle)
{
    handle->hci_socket = socket(AF_BLUETOOTH, SOCK_RAW, BTPROTO_HCI);
    if (handle->hci_socket < 0) {
        return BLE_NC_ERROR_INIT;
    }
    
    struct hci_filter flt;
    hci_filter_clear(&flt);
    hci_filter_set_ptype(HCI_EVENT_PKT, &flt);
    hci_filter_set_event(EVT_LE_META_EVENT, &flt);
    
    if (setsockopt(handle->hci_socket, SOL_HCI, HCI_FILTER,
                   &flt, sizeof(flt)) < 0) {
        close(handle->hci_socket);
        return BLE_NC_ERROR_INIT;
    }
    
    return BLE_NC_OK;
}

// 设置广播数据
static int set_advertising_data(ble_nc_t *handle)
{
    uint8_t *pos = handle->adv_data;
    
    // 设置标志
    *pos++ = 2;  // 长度
    *pos++ = 0x01;  // 类型：标志
    *pos++ = 0x06;  // LE General Discoverable + BR/EDR Not Supported
    
    // 设置服务UUID
    *pos++ = 17;  // 长度
    *pos++ = 0x07;  // 类型：完整128位UUID列表
    memcpy(pos, BLE_NC_SERVICE_UUID, 16);
    pos += 16;
    
    // 设置设备名称
    size_t name_len = strlen(handle->config.device_name);
    if (name_len > 0) {
        *pos++ = name_len + 1;  // 长度
        *pos++ = 0x09;  // 类型：完整本地名称
        memcpy(pos, handle->config.device_name, name_len);
        pos += name_len;
    }
    
    // 发送HCI命令
    struct hci_request rq;
    le_set_advertising_data_cp data_cp;
    uint8_t status;
    
    memset(&data_cp, 0, sizeof(data_cp));
    memcpy(data_cp.data, handle->adv_data, sizeof(handle->adv_data));
    
    memset(&rq, 0, sizeof(rq));
    rq.ogf = OGF_LE_CTL;
    rq.ocf = OCF_LE_SET_ADVERTISING_DATA;
    rq.cparam = &data_cp;
    rq.clen = LE_SET_ADVERTISING_DATA_CP_SIZE;
    rq.rparam = &status;
    rq.rlen = 1;
    
    if (hci_send_req(handle->hci_socket, &rq, 1000) < 0) {
        return BLE_NC_ERROR_INIT;
    }
    
    return status ? BLE_NC_ERROR_INIT : BLE_NC_OK;
}

// 启动广播
static int start_advertising(ble_nc_t *handle)
{
    struct hci_request rq;
    le_set_advertising_parameters_cp adv_params_cp;
    uint8_t status;
    
    memset(&adv_params_cp, 0, sizeof(adv_params_cp));
    adv_params_cp.min_interval = htobs(handle->config.adv_interval);
    adv_params_cp.max_interval = htobs(handle->config.adv_interval);
    adv_params_cp.advtype = 0x00;  // ADV_IND
    adv_params_cp.chan_map = 0x07;  // 所有通道
    
    memset(&rq, 0, sizeof(rq));
    rq.ogf = OGF_LE_CTL;
    rq.ocf = OCF_LE_SET_ADVERTISING_PARAMETERS;
    rq.cparam = &adv_params_cp;
    rq.clen = LE_SET_ADVERTISING_PARAMETERS_CP_SIZE;
    rq.rparam = &status;
    rq.rlen = 1;
    
    if (hci_send_req(handle->hci_socket, &rq, 1000) < 0) {
        return BLE_NC_ERROR_INIT;
    }
    
    le_set_advertise_enable_cp adv_cp;
    memset(&adv_cp, 0, sizeof(adv_cp));
    adv_cp.enable = 0x01;
    
    memset(&rq, 0, sizeof(rq));
    rq.ogf = OGF_LE_CTL;
    rq.ocf = OCF_LE_SET_ADVERTISE_ENABLE;
    rq.cparam = &adv_cp;
    rq.clen = LE_SET_ADVERTISE_ENABLE_CP_SIZE;
    rq.rparam = &status;
    rq.rlen = 1;
    
    if (hci_send_req(handle->hci_socket, &rq, 1000) < 0) {
        return BLE_NC_ERROR_INIT;
    }
    
    return status ? BLE_NC_ERROR_INIT : BLE_NC_OK;
} 