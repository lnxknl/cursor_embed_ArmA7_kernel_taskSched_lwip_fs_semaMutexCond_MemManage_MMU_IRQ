#include "mqtt.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

/* Network Context */
typedef struct {
    int socket;
    SSL *ssl;
    SSL_CTX *ssl_ctx;
    bool use_ssl;
} network_t;

/* Create network context */
static network_t *network_new(void)
{
    network_t *net = calloc(1, sizeof(network_t));
    return net;
}

/* Destroy network context */
static void network_destroy(network_t *net)
{
    if (!net) return;
    
    if (net->ssl) {
        SSL_shutdown(net->ssl);
        SSL_free(net->ssl);
    }
    
    if (net->ssl_ctx) {
        SSL_CTX_free(net->ssl_ctx);
    }
    
    if (net->socket >= 0) {
        close(net->socket);
    }
    
    free(net);
}

/* Connect to broker */
static int network_connect(network_t *net, const char *host, uint16_t port, const mqtt_config_t *config)
{
    struct addrinfo hints = {0}, *addr_list, *cur;
    char port_str[6];
    int ret;
    
    /* Get address info */
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    
    snprintf(port_str, sizeof(port_str), "%d", port);
    
    if ((ret = getaddrinfo(host, port_str, &hints, &addr_list)) != 0) {
        return MQTT_ERR_NO_CONN;
    }
    
    /* Try each address until we successfully connect */
    ret = MQTT_ERR_NO_CONN;
    for (cur = addr_list; cur != NULL; cur = cur->ai_next) {
        net->socket = socket(cur->ai_family, cur->ai_socktype, cur->ai_protocol);
        if (net->socket < 0) {
            ret = MQTT_ERR_NO_CONN;
            continue;
        }
        
        if (connect(net->socket, cur->ai_addr, cur->ai_addrlen) == 0) {
            ret = MQTT_OK;
            break;
        }
        
        close(net->socket);
        net->socket = -1;
    }
    
    freeaddrinfo(addr_list);
    
    if (ret != MQTT_OK) {
        return ret;
    }
    
    /* Setup SSL if required */
    if (config->use_ssl) {
        net->use_ssl = true;
        
        /* Initialize OpenSSL */
        SSL_library_init();
        SSL_load_error_strings();
        
        /* Create SSL context */
        net->ssl_ctx = SSL_CTX_new(TLS_client_method());
        if (!net->ssl_ctx) {
            return MQTT_ERR_TLS;
        }
        
        /* Load CA certificate */
        if (config->ca_cert) {
            if (!SSL_CTX_load_verify_locations(net->ssl_ctx, config->ca_cert, NULL)) {
                return MQTT_ERR_TLS;
            }
        }
        
        /* Load client certificate and key */
        if (config->client_cert && config->client_key) {
            if (!SSL_CTX_use_certificate_file(net->ssl_ctx, config->client_cert, SSL_FILETYPE_PEM) ||
                !SSL_CTX_use_PrivateKey_file(net->ssl_ctx, config->client_key, SSL_FILETYPE_PEM) ||
                !SSL_CTX_check_private_key(net->ssl_ctx)) {
                return MQTT_ERR_TLS;
            }
        }
        
        /* Create SSL connection */
        net->ssl = SSL_new(net->ssl_ctx);
        if (!net->ssl) {
            return MQTT_ERR_TLS;
        }
        
        SSL_set_fd(net->ssl, net->socket);
        
        /* Perform SSL handshake */
        if (SSL_connect(net->ssl) != 1) {
            return MQTT_ERR_TLS;
        }
    }
    
    return MQTT_OK;
}

/* Read from network */
static int network_read(network_t *net, uint8_t *buffer, size_t len, int timeout_ms)
{
    int rc;
    struct timeval tv;
    fd_set fds;
    
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    
    FD_ZERO(&fds);
    FD_SET(net->socket, &fds);
    
    rc = select(net->socket + 1, &fds, NULL, NULL, &tv);
    if (rc < 0) {
        return MQTT_ERR_UNKNOWN;
    }
    
    if (rc == 0) {
        return MQTT_ERR_TIMEOUT;
    }
    
    if (net->use_ssl) {
        rc = SSL_read(net->ssl, buffer, len);
    } else {
        rc = recv(net->socket, buffer, len, 0);
    }
    
    if (rc < 0) {
        return MQTT_ERR_CONN_LOST;
    }
    
    return rc;
}

/* Write to network */
static int network_write(network_t *net, const uint8_t *buffer, size_t len, int timeout_ms)
{
    int rc;
    struct timeval tv;
    fd_set fds;
    
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    
    FD_ZERO(&fds);
    FD_SET(net->socket, &fds);
    
    rc = select(net->socket + 1, NULL, &fds, NULL, &tv);
    if (rc < 0) {
        return MQTT_ERR_UNKNOWN;
    }
    
    if (rc == 0) {
        return MQTT_ERR_TIMEOUT;
    }
    
    if (net->use_ssl) {
        rc = SSL_write(net->ssl, buffer, len);
    } else {
        rc = send(net->socket, buffer, len, 0);
    }
    
    if (rc < 0) {
        return MQTT_ERR_CONN_LOST;
    }
    
    return rc;
} 