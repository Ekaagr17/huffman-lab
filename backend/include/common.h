#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#define HUFF_MAGIC "HUFF"
#define HUFF_VERSION 1
#define MAX_SYMBOLS 256
#define CHUNK_SIZE 65536
#define MAX_PATH_LEN 512
#define MAX_ID_LEN 64

/* Error codes */
#define HUFF_OK 0
#define HUFF_ERR_FILE_NOT_FOUND -1
#define HUFF_ERR_FILE_READ -2
#define HUFF_ERR_FILE_WRITE -3
#define HUFF_ERR_OUT_OF_MEMORY -4
#define HUFF_ERR_INVALID_HEADER -5
#define HUFF_ERR_CORRUPT_DATA -6
#define HUFF_ERR_UNSUPPORTED_VERSION -7
#define HUFF_ERR_INVALID_ARG -8

/* Serialization helpers (Little-Endian) */
static inline void write_uint16_le(uint8_t *buf, uint16_t val) {
    buf[0] = (uint8_t)(val & 0xFF);
    buf[1] = (uint8_t)((val >> 8) & 0xFF);
}

static inline uint16_t read_uint16_le(const uint8_t *buf) {
    return (uint16_t)buf[0] | ((uint16_t)buf[1] << 8);
}

static inline void write_uint32_le(uint8_t *buf, uint32_t val) {
    buf[0] = (uint8_t)(val & 0xFF);
    buf[1] = (uint8_t)((val >> 8) & 0xFF);
    buf[2] = (uint8_t)((val >> 16) & 0xFF);
    buf[3] = (uint8_t)((val >> 24) & 0xFF);
}

static inline uint32_t read_uint32_le(const uint8_t *buf) {
    return (uint32_t)buf[0] |
           ((uint32_t)buf[1] << 8) |
           ((uint32_t)buf[2] << 16) |
           ((uint32_t)buf[3] << 24);
}

static inline void write_uint64_le(uint8_t *buf, uint64_t val) {
    for (int i = 0; i < 8; i++) {
        buf[i] = (uint8_t)((val >> (i * 8)) & 0xFF);
    }
}

static inline uint64_t read_uint64_le(const uint8_t *buf) {
    uint64_t val = 0;
    for (int i = 0; i < 8; i++) {
        val |= ((uint64_t)buf[i]) << (i * 8);
    }
    return val;
}

#endif /* COMMON_H */
