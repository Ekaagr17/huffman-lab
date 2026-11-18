#ifndef DECOMPRESS_H
#define DECOMPRESS_H

#include "common.h"
#include "frequency.h"
#include "tree.h"

typedef struct {
    FILE *fp;
    uint8_t buffer;
    uint8_t bit_count;
    bool eof;
} BitReader;

void bitreader_init(BitReader *br, FILE *fp);
int bitreader_read_bit(BitReader *br);

int huffman_decompress_file(const char *input_path, const char *output_path, uint64_t *original_size);

#endif /* DECOMPRESS_H */
