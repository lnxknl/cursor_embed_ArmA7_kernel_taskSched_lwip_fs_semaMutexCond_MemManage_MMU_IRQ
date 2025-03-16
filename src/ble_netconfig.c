#include "ble_netconfig.h"
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

// 事件处理线程
static void *event_thread(void *arg)
{
    ble_nc_t *handle = (ble_nc_t *)arg;
    uint8_t buf[HCI_MAX_EVENT_SIZE];
    ssize_t len;
    
    while (1) {
        len = read(handle->hci_socket, buf, sizeof(buf));
        if (len < 0) break;
        
        evt_le_meta_event *meta = (void *)(buf + HCI_EVENT_HDR_SIZE);
        
        switch (meta->subevent) {
            case EVT_LE_CONN_COMPLETE:
                handle_connection_complete(handle,
                    (void *)meta->data);
                break;
                
            case EVT_LE_CONN_UPDATE_COMPLETE:
                handle_connection_update_complete(handle,
                    (void *)meta->data);
                break;
                
            case EVT_DISCONN_COMPLETE:
                handle_disconnection_complete(handle,
                    (void *)meta->data);
                break;
        }
    }
    
    return NULL;
}

// 初始化BLE网络配置
ble_nc_t *ble_nc_init(const ble_nc_config_t *config)
{
    if (!config || !config->device_name) {
        return NULL;
    }
    
    ble_nc_t *handle = calloc(1, sizeof(ble_nc_t));
    if (!handle) {
        return NULL;
    }
    
    // 复制配置
    memcpy(&handle->config, config, sizeof(ble_nc_config_t));
    handle->config.device_name = strdup(config->device_name);
    
    // 初始化HCI
    if (init_hci(handle) != BLE_NC_OK) {
        free(handle->config.device_name);
        free(handle);
        return NULL;
    }
    
    // 初始化GATT服务
    if (init_gatt_service(handle) != BLE_NC_OK) {
        close(handle->hci_socket);
        free(handle->config.device_name);
        free(handle);
        return NULL;
    }
    
    // 创建事件处理线程
    pthread_t thread;
    if (pthread_create(&thread, NULL, event_thread, handle) != 0) {
        close(handle->hci_socket);
        free(handle->config.device_name);
        free(handle);
        return NULL;
    }
    pthread_detach(thread);
    
    return handle;
}

// 清理BLE网络配置
void ble_nc_deinit(ble_nc_t *handle)
{
    if (!handle) return;
    
    // 停止广播
    ble_nc_stop_advertising(handle);
    
    // 断开连接
    if (handle->conn) {
        ble_nc_disconnect(handle);
    }
    
    // 清理资源
    close(handle->hci_socket);
    free(handle->config.device_name);
    free(handle);
}

// 启动广播
int ble_nc_start_advertising(ble_nc_t *handle)
{
    if (!handle) return BLE_NC_ERROR_PARAM;
    
    if (handle->state != BLE_NC_STATE_IDLE) {
        return BLE_NC_ERROR_STATE;
    }
    
    // 设置广播数据
    int ret = set_advertising_data(handle);
    if (ret != BLE_NC_OK) return ret;
    
    // 启动广播
    ret = start_advertising(handle);
    if (ret != BLE_NC_OK) return ret;
    
    // 更新状态
    handle->state = BLE_NC_STATE_ADVERTISING;
    if (handle->config.state_cb) {
        handle->config.state_cb(handle->state,
                              handle->config.user_data);
    }
    
    return BLE_NC_OK;
} 