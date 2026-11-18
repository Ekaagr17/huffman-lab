#include "frequency.h"

void frequency_table_init(FrequencyTable *ft) {
    if (!ft) return;
    memset(ft->counts, 0, sizeof(ft->counts));
    ft->total_bytes = 0;
    ft->unique_symbols = 0;
}

int frequency_count_file(const char *filepath, FrequencyTable *ft) {
    if (!filepath || !ft) return HUFF_ERR_INVALID_ARG;
    
    frequency_table_init(ft);
    
    FILE *fp = fopen(filepath, "rb");
    if (!fp) {
        return HUFF_ERR_FILE_NOT_FOUND;
    }
    
    uint8_t buffer[CHUNK_SIZE];
    size_t bytes_read;
    
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), fp)) > 0) {
        for (size_t i = 0; i < bytes_read; i++) {
            ft->counts[buffer[i]]++;
        }
        ft->total_bytes += bytes_read;
    }
    
    if (ferror(fp)) {
        fclose(fp);
        return HUFF_ERR_FILE_READ;
    }
    
    fclose(fp);
    
    uint16_t unique = 0;
    for (int i = 0; i < MAX_SYMBOLS; i++) {
        if (ft->counts[i] > 0) {
            unique++;
        }
    }
    ft->unique_symbols = unique;
    
    return HUFF_OK;
}

int frequency_count_buffer(const uint8_t *data, size_t size, FrequencyTable *ft) {
    if (!ft) return HUFF_ERR_INVALID_ARG;
    
    frequency_table_init(ft);
    if (!data || size == 0) return HUFF_OK;
    
    for (size_t i = 0; i < size; i++) {
        ft->counts[data[i]]++;
    }
    ft->total_bytes = (uint64_t)size;
    
    uint16_t unique = 0;
    for (int i = 0; i < MAX_SYMBOLS; i++) {
        if (ft->counts[i] > 0) {
            unique++;
        }
    }
    ft->unique_symbols = unique;
    
    return HUFF_OK;
}
