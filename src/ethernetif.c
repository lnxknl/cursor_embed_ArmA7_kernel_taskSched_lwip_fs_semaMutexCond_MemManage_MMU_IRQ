#include "lwip/opt.h"
#include "lwip/def.h"
#include "lwip/mem.h"
#include "lwip/pbuf.h"
#include "lwip/stats.h"
#include "lwip/snmp.h"
#include "lwip/etharp.h"
#include "netif/etharp.h"
#include "ethernetif.h"
#include <string.h>

// 网络接口状态
static struct netif *active_netif = NULL;
static uint8_t mac_addr[6] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55};

// 初始化网络接口
static err_t ethernetif_init(struct netif *netif)
{
    LWIP_ASSERT("netif != NULL", (netif != NULL));

    // 设置接口名称
    netif->name[0] = 'e';
    netif->name[1] = '0';

    // 设置MAC地址
    netif->hwaddr_len = ETHARP_HWADDR_LEN;
    memcpy(netif->hwaddr, mac_addr, ETHARP_HWADDR_LEN);

    // 设置MTU
    netif->mtu = 1500;

    // 设置标志
    netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_LINK_UP;

    // 设置回调函数
    netif->output = etharp_output;
    netif->linkoutput = low_level_output;

    // 初始化硬件
    low_level_init(netif);

    active_netif = netif;
    return ERR_OK;
}

// 底层初始化
static void low_level_init(struct netif *netif)
{
    // 初始化以太网控制器
    eth_init();

    // 设置中断处理
    eth_set_rx_callback(ethernetif_input);
}

// 发送数据包
static err_t low_level_output(struct netif *netif, struct pbuf *p)
{
    struct pbuf *q;
    uint8_t *buffer;
    uint16_t len = 0;

    // 分配发送缓冲区
    buffer = eth_get_tx_buffer();
    if (buffer == NULL) {
        return ERR_MEM;
    }

    // 复制数据到发送缓冲区
    for(q = p; q != NULL; q = q->next) {
        memcpy(buffer + len, q->payload, q->len);
        len += q->len;
    }

    // 发送数据包
    if (eth_send_packet(buffer, len) != 0) {
        return ERR_IF;
    }

    LINK_STATS_INC(link.xmit);
    return ERR_OK;
}

// 接收数据包
void ethernetif_input(struct netif *netif)
{
    struct pbuf *p;
    uint16_t len;
    uint8_t *buffer;

    // 获取接收缓冲区
    buffer = eth_get_rx_buffer(&len);
    if (buffer == NULL) {
        return;
    }

    // 分配pbuf
    p = pbuf_alloc(PBUF_RAW, len, PBUF_POOL);
    if (p != NULL) {
        struct pbuf *q;
        uint16_t offset = 0;

        // 复制数据到pbuf
        for(q = p; q != NULL; q = q->next) {
            memcpy(q->payload, buffer + offset, q->len);
            offset += q->len;
        }

        // 传递给协议栈
        if (netif->input(p, netif) != ERR_OK) {
            pbuf_free(p);
        }
    }

    // 释放接收缓冲区
    eth_release_rx_buffer(buffer);
    LINK_STATS_INC(link.recv);
} 
// 智能家居网关应用示例

// 设备状态结构
typedef struct {
    bool light_status;
    int temperature;
    int humidity;
    bool door_locked;
    bool window_opened;
    int air_quality;
} home_status_t;

static home_status_t home_status;

// JSON处理函数
static char* create_status_json(void)
{
    static char json_buffer[512];
    
    snprintf(json_buffer, sizeof(json_buffer),
             "{"
             "\"light\": %s,"
             "\"temperature\": %d,"
             "\"humidity\": %d,"
             "\"door\": %s,"
             "\"window\": %s,"
             "\"air_quality\": %d"
             "}",
             home_status.light_status ? "true" : "false",
             home_status.temperature,
             home_status.humidity,
             home_status.door_locked ? "locked" : "unlocked",
             home_status.window_opened ? "opened" : "closed",
             home_status.air_quality);
    
    return json_buffer;
}

