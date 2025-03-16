#ifndef __LWIPOPTS_H__
#define __LWIPOPTS_H__

// 系统配置
#define NO_SYS                      0
#define LWIP_SOCKET                 1
#define LWIP_NETCONN               1
#define SYS_LIGHTWEIGHT_PROT       1

// 内存配置
#define MEM_ALIGNMENT              4
#define MEM_SIZE                   (32*1024)
#define MEMP_NUM_PBUF             32
#define MEMP_NUM_UDP_PCB          8
#define MEMP_NUM_TCP_PCB          8
#define MEMP_NUM_TCP_PCB_LISTEN   8
#define MEMP_NUM_TCP_SEG          32
#define MEMP_NUM_NETBUF           8
#define MEMP_NUM_NETCONN          16
#define MEMP_NUM_SYS_TIMEOUT      8

// TCP配置
#define LWIP_TCP                   1
#define TCP_TTL                    255
#define TCP_QUEUE_OOSEQ           1
#define TCP_MSS                    1460
#define TCP_SND_BUF               (4*TCP_MSS)
#define TCP_WND                    (2*TCP_MSS)
#define TCP_SND_QUEUELEN          ((4 * (TCP_SND_BUF) + (TCP_MSS - 1))/(TCP_MSS))

// PBUF配置
#define PBUF_POOL_SIZE            16
#define PBUF_POOL_BUFSIZE         1524

// ARP配置
#define LWIP_ARP                  1
#define ARP_TABLE_SIZE            10
#define ARP_QUEUEING              1

// ICMP配置
#define LWIP_ICMP                 1
#define LWIP_RAW                  1

// DHCP配置
#define LWIP_DHCP                 1
#define LWIP_DNS                  1

// 统计配置
#define LWIP_STATS                1
#define LWIP_STATS_DISPLAY        1

// 调试配置
#define LWIP_DEBUG               LWIP_DBG_ON
#define ETHARP_DEBUG             LWIP_DBG_OFF
#define NETIF_DEBUG              LWIP_DBG_ON
#define PBUF_DEBUG               LWIP_DBG_OFF
#define API_LIB_DEBUG            LWIP_DBG_OFF
#define API_MSG_DEBUG            LWIP_DBG_OFF
#define SOCKETS_DEBUG            LWIP_DBG_ON
#define ICMP_DEBUG               LWIP_DBG_OFF
#define IGMP_DEBUG               LWIP_DBG_OFF
#define INET_DEBUG               LWIP_DBG_OFF
#define IP_DEBUG                 LWIP_DBG_OFF
#define IP_REASS_DEBUG           LWIP_DBG_OFF
#define RAW_DEBUG                LWIP_DBG_OFF
#define MEM_DEBUG                LWIP_DBG_OFF
#define MEMP_DEBUG               LWIP_DBG_OFF
#define SYS_DEBUG                LWIP_DBG_OFF
#define TIMERS_DEBUG             LWIP_DBG_OFF
#define TCP_DEBUG                LWIP_DBG_ON
#define TCP_INPUT_DEBUG          LWIP_DBG_OFF
#define TCP_FR_DEBUG             LWIP_DBG_OFF
#define TCP_RTO_DEBUG            LWIP_DBG_OFF
#define TCP_CWND_DEBUG           LWIP_DBG_OFF
#define TCP_WND_DEBUG            LWIP_DBG_OFF
#define TCP_OUTPUT_DEBUG         LWIP_DBG_OFF
#define TCP_RST_DEBUG            LWIP_DBG_OFF
#define TCP_QLEN_DEBUG           LWIP_DBG_OFF
#define UDP_DEBUG                LWIP_DBG_OFF
#define TCPIP_DEBUG              LWIP_DBG_ON
#define PPP_DEBUG                LWIP_DBG_OFF
#define SLIP_DEBUG               LWIP_DBG_OFF
#define DHCP_DEBUG               LWIP_DBG_ON

#endif 