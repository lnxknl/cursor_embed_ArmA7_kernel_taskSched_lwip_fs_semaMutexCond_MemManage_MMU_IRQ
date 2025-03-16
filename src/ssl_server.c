#include "mbedtls/net_sockets.h"
#include "mbedtls/ssl.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/error.h"
#include "mbedtls/debug.h"

#define SERVER_PORT "4433"
#define DEBUG_LEVEL 1

static void my_debug(void *ctx, int level, const char *file, int line, const char *str)
{
    ((void)level);
    printf("%s:%04d: %s", file, line, str);
}

typedef struct {
    mbedtls_net_context listen_fd;
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_ssl_context ssl;
    mbedtls_ssl_config conf;
    mbedtls_x509_crt srvcert;
    mbedtls_pk_context pkey;
} ssl_server_context_t;

static ssl_server_context_t server_ctx;

int ssl_server_init(const char *cert_path, const char *key_path)
{
    int ret;
    const char *pers = "ssl_server";
    
    // 初始化 MbedTLS 组件
    mbedtls_net_init(&server_ctx.listen_fd);
    mbedtls_ssl_init(&server_ctx.ssl);
    mbedtls_ssl_config_init(&server_ctx.conf);
    mbedtls_x509_crt_init(&server_ctx.srvcert);
    mbedtls_pk_init(&server_ctx.pkey);
    mbedtls_entropy_init(&server_ctx.entropy);
    mbedtls_ctr_drbg_init(&server_ctx.ctr_drbg);
    
    // 配置调试级别
    mbedtls_debug_set_threshold(DEBUG_LEVEL);
    
    // 设置随机数生成器
    if ((ret = mbedtls_ctr_drbg_seed(&server_ctx.ctr_drbg, mbedtls_entropy_func,
                                    &server_ctx.entropy,
                                    (const unsigned char *)pers,
                                    strlen(pers))) != 0) {
        printf("mbedtls_ctr_drbg_seed returned %d\n", ret);
        return ret;
    }
    
    // 加载证书
    if ((ret = mbedtls_x509_crt_parse_file(&server_ctx.srvcert, cert_path)) != 0) {
        printf("mbedtls_x509_crt_parse_file returned %d\n", ret);
        return ret;
    }
    
    // 加载私钥
    if ((ret = mbedtls_pk_parse_keyfile(&server_ctx.pkey, key_path, "")) != 0) {
        printf("mbedtls_pk_parse_keyfile returned %d\n", ret);
        return ret;
    }
    
    // 配置SSL/TLS
    if ((ret = mbedtls_ssl_config_defaults(&server_ctx.conf,
                                         MBEDTLS_SSL_IS_SERVER,
                                         MBEDTLS_SSL_TRANSPORT_STREAM,
                                         MBEDTLS_SSL_PRESET_DEFAULT)) != 0) {
        printf("mbedtls_ssl_config_defaults returned %d\n", ret);
        return ret;
    }
    
    mbedtls_ssl_conf_rng(&server_ctx.conf, mbedtls_ctr_drbg_random,
                         &server_ctx.ctr_drbg);
    mbedtls_ssl_conf_dbg(&server_ctx.conf, my_debug, stdout);
    
    mbedtls_ssl_conf_ca_chain(&server_ctx.conf, server_ctx.srvcert.next, NULL);
    if ((ret = mbedtls_ssl_conf_own_cert(&server_ctx.conf, &server_ctx.srvcert,
                                        &server_ctx.pkey)) != 0) {
        printf("mbedtls_ssl_conf_own_cert returned %d\n", ret);
        return ret;
    }
    
    // 绑定端口
    if ((ret = mbedtls_net_bind(&server_ctx.listen_fd, NULL,
                               SERVER_PORT, MBEDTLS_NET_PROTO_TCP)) != 0) {
        printf("mbedtls_net_bind returned %d\n", ret);
        return ret;
    }
    
    return 0;
}

