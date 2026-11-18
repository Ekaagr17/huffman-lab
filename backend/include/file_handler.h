#ifndef FILE_HANDLER_H
#define FILE_HANDLER_H

#include "common.h"

#define MAX_UPLOAD_SIZE (100 * 1024 * 1024) /* 100 MB max */

/* Generates safe unique alphanumeric ID */
void generate_unique_id(char *out, size_t out_len);

/* Sanitizes an untrusted filename, stripping path components and dangerous chars */
void sanitize_filename(const char *input, char *output, size_t max_len);

/* Checks if a file exists and gets its size */
bool file_exists(const char *path, uint64_t *size_out);

/* Verifies that two files are byte-for-byte identical */
bool files_match_exact(const char *path1, const char *path2);

/* Calculates SHA-256 hex string (65 bytes with null terminator) */
int file_sha256(const char *filepath, char *hex_out);

/* Helper to ensure safe path within allowed directory */
bool resolve_safe_path(const char *dir, const char *filename, char *resolved_out, size_t max_len);

#endif /* FILE_HANDLER_H */
