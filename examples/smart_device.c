#include "ble_netconfig.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/time.h>

// 设备状态
typedef struct {
    bool configured;
    char ssid[33];
    int rssi;
    pthread_mutex_t lock;
} device_state_t;

static device_state_t g_state = {
    .configured = false,
    .lock = PTHREAD_MUTEX_INITIALIZER
};

static bool g_running = true;

// WiFi配置回调
static void config_callback(const ble_nc_wifi_config_t *config,
                          void *user_data)
{
    printf("Received WiFi configuration:\n");
    printf("  SSID: %s\n", config->ssid);
    printf("  Security: %d\n", config->security);
    printf("  Hidden: %s\n", config->hidden ? "yes" : "no");
    
    // 模拟WiFi连接过程
    printf("Connecting to WiFi...\n");
    sleep(2);
    
    pthread_mutex_lock(&g_state.lock);
    strncpy(g_state.ssid, config->ssid, sizeof(g_state.ssid) - 1);
    g_state.configured = true;
    g_state.rssi = -50;  // 模拟信号强度
    pthread_mutex_unlock(&g_state.lock);
    
    // 更新配置状态
    ble_nc_t *handle = (ble_nc_t *)user_data;
    ble_nc_set_config_status(handle, true, "Connected");
}

// 状态回调
static void state_callback(ble_nc_state_t state, void *user_data)
{
    const char *state_str;
    switch (state) {
        case BLE_NC_STATE_IDLE:
            state_str = "IDLE";
            break;
        case BLE_NC_STATE_ADVERTISING:
            state_str = "ADVERTISING";
            break;
        case BLE_NC_STATE_CONNECTED:
            state_str = "CONNECTED";
            break;
        case BLE_NC_STATE_CONFIGURING:
            state_str = "CONFIGURING";
            break;
        case BLE_NC_STATE_CONFIGURED:
            state_str = "CONFIGURED";
            break;
        case BLE_NC_STATE_ERROR:
            state_str = "ERROR";
            break;
        default:
            state_str = "UNKNOWN";
            break;
    }
    
    printf("BLE state changed: %s\n", state_str);
}

// 信号处理
static void signal_handler(int signo)
{
    g_running = false;
}

// 主循环
static void main_loop(ble_nc_t *handle)
{
    while (g_running) {
        pthread_mutex_lock(&g_state.lock);
        
        if (g_state.configured) {
            // 模拟信号强度变化
            g_state.rssi += (rand() % 3) - 1;
            if (g_state.rssi < -90) g_state.rssi = -90;
            if (g_state.rssi > -30) g_state.rssi = -30;
            
            // 更新状态
            char status[64];
            snprintf(status, sizeof(status),
                    "Connected to %s (RSSI: %d dBm)",
                    g_state.ssid, g_state.rssi);
            ble_nc_set_config_status(handle, true, status);
        }
        
        pthread_mutex_unlock(&g_state.lock);
        
        sleep(1);
    }
}

int main(int argc, char *argv[])
{
    // 设置信号处理
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // 初始化BLE配置
    ble_nc_config_t config = {
        .device_name = "Smart Device",
        .adv_interval = 0x0800,  // 1.28s
        .conn_interval = 0x0018,  // 30ms
        .slave_latency = 0,
        .sup_timeout = 0x0048,   // 720ms
        .config_cb = config_callback,
        .state_cb = state_callback
    };
    
    // 创建BLE网络配置实例
    ble_nc_t *handle = ble_nc_init(&config);
    if (!handle) {
        printf("Failed to initialize BLE network config\n");
        return 1;
    }
    
    // 设置回调上下文
    config.user_data = handle;
    
    // 启动广播
    if (ble_nc_start_advertising(handle) != BLE_NC_OK) {
        printf("Failed to start advertising\n");
        ble_nc_deinit(handle);
        return 1;
    }
    
    printf("Smart device started, waiting for configuration...\n");
    
    // 运行主循环
    main_loop(handle);
    
    // 清理
    ble_nc_deinit(handle);
    return 0;
} 