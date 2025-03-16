#ifndef MQTT_H
#define MQTT_H

#include <stdint.h>
#include <stdbool.h>

/* MQTT Version */
#define MQTT_PROTOCOL_V3    3
#define MQTT_PROTOCOL_V4    4
#define MQTT_PROTOCOL_V5    5

/* MQTT Control Packet Types */
#define MQTT_CONNECT        1
#define MQTT_CONNACK        2
#define MQTT_PUBLISH        3
#define MQTT_PUBACK        4
#define MQTT_PUBREC        5
#define MQTT_PUBREL        6
#define MQTT_PUBCOMP       7
#define MQTT_SUBSCRIBE     8
#define MQTT_SUBACK        9
#define MQTT_UNSUBSCRIBE  10
#define MQTT_UNSUBACK     11
#define MQTT_PINGREQ      12
#define MQTT_PINGRESP     13
#define MQTT_DISCONNECT   14

/* MQTT QoS Levels */
#define MQTT_QOS0          0
#define MQTT_QOS1          1
#define MQTT_QOS2          2

/* MQTT Connect Return Codes */
#define MQTT_CONN_ACCEPTED                     0
#define MQTT_CONN_REFUSED_PROTOCOL_VERSION     1
#define MQTT_CONN_REFUSED_IDENTIFIER_REJECTED  2
#define MQTT_CONN_REFUSED_SERVER_UNAVAILABLE   3
#define MQTT_CONN_REFUSED_BAD_USERNAME_PASSWORD 4
#define MQTT_CONN_REFUSED_NOT_AUTHORIZED       5

/* MQTT Error Codes */
#define MQTT_OK                    0
#define MQTT_ERR_NOMEM           -1
#define MQTT_ERR_PROTOCOL        -2
#define MQTT_ERR_INVAL           -3
#define MQTT_ERR_NO_CONN         -4
#define MQTT_ERR_CONN_REFUSED    -5
#define MQTT_ERR_NOT_FOUND       -6
#define MQTT_ERR_CONN_LOST       -7
#define MQTT_ERR_TLS             -8
#define MQTT_ERR_PAYLOAD_SIZE    -9
#define MQTT_ERR_NOT_SUPPORTED   -10
#define MQTT_ERR_AUTH            -11
#define MQTT_ERR_ACL_DENIED      -12
#define MQTT_ERR_UNKNOWN         -13
#define MQTT_ERR_TIMEOUT         -14
#define MQTT_ERR_QOS_NOT_SUPPORTED -15

/* MQTT Message Flags */
#define MQTT_FLAG_RETAIN     (1 << 0)
#define MQTT_FLAG_QOS_0      (0 << 1)
#define MQTT_FLAG_QOS_1      (1 << 1)
#define MQTT_FLAG_QOS_2      (2 << 1)
#define MQTT_FLAG_DUP        (1 << 3)

/* Maximum packet size and buffer lengths */
#define MQTT_MAX_PACKET_SIZE    (128*1024)
#define MQTT_MAX_TOPIC_LENGTH   256
#define MQTT_MAX_CLIENT_ID      23

/* MQTT Message */
typedef struct mqtt_message {
    uint8_t  type;
    uint8_t  qos;
    uint8_t  retain;
    uint8_t  dup;
    uint16_t packet_id;
    void    *payload;
    size_t   payloadlen;
    char    *topic;
    size_t   topiclen;
} mqtt_message_t;

/* MQTT Client Configuration */
typedef struct mqtt_config {
    char    *client_id;
    char    *username;
    char    *password;
    char    *host;
    uint16_t port;
    uint16_t keepalive;
    bool     clean_session;
    char    *will_topic;
    char    *will_message;
    uint8_t  will_qos;
    bool     will_retain;
    bool     use_ssl;
    char    *ca_cert;
    char    *client_cert;
    char    *client_key;
} mqtt_config_t;

/* MQTT Client Context */
typedef struct mqtt_client mqtt_client_t;

/* Callback function types */
typedef void (*mqtt_connect_callback)(mqtt_client_t *client, void *user_data);
typedef void (*mqtt_disconnect_callback)(mqtt_client_t *client, void *user_data);
typedef void (*mqtt_publish_callback)(mqtt_client_t *client, const char *topic, const void *payload, size_t payload_len, void *user_data);
typedef void (*mqtt_subscribe_callback)(mqtt_client_t *client, const char *topic, uint8_t qos, void *user_data);

/* API Functions */
mqtt_client_t *mqtt_client_new(void);
void mqtt_client_destroy(mqtt_client_t *client);

int mqtt_client_set_config(mqtt_client_t *client, const mqtt_config_t *config);
int mqtt_client_connect(mqtt_client_t *client);
int mqtt_client_disconnect(mqtt_client_t *client);

int mqtt_client_publish(mqtt_client_t *client, const char *topic, const void *payload, size_t payload_len, uint8_t qos, bool retain);
int mqtt_client_subscribe(mqtt_client_t *client, const char *topic, uint8_t qos);
int mqtt_client_unsubscribe(mqtt_client_t *client, const char *topic);

void mqtt_client_set_connect_callback(mqtt_client_t *client, mqtt_connect_callback cb, void *user_data);
void mqtt_client_set_disconnect_callback(mqtt_client_t *client, mqtt_disconnect_callback cb, void *user_data);
void mqtt_client_set_publish_callback(mqtt_client_t *client, mqtt_publish_callback cb, void *user_data);
void mqtt_client_set_subscribe_callback(mqtt_client_t *client, mqtt_subscribe_callback cb, void *user_data);

int mqtt_client_yield(mqtt_client_t *client, int timeout_ms);

#endif 