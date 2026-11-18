#ifndef COMPRESS_H
#define COMPRESS_H

#include "common.h"
#include "frequency.h"
#include "tree.h"
#include "cJSON.h"

typedef struct {
    char code[MAX_SYMBOLS + 1];
    uint16_t length;
} HuffmanCodeEntry;

typedef struct {
    HuffmanCodeEntry entries[MAX_SYMBOLS];
} HuffmanCodeTable;

typedef struct {
    uint64_t original_size;
    uint64_t compressed_size;
    double compression_ratio;
    double space_saved_percent;
    int64_t space_saved_bytes;
} CompressionStats;

typedef struct {
    FILE *fp;
    uint8_t buffer;
    uint8_t bit_count;
    uint64_t bytes_written;
} BitWriter;

void bitwriter_init(BitWriter *bw, FILE *fp);
void bitwriter_write_bit(BitWriter *bw, int bit);
void bitwriter_write_string(BitWriter *bw, const char *code);
void bitwriter_flush(BitWriter *bw);

void huffman_build_code_table(const HuffmanNode *root, HuffmanCodeTable *table);

int huffman_compress_file(const char *input_path, const char *output_path,
                          CompressionStats *stats, cJSON **tree_json);

#endif /* COMPRESS_H */
