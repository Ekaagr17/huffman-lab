#include "decompress.h"

void bitreader_init(BitReader *br, FILE *fp) {
    if (!br) return;
    br->fp = fp;
    br->buffer = 0;
    br->bit_count = 0;
    br->eof = false;
}

int bitreader_read_bit(BitReader *br) {
    if (!br || !br->fp || br->eof) return -1;
    
    if (br->bit_count == 0) {
        int c = fgetc(br->fp);
        if (c == EOF) {
            br->eof = true;
            return -1;
        }
        br->buffer = (uint8_t)c;
        br->bit_count = 8;
    }
    
    int bit = (br->buffer >> (br->bit_count - 1)) & 1;
    br->bit_count--;
    return bit;
}

int huffman_decompress_file(const char *input_path, const char *output_path, uint64_t *original_size) {
    if (!input_path || !output_path) return HUFF_ERR_INVALID_ARG;
    
    FILE *in_fp = fopen(input_path, "rb");
    if (!in_fp) {
        return HUFF_ERR_FILE_NOT_FOUND;
    }
    
    /* 1. Read & Validate Magic "HUFF" */
    char magic[5] = {0};
    if (fread(magic, 1, 4, in_fp) != 4) {
        fclose(in_fp);
        return HUFF_ERR_INVALID_HEADER;
    }
    if (memcmp(magic, HUFF_MAGIC, 4) != 0) {
        fclose(in_fp);
        return HUFF_ERR_INVALID_HEADER;
    }
    
    /* 2. Version & Reserved */
    uint8_t ver_hdr[2];
    if (fread(ver_hdr, 1, 2, in_fp) != 2) {
        fclose(in_fp);
        return HUFF_ERR_INVALID_HEADER;
    }
    if (ver_hdr[0] != HUFF_VERSION) {
        fclose(in_fp);
        return HUFF_ERR_UNSUPPORTED_VERSION;
    }
    
    /* 3. Original file size */
    uint8_t size_buf[8];
    if (fread(size_buf, 1, 8, in_fp) != 8) {
        fclose(in_fp);
        return HUFF_ERR_INVALID_HEADER;
    }
    uint64_t orig_size = read_uint64_le(size_buf);
    if (original_size) {
        *original_size = orig_size;
    }
    
    /* 4. Unique symbols count */
    uint8_t uniq_buf[2];
    if (fread(uniq_buf, 1, 2, in_fp) != 2) {
        fclose(in_fp);
        return HUFF_ERR_INVALID_HEADER;
    }
    uint16_t unique_symbols = read_uint16_le(uniq_buf);
    if (unique_symbols > MAX_SYMBOLS) {
        fclose(in_fp);
        return HUFF_ERR_CORRUPT_DATA;
    }
    
    /* Edge case: empty file */
    if (orig_size == 0) {
        fclose(in_fp);
        FILE *out_fp = fopen(output_path, "wb");
        if (!out_fp) return HUFF_ERR_FILE_WRITE;
        fclose(out_fp);
        return HUFF_OK;
    }
    
    if (unique_symbols == 0 && orig_size > 0) {
        fclose(in_fp);
        return HUFF_ERR_CORRUPT_DATA;
    }
    
    /* 5. Read Frequency Table */
    FrequencyTable ft;
    frequency_table_init(&ft);
    ft.total_bytes = orig_size;
    ft.unique_symbols = unique_symbols;
    
    for (uint16_t i = 0; i < unique_symbols; i++) {
        uint8_t sym;
        if (fread(&sym, 1, 1, in_fp) != 1) {
            fclose(in_fp);
            return HUFF_ERR_CORRUPT_DATA;
        }
        uint8_t freq_buf[8];
        if (fread(freq_buf, 1, 8, in_fp) != 8) {
            fclose(in_fp);
            return HUFF_ERR_CORRUPT_DATA;
        }
        uint64_t freq = read_uint64_le(freq_buf);
        ft.counts[sym] = freq;
    }
    
    FILE *out_fp = fopen(output_path, "wb");
    if (!out_fp) {
        fclose(in_fp);
        return HUFF_ERR_FILE_WRITE;
    }
    
    /* Edge case: Exactly 1 unique symbol */
    if (unique_symbols == 1) {
        uint8_t single_sym = 0;
        for (int i = 0; i < MAX_SYMBOLS; i++) {
            if (ft.counts[i] > 0) {
                single_sym = (uint8_t)i;
                break;
            }
        }
        
        uint8_t chunk[CHUNK_SIZE];
        memset(chunk, single_sym, sizeof(chunk));
        uint64_t remaining = orig_size;
        
        while (remaining > 0) {
            size_t to_write = remaining > CHUNK_SIZE ? CHUNK_SIZE : (size_t)remaining;
            if (fwrite(chunk, 1, to_write, out_fp) != to_write) {
                fclose(in_fp);
                fclose(out_fp);
                return HUFF_ERR_FILE_WRITE;
            }
            remaining -= to_write;
        }
        
        fclose(in_fp);
        fclose(out_fp);
        return HUFF_OK;
    }
    
    /* General case: 2+ symbols - Rebuild tree */
    HuffmanNode *root = huffman_build_tree(&ft, NULL);
    if (!root) {
        fclose(in_fp);
        fclose(out_fp);
        return HUFF_ERR_CORRUPT_DATA;
    }
    
    BitReader br;
    bitreader_init(&br, in_fp);
    
    uint8_t out_chunk[CHUNK_SIZE];
    size_t out_idx = 0;
    uint64_t total_decoded = 0;
    
    while (total_decoded < orig_size) {
        const HuffmanNode *curr = root;
        
        while (!curr->is_leaf) {
            int bit = bitreader_read_bit(&br);
            if (bit == -1) {
                /* Unexpected EOF */
                huffman_free_tree(root);
                fclose(in_fp);
                fclose(out_fp);
                return HUFF_ERR_CORRUPT_DATA;
            }
            
            if (bit == 0) {
                curr = curr->left;
            } else {
                curr = curr->right;
            }
            
            if (!curr) {
                huffman_free_tree(root);
                fclose(in_fp);
                fclose(out_fp);
                return HUFF_ERR_CORRUPT_DATA;
            }
        }
        
        out_chunk[out_idx++] = (uint8_t)curr->symbol;
        total_decoded++;
        
        if (out_idx == CHUNK_SIZE) {
            if (fwrite(out_chunk, 1, out_idx, out_fp) != out_idx) {
                huffman_free_tree(root);
                fclose(in_fp);
                fclose(out_fp);
                return HUFF_ERR_FILE_WRITE;
            }
            out_idx = 0;
        }
    }
    
    if (out_idx > 0) {
        if (fwrite(out_chunk, 1, out_idx, out_fp) != out_idx) {
            huffman_free_tree(root);
            fclose(in_fp);
            fclose(out_fp);
            return HUFF_ERR_FILE_WRITE;
        }
    }
    
    huffman_free_tree(root);
    fclose(in_fp);
    fclose(out_fp);
    
    return HUFF_OK;
}
