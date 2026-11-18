#ifndef FREQUENCY_H
#define FREQUENCY_H

#include "common.h"

typedef struct {
    uint64_t counts[MAX_SYMBOLS];
    uint64_t total_bytes;
    uint16_t unique_symbols;
} FrequencyTable;

void frequency_table_init(FrequencyTable *ft);
int frequency_count_file(const char *filepath, FrequencyTable *ft);
int frequency_count_buffer(const uint8_t *data, size_t size, FrequencyTable *ft);

#endif /* FREQUENCY_H */
