#include "mbedtls/net_sockets.h"
#include "mbedtls/ssl.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/error.h"
#include "mbedtls/debug.h"
#include "os.h"

// 安全通信会话结构
typedef struct {
    mbedtls_net_context fd;
    mbedtls_ssl_context ssl;
    mbedtls_ssl_config conf;
    mbedtls_x509_crt cacert;
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
} secure_session_t;

// 加密消息结构
typedef struct {
    uint32_t msg_type;
    uint32_t msg_len;
    uint8_t data[1024];
    uint8_t signature[64];
} secure_message_t;

// 消息类型定义
#define MSG_TYPE_AUTH_REQUEST    1
#define MSG_TYPE_AUTH_RESPONSE   2
#define MSG_TYPE_DATA            3
#define MSG_TYPE_COMMAND         4
#define MSG_TYPE_STATUS          5

// 安全通信客户端示例
static int secure_client_example(void)
{
    secure_session_t session;
    int ret;
    
    // 初始化会话
    memset(&session, 0, sizeof(session));
    mbedtls_net_init(&session.fd);
    mbedtls_ssl_init(&session.ssl);
    mbedtls_ssl_config_init(&session.conf);
    mbedtls_x509_crt_init(&session.cacert);
    mbedtls_entropy_init(&session.entropy);
    mbedtls_ctr_drbg_init(&session.ctr_drbg);
    
    // 加载CA证书
    ret = mbedtls_x509_crt_parse_file(&session.cacert, "ca.crt");
    if (ret < 0) {
        printf("Failed to parse CA certificate: %d\n", ret);
        goto exit;
    }
    
    // 初始化RNG
    ret = mbedtls_ctr_drbg_seed(&session.ctr_drbg, mbedtls_entropy_func,
                                &session.entropy, NULL, 0);
    if (ret != 0) {
        printf("Failed to seed RNG: %d\n", ret);
        goto exit;
    }
    
    // 配置SSL/TLS
    ret = mbedtls_ssl_config_defaults(&session.conf,
                                    MBEDTLS_SSL_IS_CLIENT,
                                    MBEDTLS_SSL_TRANSPORT_STREAM,
                                    MBEDTLS_SSL_PRESET_DEFAULT);
    if (ret != 0) {
        printf("Failed to configure SSL: %d\n", ret);
        goto exit;
    }
    
    mbedtls_ssl_conf_authmode(&session.conf, MBEDTLS_SSL_VERIFY_REQUIRED);
    mbedtls_ssl_conf_ca_chain(&session.conf, &session.cacert, NULL);
    mbedtls_ssl_conf_rng(&session.conf, mbedtls_ctr_drbg_random,
                         &session.ctr_drbg);
    
    // 建立连接
    ret = mbedtls_net_connect(&session.fd, "localhost",
                             "4433", MBEDTLS_NET_PROTO_TCP);
    if (ret != 0) {
        printf("Failed to connect: %d\n", ret);
        goto exit;
    }
    
    ret = mbedtls_ssl_setup(&session.ssl, &session.conf);
    if (ret != 0) {
        printf("Failed to setup SSL: %d\n", ret);
        goto exit;
    }
    
    mbedtls_ssl_set_bio(&session.ssl, &session.fd,
                        mbedtls_net_send, mbedtls_net_recv, NULL);
    
    // 执行SSL握手
    while ((ret = mbedtls_ssl_handshake(&session.ssl)) != 0) {
        if (ret != MBEDTLS_ERR_SSL_WANT_READ &&
            ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
            printf("Failed to perform SSL handshake: %d\n", ret);
            goto exit;
        }
    }
    
    // 发送认证请求
    secure_message_t auth_req;
    auth_req.msg_type = MSG_TYPE_AUTH_REQUEST;
    auth_req.msg_len = sizeof(auth_req.data);
    
    // 填充认证数据
    memcpy(auth_req.data, "AUTH_TOKEN", 10);
    
    // 计算签名
    uint8_t hash[32];
    mbedtls_sha256(auth_req.data, auth_req.msg_len, hash, 0);
    
    size_t sig_len;
    ret = mbedtls_pk_sign(&client_key, MBEDTLS_MD_SHA256, hash, 0,
                          auth_req.signature, &sig_len,
                          mbedtls_ctr_drbg_random, &session.ctr_drbg);
    
    // 发送消息
    ret = mbedtls_ssl_write(&session.ssl, (unsigned char *)&auth_req,
                           sizeof(auth_req));
    if (ret < 0) {
        printf("Failed to send auth request: %d\n", ret);
        goto exit;
    }
    
    // 接收响应
    secure_message_t auth_resp;
    ret = mbedtls_ssl_read(&session.ssl, (unsigned char *)&auth_resp,
                          sizeof(auth_resp));
    if (ret < 0) {
        printf("Failed to receive auth response: %d\n", ret);
        goto exit;
    }
    
    // 验证响应
    if (auth_resp.msg_type != MSG_TYPE_AUTH_RESPONSE) {
        printf("Unexpected response type\n");
        goto exit;
    }
    
    // 验证签名
    mbedtls_sha256(auth_resp.data, auth_resp.msg_len, hash, 0);
    ret = mbedtls_pk_verify(&server_pubkey, MBEDTLS_MD_SHA256,
                           hash, 0, auth_resp.signature, 64);
    if (ret != 0) {
        printf("Invalid signature in auth response\n");
        goto exit;
    }
    
    // 发送数据
    secure_message_t data_msg;
    data_msg.msg_type = MSG_TYPE_DATA;
    data_msg.msg_len = sprintf((char *)data_msg.data,
                              "Secure message from client");
    
    mbedtls_sha256(data_msg.data, data_msg.msg_len, hash, 0);
    mbedtls_pk_sign(&client_key, MBEDTLS_MD_SHA256, hash, 0,
                    data_msg.signature, &sig_len,
                    mbedtls_ctr_drbg_random, &session.ctr_drbg);
    
    ret = mbedtls_ssl_write(&session.ssl, (unsigned char *)&data_msg,
                           sizeof(data_msg));
    if (ret < 0) {
        printf("Failed to send data: %d\n", ret);
        goto exit;
    }
    
exit:
    mbedtls_ssl_close_notify(&session.ssl);
    mbedtls_net_free(&session.fd);
    mbedtls_ssl_free(&session.ssl);
    mbedtls_ssl_config_free(&session.conf);
    mbedtls_x509_crt_free(&session.cacert);
    mbedtls_entropy_free(&session.entropy);
    mbedtls_ctr_drbg_free(&session.ctr_drbg);
    
    return ret;
} 