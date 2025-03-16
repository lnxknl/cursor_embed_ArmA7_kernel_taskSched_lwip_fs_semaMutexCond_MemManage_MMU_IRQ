#include "websocket.h"
#include <string.h>
#include <arpa/inet.h>

static void ws_mask_payload(uint8_t *payload, size_t len, const uint8_t *mask)
{
    for (size_t i = 0; i < len; i++) {
        payload[i] ^= mask[i % 4];
    }
}

int ws_parse_frame_header(const uint8_t *data, size_t len, ws_frame_header_t *header)
{
    if (len < 2) return -1;

    // 解析基本头部
    header->fin = (data[0] >> 7) & 0x01;
    header->rsv1 = (data[0] >> 6) & 0x01;
    header->rsv2 = (data[0] >> 5) & 0x01;
    header->rsv3 = (data[0] >> 4) & 0x01;
    header->opcode = data[0] & 0x0F;
    header->mask = (data[1] >> 7) & 0x01;
    header->payload_len = data[1] & 0x7F;

    size_t header_size = 2;

    // 解析扩展长度
    if (header->payload_len == 126) {
        if (len < 4) return -1;
        uint16_t length;
        memcpy(&length, data + 2, 2);
        header->payload_length = ntohs(length);
        header_size += 2;
    } else if (header->payload_len == 127) {
        if (len < 10) return -1;
        uint64_t length;
        memcpy(&length, data + 2, 8);
        header->payload_length = be64toh(length);
        header_size += 8;
    } else {
        header->payload_length = header->payload_len;
    }

    // 解析掩码
    if (header->mask) {
        if (len < header_size + 4) return -1;
        memcpy(header->masking_key, data + header_size, 4);
        header_size += 4;
    }

    return header_size;
}

int ws_build_frame(uint8_t *buffer, size_t buffer_size,
                  int opcode, const void *data, size_t len,
                  bool mask, const uint8_t *masking_key)
{
    size_t header_size = 2;
    
    // 计算头部大小
    if (len <= 125) {
        if (buffer_size < 2) return -1;
    } else if (len <= 65535) {
        if (buffer_size < 4) return -1;
        header_size += 2;
    } else {
        if (buffer_size < 10) return -1;
        header_size += 8;
    }
    
    if (mask) {
        header_size += 4;
    }
    
    if (buffer_size < header_size + len) return -1;

    // 设置基本头部
    buffer[0] = 0x80 | (opcode & 0x0F);  // FIN + opcode
    
    // 设置长度
    if (len <= 125) {
        buffer[1] = mask ? 0x80 | len : len;
    } else if (len <= 65535) {
        buffer[1] = mask ? 0xFE : 0x7E;  // 126
        uint16_t net_len = htons(len);
        memcpy(buffer + 2, &net_len, 2);
    } else {
        buffer[1] = mask ? 0xFF : 0x7F;  // 127
        uint64_t net_len = htobe64(len);
        memcpy(buffer + 2, &net_len, 8);
    }

    // 设置掩码
    if (mask) {
        memcpy(buffer + header_size - 4, masking_key, 4);
        memcpy(buffer + header_size, data, len);
        ws_mask_payload(buffer + header_size, len, masking_key);
    } else {
        memcpy(buffer + header_size, data, len);
    }

    return header_size + len;
} 