#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "common.h"
#include "frequency.h"
#include "minheap.h"
#include "tree.h"
#include "compress.h"
#include "decompress.h"
#include "file_handler.h"

static int g_tests_run = 0;
static int g_tests_passed = 0;

#define TEST_ASSERT(cond, msg) do { \
    g_tests_run++; \
    if (cond) { \
        g_tests_passed++; \
        printf("  [PASS] %s\n", msg); \
    } else { \
        printf("  [FAIL] %s (Line %d)\n", msg, __LINE__); \
    } \
} while(0)

static void create_test_file(const char *path, const uint8_t *data, size_t size) {
    FILE *fp = fopen(path, "wb");
    assert(fp != NULL);
    if (size > 0 && data != NULL) {
        fwrite(data, 1, size, fp);
    }
    fclose(fp);
}

static void run_roundtrip_test(const char *test_name, const uint8_t *data, size_t size) {
    printf("\n=== Running Test: %s (Size: %lu bytes) ===\n", test_name, (unsigned long)size);
    
    char src_file[MAX_PATH_LEN];
    char huff_file[MAX_PATH_LEN];
    char dec_file[MAX_PATH_LEN];
    
    snprintf(src_file, sizeof(src_file), "test_src_%s.tmp", test_name);
    snprintf(huff_file, sizeof(huff_file), "test_%s.huff", test_name);
    snprintf(dec_file, sizeof(dec_file), "test_dec_%s.tmp", test_name);
    
    create_test_file(src_file, data, size);
    
    CompressionStats stats;
    cJSON *tree_json = NULL;
    int comp_res = huffman_compress_file(src_file, huff_file, &stats, &tree_json);
    TEST_ASSERT(comp_res == HUFF_OK, "Compression completed successfully");
    TEST_ASSERT(stats.original_size == size, "Original size matches input");
    
    if (tree_json) {
        cJSON_Delete(tree_json);
    }
    
    uint64_t recovered_size = 0;
    int dec_res = huffman_decompress_file(huff_file, dec_file, &recovered_size);
    TEST_ASSERT(dec_res == HUFF_OK, "Decompression completed successfully");
    TEST_ASSERT(recovered_size == size, "Recovered size matches original");
    
    bool match = files_match_exact(src_file, dec_file);
    TEST_ASSERT(match, "Recovered file is 100% BYTE-FOR-BYTE IDENTICAL");
    
    char hash_src[65], hash_dec[65];
    file_sha256(src_file, hash_src);
    file_sha256(dec_file, hash_dec);
    TEST_ASSERT(strcmp(hash_src, hash_dec) == 0, "Checksum of original and recovered match");
    
    printf("  Original: %lu B -> Compressed: %lu B (Saved: %.2f%%)\n",
           (unsigned long)stats.original_size, (unsigned long)stats.compressed_size, stats.space_saved_percent);
    
    remove(src_file);
    remove(huff_file);
    remove(dec_file);
}

static void test_banana() {
    const char *text = "BANANA";
    run_roundtrip_test("banana", (const uint8_t*)text, strlen(text));
}

static void test_repeated_characters() {
    size_t sz = 2048;
    uint8_t *data = (uint8_t*)malloc(sz);
    memset(data, 'A', sz);
    run_roundtrip_test("repeated_A", data, sz);
    free(data);
}

static void test_empty_file() {
    run_roundtrip_test("empty", NULL, 0);
}

static void test_single_byte() {
    uint8_t b = 'Z';
    run_roundtrip_test("single_byte", &b, 1);
}

static void test_all_256_bytes() {
    uint8_t data[256];
    for (int i = 0; i < 256; i++) {
        data[i] = (uint8_t)i;
    }
    run_roundtrip_test("all_256_bytes", data, 256);
}

static void test_binary_file() {
    size_t sz = 16384;
    uint8_t *data = (uint8_t*)malloc(sz);
    /* Simulated PDF/binary header and content */
    memcpy(data, "%PDF-1.7\n", 9);
    for (size_t i = 9; i < sz; i++) {
        data[i] = (uint8_t)((i * 37 + 13) % 256);
    }
    run_roundtrip_test("binary_pdf", data, sz);
    free(data);
}

static void test_large_file() {
    size_t sz = 250000; /* 250 KB */
    uint8_t *data = (uint8_t*)malloc(sz);
    for (size_t i = 0; i < sz; i++) {
        /* English-like distribution pattern with repeated words */
        data[i] = (uint8_t)("Huffman coding is an optimal prefix code invented by David Huffman. "[i % 68]);
    }
    run_roundtrip_test("large_text", data, sz);
    free(data);
}

static void test_corrupted_file() {
    printf("\n=== Running Test: Corrupted & Invalid Header Handling ===\n");
    const char *bad_file = "test_bad.huff";
    const char *out_file = "test_bad.out";
    
    /* 1. Invalid Magic */
    uint8_t bad_magic[16] = { 'F', 'A', 'K', 'E', 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    create_test_file(bad_file, bad_magic, sizeof(bad_magic));
    uint64_t sz = 0;
    int err = huffman_decompress_file(bad_file, out_file, &sz);
    TEST_ASSERT(err == HUFF_ERR_INVALID_HEADER, "Invalid magic is rejected");
    
    /* 2. Truncated Header */
    uint8_t trunc[3] = { 'H', 'U', 'F' };
    create_test_file(bad_file, trunc, sizeof(trunc));
    err = huffman_decompress_file(bad_file, out_file, &sz);
    TEST_ASSERT(err == HUFF_ERR_INVALID_HEADER, "Truncated header is rejected");
    
    /* 3. Unsupported Version */
    uint8_t bad_ver[16] = { 'H', 'U', 'F', 'F', 99, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    create_test_file(bad_file, bad_ver, sizeof(bad_ver));
    err = huffman_decompress_file(bad_file, out_file, &sz);
    TEST_ASSERT(err == HUFF_ERR_UNSUPPORTED_VERSION, "Unsupported version is rejected");
    
    remove(bad_file);
    remove(out_file);
}

int main() {
    printf("====================================================\n");
    printf("        HUFFMAN LAB - CORE C TEST SUITE             \n");
    printf("====================================================\n");
    
    test_banana();
    test_repeated_characters();
    test_empty_file();
    test_single_byte();
    test_all_256_bytes();
    test_binary_file();
    test_large_file();
    test_corrupted_file();
    
    printf("\n====================================================\n");
    printf("SUMMARY: %d / %d TESTS PASSED\n", g_tests_passed, g_tests_run);
    printf("====================================================\n");
    
    return (g_tests_passed == g_tests_run) ? 0 : 1;
}