int ssl_server_handle_client(void)
{
    int ret;
    mbedtls_net_context client_fd;
    mbedtls_ssl_context ssl;
    
    mbedtls_net_init(&client_fd);
    mbedtls_ssl_init(&ssl);
    
    // 等待客户端连接
    if ((ret = mbedtls_net_accept(&server_ctx.listen_fd, &client_fd,
                                 NULL, 0, NULL)) != 0) {
        printf("mbedtls_net_accept returned %d\n", ret);
        return ret;
    }
    
    // 配置SSL会话
    if ((ret = mbedtls_ssl_setup(&ssl, &server_ctx.conf)) != 0) {
        printf("mbedtls_ssl_setup returned %d\n", ret);
        goto exit;
    }
    
    mbedtls_ssl_set_bio(&ssl, &client_fd, mbedtls_net_send, mbedtls_net_recv, NULL);
    
    // 执行SSL握手
    while ((ret = mbedtls_ssl_handshake(&ssl)) != 0) {
        if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
            printf("mbedtls_ssl_handshake returned -0x%x\n", -ret);
            goto exit;
        }
    }
    
    // 处理数据交换
    unsigned char buf[1024];
    while (1) {
        ret = mbedtls_ssl_read(&ssl, buf, sizeof(buf));
        
        if (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE) {
            continue;
        }
        
        if (ret <= 0) {
            switch (ret) {
                case MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY:
                    printf("Connection closed by client\n");
                    goto exit;
                
                case MBEDTLS_ERR_NET_CONN_RESET:
                    printf("Connection reset by peer\n");
                    goto exit;
                
                default:
                    printf("mbedtls_ssl_read returned -0x%x\n", -ret);
                    goto exit;
            }
        }
        
        // 处理接收到的数据
        buf[ret] = '\0';
        printf("Received: %s\n", buf);
        
        // 发送响应
        const char *response = "Server received your message\n";
        len = strlen(response);
        
        while ((ret = mbedtls_ssl_write(&ssl, (unsigned char *)response, len)) <= 0) {
            if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
                printf("mbedtls_ssl_write returned %d\n", ret);
                goto exit;
            }
        }
    }
    
exit:
    mbedtls_ssl_close_notify(&ssl);
    mbedtls_net_free(&client_fd);
    mbedtls_ssl_free(&ssl);
    return ret;
} 

// 安全远程设备管理系统示例

// 设备信息结构
typedef struct {
    char device_id[32];
    char model[32];
    char firmware_version[16];
    uint32_t status;
    uint32_t last_update;
} device_info_t;

// 命令结构
typedef struct {
    uint32_t cmd_id;
    uint32_t param_len;
    uint8_t parameters[256];
} device_command_t;

// 设备管理器
typedef struct {
    secure_session_t session;
    device_info_t info;
    mbedtls_pk_context device_key;
    uint8_t is_authenticated;
} device_manager_t;

// 初始化设备管理器
static int init_device_manager(device_manager_t *manager,
                             const char *device_id,
                             const char *key_file)
{
    memset(manager, 0, sizeof(device_manager_t));
    
    // 设置设备信息
    strncpy(manager->info.device_id, device_id, sizeof(manager->info.device_id));
    strncpy(manager->info.model, "SECURE-IOT-V1", sizeof(manager->info.model));
    strncpy(manager->info.firmware_version, "1.0.0", sizeof(manager->info.firmware_version));
    
    // 加载设备私钥
    mbedtls_pk_init(&manager->device_key);
    int ret = mbedtls_pk_parse_keyfile(&manager->device_key, key_file, NULL);
    if (ret != 0) {
        printf("Failed to load device key: %d\n", ret);
        return ret;
    }
    
    // 初始化安全会话
    ret = init_secure_session(&manager->session);
    if (ret != 0) {
        printf("Failed to initialize secure session: %d\n", ret);
        return ret;
    }
    
    return 0;
}

