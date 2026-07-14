#include "crc32.h"

#define CRC32_POLY      0xEDB88320u
#define CRC32_INIT      0xFFFFFFFFu

static uint32_t crc32_byte(uint32_t crc, uint8_t byte)
{
    crc ^= byte;
    for (int j = 0; j < 8; j++) {
        uint32_t mask = -(crc & 1u);
        crc = (crc >> 1) ^ (CRC32_POLY & mask);
    }
    return crc;
}

uint32_t CRC32_Compute(const uint8_t *data, uint32_t len)
{
    uint32_t crc = CRC32_INIT;
    for (uint32_t i = 0; i < len; i++) {
        crc = crc32_byte(crc, data[i]);
    }
    return ~crc;
}

uint32_t CRC32_ComputeWithSkip(const uint8_t *data, uint32_t len, uint32_t skip_offset)
{
    uint32_t crc = CRC32_INIT;
    for (uint32_t i = 0; i < len; i++) {
        uint8_t byte;
        if (i >= skip_offset && i < skip_offset + 4u) {
            byte = 0;
        } else {
            byte = data[i];
        }
        crc = crc32_byte(crc, byte);
    }
    return ~crc;
}