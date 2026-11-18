#ifndef SERVER_H
#define SERVER_H

#include "common.h"
#include <time.h>
#include "civetweb.h"
#include "cJSON.h"
#include "compress.h"
#include "decompress.h"
#include "file_handler.h"

typedef struct {
    char id[MAX_ID_LEN];
    char original_filename[MAX_PATH_LEN];
    char upload_path[MAX_PATH_LEN];
    char compressed_path[MAX_PATH_LEN];
    char decompressed_path[MAX_PATH_LEN];
    char original_hash[65];
    char decompressed_hash[65];
    uint64_t original_size;
    uint64_t compressed_size;
    uint64_t decompressed_size;
    double compression_ratio;
    double space_saved_percent;
    int64_t space_saved_bytes;
    cJSON *tree_json;
    FrequencyTable ft;
    bool is_compressed;
    bool is_decompressed;
    time_t created_at;
} FileRecord;

int server_start(int port, const char *document_root);
void server_stop(void);

FileRecord* file_record_create(const char *filename);
FileRecord* file_record_find(const char *id);

#endif /* SERVER_H */
