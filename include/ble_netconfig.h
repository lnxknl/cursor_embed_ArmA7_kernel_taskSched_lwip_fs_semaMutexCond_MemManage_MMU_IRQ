#ifndef BLE_NETCONFIG_H
#define BLE_NETCONFIG_H

#include <stdint.h>
#include <stdbool.h>

// 错误码定义
#define BLE_NC_OK               0
#define BLE_NC_ERROR_INIT      -1
#define BLE_NC_ERROR_PARAM     -2
#define BLE_NC_ERROR_STATE     -3
#define BLE_NC_ERROR_TIMEOUT   -4
#define BLE_NC_ERROR_MEMORY    -5
#define BLE_NC_ERROR_BUSY      -6

// BLE服务UUID
#define BLE_NC_SERVICE_UUID        "12345678-1234-5678-1234-56789abcdef0"
#define BLE_NC_CHAR_WIFI_UUID      "12345678-1234-5678-1234-56789abcdef1"
#define BLE_NC_CHAR_STATUS_UUID    "12345678-1234-5678-1234-56789abcdef2"

// 网络配置状态
typedef enum {
    BLE_NC_STATE_IDLE,
    BLE_NC_STATE_ADVERTISING,
    BLE_NC_STATE_CONNECTED,
    BLE_NC_STATE_CONFIGURING,
    BLE_NC_STATE_CONFIGURED,
    BLE_NC_STATE_ERROR
} ble_nc_state_t;

// WiFi安全类型
typedef enum {
    BLE_NC_SECURITY_OPEN,
    BLE_NC_SECURITY_WEP,
    BLE_NC_SECURITY_WPA_PSK,
    BLE_NC_SECURITY_WPA2_PSK,
    BLE_NC_SECURITY_WPA_WPA2_PSK
} ble_nc_security_t;

// WiFi配置结构
typedef struct {
    char ssid[33];
    char password[65];
    ble_nc_security_t security;
    bool hidden;
} ble_nc_wifi_config_t;

// 配置回调函数
typedef void (*ble_nc_config_callback)(const ble_nc_wifi_config_t *config,
                                     void *user_data);

// 状态回调函数
typedef void (*ble_nc_state_callback)(ble_nc_state_t state,
                                    void *user_data);

// 配置选项
typedef struct {
    const char *device_name;
    uint16_t adv_interval;
    uint16_t conn_interval;
    uint16_t slave_latency;
    uint16_t sup_timeout;
    ble_nc_config_callback config_cb;
    ble_nc_state_callback state_cb;
    void *user_data;
} ble_nc_config_t;

// BLE网络配置句柄
typedef struct ble_nc_handle ble_nc_t;

// API函数声明
ble_nc_t *ble_nc_init(const ble_nc_config_t *config);
void ble_nc_deinit(ble_nc_t *handle);

int ble_nc_start_advertising(ble_nc_t *handle);
int ble_nc_stop_advertising(ble_nc_t *handle);

int ble_nc_set_config_status(ble_nc_t *handle, bool success,
                            const char *message);

// 高级功能
int ble_nc_set_tx_power(ble_nc_t *handle, int8_t power);
int ble_nc_get_rssi(ble_nc_t *handle, int8_t *rssi);
int ble_nc_disconnect(ble_nc_t *handle);

#endif 