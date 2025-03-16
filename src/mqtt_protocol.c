#include "mqtt.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* MQTT Fixed Header */
typedef struct {
    uint8_t type;
    uint8_t dup;
    uint8_t qos;
    uint8_t retain;
    uint32_t remaining_length;
} mqtt_fixed_header_t;

/* MQTT Connect Flags */
#define MQTT_CONNECT_FLAG_USERNAME      0x80
#define MQTT_CONNECT_FLAG_PASSWORD      0x40
#define MQTT_CONNECT_FLAG_WILL_RETAIN   0x20
#define MQTT_CONNECT_FLAG_WILL_QOS      0x18
#define MQTT_CONNECT_FLAG_WILL          0x04
#define MQTT_CONNECT_FLAG_CLEAN_SESSION 0x02

/* Encode remaining length */
static int mqtt_encode_remaining_length(uint8_t *buf, uint32_t length)
{
    int encoded = 0;
    
    do {
        uint8_t digit = length % 128;
        length = length / 128;
        if (length > 0) {
            digit |= 0x80;
        }
        buf[encoded++] = digit;
    } while (length > 0);
    
    return encoded;
}

/* Decode remaining length */
static int mqtt_decode_remaining_length(const uint8_t *buf, uint32_t *value)
{
    int decoded = 0;
    uint32_t multiplier = 1;
    uint32_t length = 0;
    uint8_t digit;
    
    do {
        if (decoded >= 4) {
            return MQTT_ERR_PROTOCOL;
        }
        digit = buf[decoded++];
        length += (digit & 127) * multiplier;
        multiplier *= 128;
    } while ((digit & 128) != 0);
    
    *value = length;
    return decoded;
}

/* Create CONNECT packet */
static int mqtt_create_connect_packet(uint8_t *buf, size_t buflen, const mqtt_config_t *config)
{
    size_t len = 0;
    uint8_t connect_flags = 0;
    uint16_t keepalive = config->keepalive;
    
    /* Fixed header */
    buf[len++] = MQTT_CONNECT << 4;
    
    /* Skip remaining length for now */
    len += 4;
    
    /* Protocol name */
    buf[len++] = 0;
    buf[len++] = 4;
    memcpy(&buf[len], "MQTT", 4);
    len += 4;
    
    /* Protocol version */
    buf[len++] = MQTT_PROTOCOL_V4;
    
    /* Connect flags */
    if (config->clean_session) {
        connect_flags |= MQTT_CONNECT_FLAG_CLEAN_SESSION;
    }
    
    if (config->username) {
        connect_flags |= MQTT_CONNECT_FLAG_USERNAME;
    }
    
    if (config->password) {
        connect_flags |= MQTT_CONNECT_FLAG_PASSWORD;
    }
    
    if (config->will_topic) {
        connect_flags |= MQTT_CONNECT_FLAG_WILL;
        connect_flags |= ((config->will_qos & 0x03) << 3);
        if (config->will_retain) {
            connect_flags |= MQTT_CONNECT_FLAG_WILL_RETAIN;
        }
    }
    
    buf[len++] = connect_flags;
    
    /* Keepalive */
    buf[len++] = keepalive >> 8;
    buf[len++] = keepalive & 0xFF;
    
    /* Client ID */
    size_t client_id_len = strlen(config->client_id);
    buf[len++] = client_id_len >> 8;
    buf[len++] = client_id_len & 0xFF;
    memcpy(&buf[len], config->client_id, client_id_len);
    len += client_id_len;
    
    /* Will topic and message */
    if (config->will_topic) {
        size_t will_topic_len = strlen(config->will_topic);
        buf[len++] = will_topic_len >> 8;
        buf[len++] = will_topic_len & 0xFF;
        memcpy(&buf[len], config->will_topic, will_topic_len);
        len += will_topic_len;
        
        size_t will_message_len = strlen(config->will_message);
        buf[len++] = will_message_len >> 8;
        buf[len++] = will_message_len & 0xFF;
        memcpy(&buf[len], config->will_message, will_message_len);
        len += will_message_len;
    }
    
    /* Username */
    if (config->username) {
        size_t username_len = strlen(config->username);
        buf[len++] = username_len >> 8;
        buf[len++] = username_len & 0xFF;
        memcpy(&buf[len], config->username, username_len);
        len += username_len;
    }
    
    /* Password */
    if (config->password) {
        size_t password_len = strlen(config->password);
        buf[len++] = password_len >> 8;
        buf[len++] = password_len & 0xFF;
        memcpy(&buf[len], config->password, password_len);
        len += password_len;
    }
    
    /* Write remaining length */
    int remaining_length_bytes = mqtt_encode_remaining_length(&buf[1], len - 5);
    memmove(&buf[1 + remaining_length_bytes], &buf[5], len - 5);
    
    return len - (4 - remaining_length_bytes);
} 