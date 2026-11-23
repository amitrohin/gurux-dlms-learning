#ifndef __CRC_H___
#define __CRC_H__ 1

#include <stddef.h>
#include <stdint.h>

extern uint16_t const crc16_table[256];

inline uint16_t crc16(uint8_t const *buf, size_t len) {
    uint16_t fcs = 0xffffU;
    while (len--)
        fcs = (fcs >> 8) ^ crc16_table[(fcs ^ *buf++) & 0xffU];
    return fcs ^ 0xffffU;
}

#endif