// 处理设备命令
static int handle_device_command(device_manager_t *manager,
                               const device_command_t *cmd)
{
    secure_message_t response;
    response.msg_type = MSG_TYPE_STATUS;
    response.msg_len = sizeof(device_info_t);
    
    switch (cmd->cmd_id) {
        case CMD_GET_STATUS:
            // 返回设备状态
            memcpy(response.data, &manager->info, sizeof(device_info_t));
            break;
            
        case CMD_UPDATE_FIRMWARE:
            // 处理固件更新
            if (verify_firmware_signature(cmd->parameters, cmd->param_len)) {
                start_firmware_update(cmd->parameters, cmd->param_len);
                manager->info.status = STATUS_UPDATING;
            } else {
                manager->info.status = STATUS_ERROR;
            }
            memcpy(response.data, &manager->info, sizeof(device_info_t));
            break;
            
        case CMD_CONFIGURE:
            // 处理配置更新
            if (update_device_config(cmd->parameters, cmd->param_len)) {
                manager->info.status = STATUS_CONFIGURED;
            } else {
                manager->info.status = STATUS_ERROR;
            }
            memcpy(response.data, &manager->info, sizeof(device_info_t));
            break;
            
        default:
            printf("Unknown command: %d\n", cmd->cmd_id);
            return -1;
    }
    
    // 计算响应签名
    uint8_t hash[32];
    mbedtls_sha256(response.data, response.msg_len, hash, 0);
    
    size_t sig_len;
    int ret = mbedtls_pk_sign(&manager->device_key, MBEDTLS_MD_SHA256,
                             hash, 0, response.signature, &sig_len,
                             mbedtls_ctr_drbg_random, &manager->session.ctr_drbg);
    
    if (ret != 0) {
        printf("Failed to sign response: %d\n", ret);
        return ret;
    }
    
    // 发送响应
    ret = mbedtls_ssl_write(&manager->session.ssl,
                           (unsigned char *)&response,
                           sizeof(response));
    
    return ret;
}

// 主循环
static void device_manager_loop(device_manager_t *manager)
{
    secure_message_t message;
    
    while (1) {
        // 接收命令
        int ret = mbedtls_ssl_read(&manager->session.ssl,
                                 (unsigned char *)&message,
                                 sizeof(message));
        
        if (ret <= 0) {
            if (ret == MBEDTLS_ERR_SSL_WANT_READ ||
                ret == MBEDTLS_ERR_SSL_WANT_WRITE) {
                continue;
            }
            
            if (ret == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY) {
                printf("Connection closed by peer\n");
                break;
            }
            
            printf("mbedtls_ssl_read returned %d\n", ret);
            break;
        }
        
        // 验证消息签名
        uint8_t hash[32];
        mbedtls_sha256(message.data, message.msg_len, hash, 0);
        
        ret = mbedtls_pk_verify(&server_pubkey, MBEDTLS_MD_SHA256,
                               hash, 0, message.signature, 64);
        
        if (ret != 0) {
            printf("Invalid message signature\n");
            continue;
        }
        
        // 处理消息
        switch (message.msg_type) {
            case MSG_TYPE_COMMAND:
                handle_device_command(manager,
                                   (device_command_t *)message.data);
                break;
                
            case MSG_TYPE_AUTH_REQUEST:
                handle_auth_request(manager,
                                  message.data,
                                  message.msg_len);
                break;
                
            default:
                printf("Unknown message type: %d\n", message.msg_type);
                break;
        }
        
        // 更新状态
        manager->info.last_update = time(NULL);
    }
}

// 主函数
int main(void)
{
    device_manager_t manager;
    
    // 初始化设备管理器
    int ret = init_device_manager(&manager, "DEVICE001", "device.key");
    if (ret != 0) {
        printf("Failed to initialize device manager\n");
        return ret;
    }
    
    // 连接到管理服务器
    ret = connect_to_server(&manager.session, "server.example.com", "4433");
    if (ret != 0) {
        printf("Failed to connect to server\n");
        return ret;
    }
    
    // 运行主循环
    device_manager_loop(&manager);
    
    // 清理资源
    cleanup_device_manager(&manager);
    
    return 0;
}