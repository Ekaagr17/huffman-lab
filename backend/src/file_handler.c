#include "file_handler.h"
#include <time.h>
#include <ctype.h>

void generate_unique_id(char *out, size_t out_len) {
    if (!out || out_len < 32) return;
    static uint32_t counter = 1000;
    counter++;
    uint32_t t = (uint32_t)time(NULL);
    uint32_t r = (uint32_t)rand() ^ (counter << 8);
    snprintf(out, out_len, "huff_%08x_%08x", (unsigned int)t, (unsigned int)r);
}

void sanitize_filename(const char *input, char *output, size_t max_len) {
    if (!input || !output || max_len == 0) return;
    
    /* Find last path separator if any */
    const char *base = input;
    for (const char *p = input; *p != '\0'; p++) {
        if (*p == '/' || *p == '\\') {
            base = p + 1;
        }
    }
    
    size_t out_idx = 0;
    for (const char *p = base; *p != '\0' && out_idx + 1 < max_len; p++) {
        char c = *p;
        if (isalnum((unsigned char)c) || c == '.' || c == '_' || c == '-') {
            output[out_idx++] = c;
        } else {
            output[out_idx++] = '_';
        }
    }
    
    if (out_idx == 0) {
        strncpy(output, "unnamed_file", max_len);
        output[max_len - 1] = '\0';
    } else {
        output[out_idx] = '\0';
    }
}

bool file_exists(const char *path, uint64_t *size_out) {
    if (!path) return false;
    FILE *fp = fopen(path, "rb");
    if (!fp) return false;
    
    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return false;
    }
    
    long sz = ftell(fp);
    fclose(fp);
    
    if (sz < 0) return false;
    if (size_out) *size_out = (uint64_t)sz;
    return true;
}

bool files_match_exact(const char *path1, const char *path2) {
    if (!path1 || !path2) return false;
    
    FILE *f1 = fopen(path1, "rb");
    if (!f1) return false;
    
    FILE *f2 = fopen(path2, "rb");
    if (!f2) {
        fclose(f1);
        return false;
    }
    
    uint8_t b1[CHUNK_SIZE];
    uint8_t b2[CHUNK_SIZE];
    bool matched = true;
    
    while (1) {
        size_t n1 = fread(b1, 1, sizeof(b1), f1);
        size_t n2 = fread(b2, 1, sizeof(b2), f2);
        
        if (n1 != n2) {
            matched = false;
            break;
        }
        if (n1 == 0) {
            break; /* EOF reached together */
        }
        if (memcmp(b1, b2, n1) != 0) {
            matched = false;
            break;
        }
    }
    
    fclose(f1);
    fclose(f2);
    return matched;
}

/* 64-bit FNV-1a checksum formatted as 16-hex characters */
int file_sha256(const char *filepath, char *hex_out) {
    if (!filepath || !hex_out) return HUFF_ERR_INVALID_ARG;
    
    FILE *fp = fopen(filepath, "rb");
    if (!fp) return HUFF_ERR_FILE_NOT_FOUND;
    
    uint64_t hash = 0xcbf29ce484222325ULL;
    uint8_t buffer[CHUNK_SIZE];
    size_t bytes_read;
    
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), fp)) > 0) {
        for (size_t i = 0; i < bytes_read; i++) {
            hash ^= (uint64_t)buffer[i];
            hash *= 0x100000001b3ULL;
        }
    }
    
    fclose(fp);
    snprintf(hex_out, 65, "fnv1a_%08x%08x", (unsigned int)(hash >> 32), (unsigned int)(hash & 0xFFFFFFFF));
    return HUFF_OK;
}

bool resolve_safe_path(const char *dir, const char *filename, char *resolved_out, size_t max_len) {
    if (!dir || !filename || !resolved_out || max_len == 0) return false;
    
    char clean_name[MAX_PATH_LEN];
    sanitize_filename(filename, clean_name, sizeof(clean_name));
    
    snprintf(resolved_out, max_len, "%s/%s", dir, clean_name);
    return true;
}
