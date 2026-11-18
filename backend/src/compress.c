#include "compress.h"

void bitwriter_init(BitWriter *bw, FILE *fp) {
    if (!bw) return;
    bw->fp = fp;
    bw->buffer = 0;
    bw->bit_count = 0;
    bw->bytes_written = 0;
}

void bitwriter_write_bit(BitWriter *bw, int bit) {
    if (!bw || !bw->fp) return;
    
    bw->buffer = (uint8_t)((bw->buffer << 1) | (bit & 1));
    bw->bit_count++;
    
    if (bw->bit_count == 8) {
        fputc(bw->buffer, bw->fp);
        bw->buffer = 0;
        bw->bit_count = 0;
        bw->bytes_written++;
    }
}

void bitwriter_write_string(BitWriter *bw, const char *code) {
    if (!bw || !code) return;
    for (size_t i = 0; code[i] != '\0'; i++) {
        bitwriter_write_bit(bw, code[i] == '1' ? 1 : 0);
    }
}

void bitwriter_flush(BitWriter *bw) {
    if (!bw || !bw->fp) return;
    if (bw->bit_count > 0) {
        bw->buffer <<= (8 - bw->bit_count);
        fputc(bw->buffer, bw->fp);
        bw->buffer = 0;
        bw->bit_count = 0;
        bw->bytes_written++;
    }
}

static void traverse_code_table(const HuffmanNode *node, char *current_code, int depth, HuffmanCodeTable *table) {
    if (!node) return;
    
    if (node->is_leaf) {
        current_code[depth] = '\0';
        uint16_t sym = node->symbol;
        if (sym < MAX_SYMBOLS) {
            if (depth == 0) {
                /* Single symbol edge case: assign "0" */
                table->entries[sym].code[0] = '0';
                table->entries[sym].code[1] = '\0';
                table->entries[sym].length = 1;
            } else {
                strncpy(table->entries[sym].code, current_code, MAX_SYMBOLS);
                table->entries[sym].code[MAX_SYMBOLS] = '\0';
                table->entries[sym].length = (uint16_t)depth;
            }
        }
        return;
    }
    
    current_code[depth] = '0';
    traverse_code_table(node->left, current_code, depth + 1, table);
    
    current_code[depth] = '1';
    traverse_code_table(node->right, current_code, depth + 1, table);
}

void huffman_build_code_table(const HuffmanNode *root, HuffmanCodeTable *table) {
    if (!table) return;
    memset(table, 0, sizeof(HuffmanCodeTable));
    if (!root) return;
    
    char prefix[MAX_SYMBOLS + 1];
    traverse_code_table(root, prefix, 0, table);
}

int huffman_compress_file(const char *input_path, const char *output_path,
                          CompressionStats *stats, cJSON **tree_json) {
    if (!input_path || !output_path) return HUFF_ERR_INVALID_ARG;
    
    FrequencyTable ft;
    int err = frequency_count_file(input_path, &ft);
    if (err != HUFF_OK) {
        return err;
    }
    
    ConstructionHistory history;
    construction_history_init(&history);
    
    HuffmanNode *root = huffman_build_tree(&ft, &history);
    
    HuffmanCodeTable code_table;
    huffman_build_code_table(root, &code_table);
    
    if (tree_json) {
        *tree_json = huffman_tree_to_json(root, &history, &ft);
    }
    
    FILE *out_fp = fopen(output_path, "wb");
    if (!out_fp) {
        huffman_free_tree(root);
        construction_history_free(&history);
        return HUFF_ERR_FILE_WRITE;
    }
    
    /* 1. Write Magic "HUFF" */
    if (fwrite(HUFF_MAGIC, 1, 4, out_fp) != 4) {
        fclose(out_fp);
        huffman_free_tree(root);
        construction_history_free(&history);
        return HUFF_ERR_FILE_WRITE;
    }
    
    /* 2. Version and Reserved byte */
    uint8_t ver_hdr[2] = { HUFF_VERSION, 0x00 };
    fwrite(ver_hdr, 1, 2, out_fp);
    
    /* 3. Original file size (8 bytes, little-endian) */
    uint8_t size_buf[8];
    write_uint64_le(size_buf, ft.total_bytes);
    fwrite(size_buf, 1, 8, out_fp);
    
    /* 4. Unique symbol count (2 bytes, little-endian) */
    uint8_t uniq_buf[2];
    write_uint16_le(uniq_buf, ft.unique_symbols);
    fwrite(uniq_buf, 1, 2, out_fp);
    
    /* 5. Frequency table */
    for (int i = 0; i < MAX_SYMBOLS; i++) {
        if (ft.counts[i] > 0) {
            uint8_t sym = (uint8_t)i;
            fwrite(&sym, 1, 1, out_fp);
            uint8_t freq_buf[8];
            write_uint64_le(freq_buf, ft.counts[i]);
            fwrite(freq_buf, 1, 8, out_fp);
        }
    }
    
    /* 6. Encode bitstream */
    if (ft.total_bytes > 0 && ft.unique_symbols > 0) {
        FILE *in_fp = fopen(input_path, "rb");
        if (!in_fp) {
            fclose(out_fp);
            huffman_free_tree(root);
            construction_history_free(&history);
            return HUFF_ERR_FILE_READ;
        }
        
        BitWriter bw;
        bitwriter_init(&bw, out_fp);
        
        uint8_t chunk[CHUNK_SIZE];
        size_t read_bytes;
        
        while ((read_bytes = fread(chunk, 1, sizeof(chunk), in_fp)) > 0) {
            for (size_t i = 0; i < read_bytes; i++) {
                uint8_t b = chunk[i];
                bitwriter_write_string(&bw, code_table.entries[b].code);
            }
        }
        
        bitwriter_flush(&bw);
        fclose(in_fp);
    }
    
    fflush(out_fp);
    long out_size = ftell(out_fp);
    fclose(out_fp);
    
    if (stats) {
        stats->original_size = ft.total_bytes;
        stats->compressed_size = (uint64_t)(out_size < 0 ? 0 : out_size);
        if (stats->original_size > 0) {
            stats->compression_ratio = (double)stats->compressed_size / (double)stats->original_size;
            stats->space_saved_bytes = (int64_t)stats->original_size - (int64_t)stats->compressed_size;
            stats->space_saved_percent = ((double)stats->space_saved_bytes / (double)stats->original_size) * 100.0;
        } else {
            stats->compression_ratio = 1.0;
            stats->space_saved_bytes = 0;
            stats->space_saved_percent = 0.0;
        }
    }
    
    huffman_free_tree(root);
    construction_history_free(&history);
    return HUFF_OK;
}
