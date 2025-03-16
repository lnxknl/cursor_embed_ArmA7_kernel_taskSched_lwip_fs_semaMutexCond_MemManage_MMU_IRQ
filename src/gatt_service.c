#include "ble_netconfig.h"
#include <stdlib.h>
#include <string.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/l2cap.h>
#include <bluetooth/att.h>
#include <bluetooth/gatt.h>

// GATT属性权限
#define PERM_READ                  (1 << 0)
#define PERM_WRITE                 (1 << 1)
#define PERM_READ_ENCRYPTED       (1 << 2)
#define PERM_WRITE_ENCRYPTED      (1 << 3)
#define PERM_READ_AUTHEN          (1 << 4)
#define PERM_WRITE_AUTHEN         (1 << 5)
#define PERM_NOTIFY               (1 << 6)

// GATT属性结构
typedef struct {
    uint16_t handle;
    uint16_t type;
    uint8_t *value;
    size_t value_len;
    uint16_t permissions;
} gatt_attr_t;

// GATT服务定义
static gatt_attr_t service_attrs[] = {
    // 主服务声明
    {
        .type = GATT_UUID_PRIMARY_SERVICE,
        .value = (uint8_t *)BLE_NC_SERVICE_UUID,
        .value_len = 16,
        .permissions = PERM_READ
    },
    
    // WiFi配置特性声明
    {
        .type = GATT_UUID_CHARACTERISTIC,
        .value = (uint8_t *)BLE_NC_CHAR_WIFI_UUID,
        .value_len = 16,
        .permissions = PERM_READ
    },
    
    // WiFi配置特性值
    {
        .type = BLE_NC_CHAR_WIFI_UUID,
        .value = NULL,
        .value_len = 0,
        .permissions = PERM_WRITE_ENCRYPTED
    },
    
    // 状态特性声明
    {
        .type = GATT_UUID_CHARACTERISTIC,
        .value = (uint8_t *)BLE_NC_CHAR_STATUS_UUID,
        .value_len = 16,
        .permissions = PERM_READ
    },
    
    // 状态特性值
    {
        .type = BLE_NC_CHAR_STATUS_UUID,
        .value = NULL,
        .value_len = 0,
        .permissions = PERM_READ | PERM_NOTIFY
    },
    
    // 状态特性客户端配置描述符
    {
        .type = GATT_UUID_CLIENT_CHAR_CONFIG,
        .value = NULL,
        .value_len = 2,
        .permissions = PERM_READ | PERM_WRITE
    }
};

// 处理ATT读请求
static int handle_read_request(ble_nc_t *handle,
                             uint16_t attr_handle,
                             uint8_t *response,
                             size_t *response_len)
{
    // 查找属性
    gatt_attr_t *attr = NULL;
    for (size_t i = 0; i < sizeof(service_attrs)/sizeof(service_attrs[0]); i++) {
        if (service_attrs[i].handle == attr_handle) {
            attr = &service_attrs[i];
            break;
        }
    }
    
    if (!attr || !(attr->permissions & PERM_READ)) {
        return ATT_ERROR_READ_NOT_PERMITTED;
    }
    
    // 检查加密要求
    if ((attr->permissions & PERM_READ_ENCRYPTED) &&
        !handle->conn->encrypted) {
        return ATT_ERROR_INSUFFICIENT_ENCRYPTION;
    }
    
    // 复制属性值
    memcpy(response, attr->value, attr->value_len);
    *response_len = attr->value_len;
    
    return 0;
}

// 处理ATT写请求
static int handle_write_request(ble_nc_t *handle,
                              uint16_t attr_handle,
                              const uint8_t *value,
                              size_t value_len)
{
    // 查找属性
    gatt_attr_t *attr = NULL;
    for (size_t i = 0; i < sizeof(service_attrs)/sizeof(service_attrs[0]); i++) {
        if (service_attrs[i].handle == attr_handle) {
            attr = &service_attrs[i];
            break;
        }
    }
    
    if (!attr || !(attr->permissions & PERM_WRITE)) {
        return ATT_ERROR_WRITE_NOT_PERMITTED;
    }
    
    // 检查加密要求
    if ((attr->permissions & PERM_WRITE_ENCRYPTED) &&
        !handle->conn->encrypted) {
        return ATT_ERROR_INSUFFICIENT_ENCRYPTION;
    }
    
    // 处理WiFi配置
    if (attr->type == BLE_NC_CHAR_WIFI_UUID) {
        ble_nc_wifi_config_t config;
        if (parse_wifi_config(value, value_len, &config) != 0) {
            return ATT_ERROR_INVALID_ATTRIBUTE_VALUE_LENGTH;
        }
        
        // 调用配置回调
        if (handle->config.config_cb) {
            handle->config.config_cb(&config, handle->config.user_data);
        }
        
        // 更新状态
        handle->state = BLE_NC_STATE_CONFIGURING;
        if (handle->config.state_cb) {
            handle->config.state_cb(handle->state,
                                  handle->config.user_data);
        }
    }
    
    // 处理客户端配置描述符
    else if (attr->type == GATT_UUID_CLIENT_CHAR_CONFIG) {
        if (value_len != 2) {
            return ATT_ERROR_INVALID_ATTRIBUTE_VALUE_LENGTH;
        }
        
        uint16_t cccd_value = (value[1] << 8) | value[0];
        handle->service.status_char.notify_enabled =
            (cccd_value & 0x0001) != 0;
    }
    
    return 0;
} 