// MQTT消息处理
static void mqtt_handle_message(const char *topic, const char *message)
{
    if (strcmp(topic, "home/light/set") == 0) {
        home_status.light_status = (strcmp(message, "on") == 0);
        // 控制实际的灯
        control_light(home_status.light_status);
        
    } else if (strcmp(topic, "home/door/set") == 0) {
        home_status.door_locked = (strcmp(message, "lock") == 0);
        // 控制门锁
        control_door(home_status.door_locked);
        
    } else if (strcmp(topic, "home/window/set") == 0) {
        home_status.window_opened = (strcmp(message, "open") == 0);
        // 控制窗户
        control_window(home_status.window_opened);
    }
    
    // 发布状态更新
    mqtt_publish("home/status", create_status_json(), 0);
}

// 传感器数据采集任务
static void sensor_task(void *arg)
{
    while (1) {
        // 读取温湿度传感器
        home_status.temperature = read_temperature();
        home_status.humidity = read_humidity();
        
        // 读取空气质量传感器
        home_status.air_quality = read_air_quality();
        
        // 发布传感器数据
        char json_buffer[128];
        
        snprintf(json_buffer, sizeof(json_buffer),
                "{\"temperature\": %d, \"humidity\": %d}",
                home_status.temperature,
                home_status.humidity);
        mqtt_publish("home/sensor/temp_humid", json_buffer, 0);
        
        snprintf(json_buffer, sizeof(json_buffer),
                "{\"value\": %d}",
                home_status.air_quality);
        mqtt_publish("home/sensor/air_quality", json_buffer, 0);
        
        // 每5秒更新一次
        sys_msleep(5000);
    }
}

// Web服务器处理函数
static void handle_http_request(int sock, const char *request)
{
    if (strstr(request, "GET /api/status") != NULL) {
        // 返回设备状态
        const char *json = create_status_json();
        
        char response[1024];
        snprintf(response, sizeof(response),
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: application/json\r\n"
                "Content-Length: %d\r\n"
                "Connection: close\r\n"
                "\r\n"
                "%s",
                strlen(json), json);
        
        send(sock, response, strlen(response), 0);
        
    } else if (strstr(request, "GET /") != NULL) {
        // 返回控制页面
        const char *html =
            "<!DOCTYPE html><html>"
            "<head><title>Smart Home Control</title></head>"
            "<body>"
            "<h1>Smart Home Control Panel</h1>"
            "<div id='status'></div>"
            "<button onclick='toggleLight()'>Toggle Light</button>"
            "<button onclick='toggleDoor()'>Toggle Door</button>"
            "<button onclick='toggleWindow()'>Toggle Window</button>"
            "<script>"
            "function updateStatus() {"
            "  fetch('/api/status')"
            "    .then(response => response.json())"
            "    .then(data => {"
            "      document.getElementById('status').innerHTML = "
            "        '<p>Light: ' + (data.light ? 'On' : 'Off') + '</p>' +"
            "        '<p>Temperature: ' + data.temperature + '°C</p>' +"
            "        '<p>Humidity: ' + data.humidity + '%</p>' +"
            "        '<p>Door: ' + (data.door ? 'Locked' : 'Unlocked') + '</p>' +"
            "        '<p>Window: ' + (data.window ? 'Opened' : 'Closed') + '</p>' +"
            "        '<p>Air Quality: ' + data.air_quality + '</p>';"
            "    });"
            "}"
            "setInterval(updateStatus, 1000);"
            "updateStatus();"
            "</script>"
            "</body></html>";
        
        char response[2048];
        snprintf(response, sizeof(response),
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: text/html\r\n"
                "Content-Length: %d\r\n"
                "Connection: close\r\n"
                "\r\n"
                "%s",
                strlen(html), html);
        
        send(sock, response, strlen(response), 0);
    }
}

// 主函数
int main(void)
{
    // 初始化硬件
    hardware_init();
    
    // 初始化网络
    network_init();
    
    // 初始化设备状态
    memset(&home_status, 0, sizeof(home_status));
    
    // 创建传感器任务
    sys_thread_new("sensor", sensor_task, NULL,
                   DEFAULT_THREAD_STACKSIZE, DEFAULT_THREAD_PRIO);
    
    // 主循环
    while (1) {
        // 处理系统事件
        sys_check_timeouts();
        
        // 其他系统任务
        process_system_tasks();
    }
    
    return 0;
}