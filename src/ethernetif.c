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