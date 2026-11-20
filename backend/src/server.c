#include "server.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_RECORDS 256
static FileRecord g_records[MAX_RECORDS];
static size_t g_record_count = 0;
static struct mg_context *g_ctx = NULL;

FileRecord* file_record_create(const char *filename) {
    if (g_record_count >= MAX_RECORDS) {
        /* Recycle oldest record */
        if (g_records[0].tree_json) {
            cJSON_Delete(g_records[0].tree_json);
        }
        memmove(&g_records[0], &g_records[1], sizeof(FileRecord) * (MAX_RECORDS - 1));
        g_record_count = MAX_RECORDS - 1;
    }
    
    FileRecord *rec = &g_records[g_record_count++];
    memset(rec, 0, sizeof(FileRecord));
    generate_unique_id(rec->id, sizeof(rec->id));
    sanitize_filename(filename, rec->original_filename, sizeof(rec->original_filename));
    rec->created_at = time(NULL);
    return rec;
}

FileRecord* file_record_find(const char *id) {
    if (!id || strlen(id) == 0) return NULL;
    for (size_t i = 0; i < g_record_count; i++) {
        if (strcmp(g_records[i].id, id) == 0) {
            return &g_records[i];
        }
    }
    return NULL;
}

static void send_json_response(struct mg_connection *conn, int status_code, cJSON *json) {
    char *json_str = cJSON_PrintUnformatted(json);
    size_t len = json_str ? strlen(json_str) : 0;
    
    const char *status_text = "OK";
    if (status_code == 400) status_text = "Bad Request";
    else if (status_code == 404) status_text = "Not Found";
    else if (status_code == 500) status_text = "Internal Server Error";
    
    mg_printf(conn,
              "HTTP/1.1 %d %s\r\n"
              "Content-Type: application/json; charset=utf-8\r\n"
              "Content-Length: %lu\r\n"
              "Access-Control-Allow-Origin: *\r\n"
              "Access-Control-Allow-Private-Network: true\r\n"
              "Access-Control-Allow-Private-Network: true\r\n"
              "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
              "Access-Control-Allow-Headers: *\r\n"
              "Connection: close\r\n\r\n",
              status_code, status_text, (unsigned long)len);
              
    if (json_str) {
        mg_write(conn, json_str, len);
        free(json_str);
    }
}

static void send_json_error(struct mg_connection *conn, int status_code, const char *msg) {
    cJSON *err_obj = cJSON_CreateObject();
    cJSON_AddBoolToObject(err_obj, "success", false);
    cJSON_AddStringToObject(err_obj, "error", msg ? msg : "An error occurred");
    send_json_response(conn, status_code, err_obj);
    cJSON_Delete(err_obj);
}

/* Helper to extract ID from URI /api/<route>/<id> or query string ?id=<id> */
static void extract_id_from_request(struct mg_connection *conn, const char *route_prefix, char *out_id, size_t max_len) {
    out_id[0] = '\0';
    const struct mg_request_info *ri = mg_get_request_info(conn);
    if (!ri) return;
    
    /* 1. Check path suffix */
    if (ri->local_uri && strncmp(ri->local_uri, route_prefix, strlen(route_prefix)) == 0) {
        const char *p = ri->local_uri + strlen(route_prefix);
        if (*p == '/') p++;
        if (strlen(p) > 0) {
            /* Stop at any query string or slash */
            size_t i = 0;
            while (p[i] && p[i] != '?' && p[i] != '/' && i + 1 < max_len) {
                out_id[i] = p[i];
                i++;
            }
            out_id[i] = '\0';
            if (strlen(out_id) > 0) return;
        }
    }
    
    /* 2. Check query string */
    if (ri->query_string) {
        mg_get_var(ri->query_string, strlen(ri->query_string), "id", out_id, max_len);
    }
}

/* Upload context for civetweb multipart handler */
struct UploadCtx {
    char original_filename[MAX_PATH_LEN];
    char stored_path[MAX_PATH_LEN];
    char file_id[MAX_ID_LEN];
    bool has_file;
};

static int on_field_found(const char *key, const char *filename, char *path, size_t pathlen, void *user_data) {
    struct UploadCtx *ctx = (struct UploadCtx*)user_data;
    if (filename && strlen(filename) > 0) {
        ctx->has_file = true;
        sanitize_filename(filename, ctx->original_filename, sizeof(ctx->original_filename));
        generate_unique_id(ctx->file_id, sizeof(ctx->file_id));
        snprintf(path, pathlen, "uploads/%s_%s", ctx->file_id, ctx->original_filename);
        strncpy(ctx->stored_path, path, sizeof(ctx->stored_path));
        return MG_FORM_FIELD_STORAGE_STORE;
    }
    return MG_FORM_FIELD_STORAGE_GET;
}

static int on_field_get(const char *key, const char *value, size_t valuelen, void *user_data) {
    (void)key; (void)value; (void)valuelen; (void)user_data;
    return MG_FORM_FIELD_HANDLE_NEXT;
}

static int on_field_store(const char *path, long long file_size, void *user_data) {
    (void)path; (void)file_size; (void)user_data;
    return MG_FORM_FIELD_HANDLE_NEXT;
}

/* POST /api/analyze */
static int handle_analyze(struct mg_connection *conn, void *cbdata) {
    (void)cbdata;
    const struct mg_request_info *ri = mg_get_request_info(conn);
    
    if (strcmp(ri->request_method, "OPTIONS") == 0) {
        mg_printf(conn,
                  "HTTP/1.1 200 OK\r\n"
                  "Access-Control-Allow-Origin: *\r\n"
              "Access-Control-Allow-Private-Network: true\r\n"
                  "Access-Control-Allow-Private-Network: true\r\n"
                  "Access-Control-Allow-Methods: POST, OPTIONS\r\n"
                  "Access-Control-Allow-Headers: *\r\n"
                  "Content-Length: 0\r\n\r\n");
        return 200;
    }
    
    if (strcmp(ri->request_method, "POST") != 0) {
        send_json_error(conn, 400, "Method not allowed. Use POST.");
        return 400;
    }
    
    struct UploadCtx uctx;
    memset(&uctx, 0, sizeof(uctx));
    
    struct mg_form_data_handler fdh = {
        on_field_found,
        on_field_get,
        on_field_store,
        &uctx
    };
    
    int files_handled = mg_handle_form_request(conn, &fdh);
    if (files_handled <= 0 || !uctx.has_file) {
        send_json_error(conn, 400, "No file uploaded or invalid multipart form data.");
        return 400;
    }
    
    FileRecord *rec = file_record_create(uctx.original_filename);
    strncpy(rec->upload_path, uctx.stored_path, sizeof(rec->upload_path));
    file_sha256(rec->upload_path, rec->original_hash);
    
    int err = frequency_count_file(rec->upload_path, &rec->ft);
    if (err != HUFF_OK) {
        send_json_error(conn, 500, "Failed to analyze file frequencies.");
        return 500;
    }
    
    rec->original_size = rec->ft.total_bytes;
    
    /* Pre-build Huffman tree and JSON */
    ConstructionHistory history;
    construction_history_init(&history);
    HuffmanNode *root = huffman_build_tree(&rec->ft, &history);
    rec->tree_json = huffman_tree_to_json(root, &history, &rec->ft);
    huffman_free_tree(root);
    construction_history_free(&history);
    
    cJSON *res = cJSON_CreateObject();
    cJSON_AddBoolToObject(res, "success", true);
    cJSON_AddStringToObject(res, "id", rec->id);
    cJSON_AddStringToObject(res, "filename", rec->original_filename);
    cJSON_AddNumberToObject(res, "original_size", (double)rec->original_size);
    cJSON_AddNumberToObject(res, "unique_symbols", rec->ft.unique_symbols);
    cJSON_AddStringToObject(res, "hash", rec->original_hash);
    
    /* Frequency array sorted descending by count */
    cJSON *freq_arr = cJSON_CreateArray();
    
    /* Sort frequencies for reporting */
    typedef struct { uint8_t sym; uint64_t count; } FreqEntry;
    FreqEntry entries[MAX_SYMBOLS];
    int count_entries = 0;
    for (int i = 0; i < MAX_SYMBOLS; i++) {
        if (rec->ft.counts[i] > 0) {
            entries[count_entries].sym = (uint8_t)i;
            entries[count_entries].count = rec->ft.counts[i];
            count_entries++;
        }
    }
    for (int i = 0; i < count_entries - 1; i++) {
        for (int j = i + 1; j < count_entries; j++) {
            if (entries[j].count > entries[i].count ||
               (entries[j].count == entries[i].count && entries[j].sym < entries[i].sym)) {
                FreqEntry tmp = entries[i];
                entries[i] = entries[j];
                entries[j] = tmp;
            }
        }
    }
    
    for (int i = 0; i < count_entries; i++) {
        cJSON *item = cJSON_CreateObject();
        cJSON_AddNumberToObject(item, "symbol", entries[i].sym);
        cJSON_AddNumberToObject(item, "count", (double)entries[i].count);
        double pct = rec->ft.total_bytes > 0 ? ((double)entries[i].count / (double)rec->ft.total_bytes) * 100.0 : 0.0;
        cJSON_AddNumberToObject(item, "percentage", pct);
        
        char lbl[32];
        uint8_t b = entries[i].sym;
        if (b == ' ') snprintf(lbl, sizeof(lbl), "[SPACE]");
        else if (b == '\n') snprintf(lbl, sizeof(lbl), "[LF]");
        else if (b == '\r') snprintf(lbl, sizeof(lbl), "[CR]");
        else if (b == '\t') snprintf(lbl, sizeof(lbl), "[TAB]");
        else if (b >= 33 && b <= 126) snprintf(lbl, sizeof(lbl), "%c", (char)b);
        else snprintf(lbl, sizeof(lbl), "0x%02X", b);
        
        cJSON_AddStringToObject(item, "label", lbl);
        cJSON_AddItemToArray(freq_arr, item);
    }
    cJSON_AddItemToObject(res, "frequencies", freq_arr);
    
    send_json_response(conn, 200, res);
    cJSON_Delete(res);
    return 200;
}

/* POST /api/compress */
static int handle_compress(struct mg_connection *conn, void *cbdata) {
    (void)cbdata;
    const struct mg_request_info *ri = mg_get_request_info(conn);
    
    if (strcmp(ri->request_method, "OPTIONS") == 0) {
        mg_printf(conn,
                  "HTTP/1.1 200 OK\r\n"
                  "Access-Control-Allow-Origin: *\r\n"
              "Access-Control-Allow-Private-Network: true\r\n"
                  "Access-Control-Allow-Private-Network: true\r\n"
                  "Access-Control-Allow-Methods: POST, OPTIONS\r\n"
                  "Access-Control-Allow-Headers: *\r\n"
                  "Content-Length: 0\r\n\r\n");
        return 200;
    }
    
    if (strcmp(ri->request_method, "POST") != 0) {
        send_json_error(conn, 400, "Method not allowed. Use POST.");
        return 400;
    }
    
    /* Check if this is a multipart upload OR JSON { "id": "..." } */
    const char *content_type = mg_get_header(conn, "Content-Type");
    FileRecord *rec = NULL;
    
    if (content_type && strstr(content_type, "multipart/form-data")) {
        struct UploadCtx uctx;
        memset(&uctx, 0, sizeof(uctx));
        
        struct mg_form_data_handler fdh = {
            on_field_found,
            on_field_get,
            on_field_store,
            &uctx
        };
        
        int files_handled = mg_handle_form_request(conn, &fdh);
        if (files_handled <= 0 || !uctx.has_file) {
            send_json_error(conn, 400, "No file uploaded.");
            return 400;
        }
        
        rec = file_record_create(uctx.original_filename);
        strncpy(rec->upload_path, uctx.stored_path, sizeof(rec->upload_path));
        file_sha256(rec->upload_path, rec->original_hash);
    } else {
        /* Read request body JSON */
        char post_data[1024];
        int post_len = mg_read(conn, post_data, sizeof(post_data) - 1);
        if (post_len <= 0) {
            send_json_error(conn, 400, "Empty request body.");
            return 400;
        }
        post_data[post_len] = '\0';
        
        cJSON *body_json = cJSON_Parse(post_data);
        if (!body_json) {
            send_json_error(conn, 400, "Invalid JSON.");
            return 400;
        }
        
        cJSON *id_item = cJSON_GetObjectItem(body_json, "id");
        if (!id_item || !cJSON_IsString(id_item)) {
            cJSON_Delete(body_json);
            send_json_error(conn, 400, "Missing 'id' parameter.");
            return 400;
        }
        
        rec = file_record_find(id_item->valuestring);
        cJSON_Delete(body_json);
        
        if (!rec) {
            send_json_error(conn, 404, "File record not found.");
            return 404;
        }
    }
    
    /* Perform Compression */
    snprintf(rec->compressed_path, sizeof(rec->compressed_path),
             "output/%s_%s.huff", rec->id, rec->original_filename);
             
    CompressionStats stats;
    cJSON *tree_json = NULL;
    int err = huffman_compress_file(rec->upload_path, rec->compressed_path, &stats, &tree_json);
    if (err != HUFF_OK) {
        send_json_error(conn, 500, "Compression algorithm failed.");
        return 500;
    }
    
    rec->original_size = stats.original_size;
    rec->compressed_size = stats.compressed_size;
    rec->compression_ratio = stats.compression_ratio;
    rec->space_saved_percent = stats.space_saved_percent;
    rec->space_saved_bytes = stats.space_saved_bytes;
    rec->is_compressed = true;
    
    if (rec->tree_json) {
        cJSON_Delete(rec->tree_json);
    }
    rec->tree_json = tree_json;
    
    file_sha256(rec->upload_path, rec->original_hash);
    
    char comp_download_name[MAX_PATH_LEN];
    snprintf(comp_download_name, sizeof(comp_download_name), "%s.huff", rec->original_filename);
    
    cJSON *res = cJSON_CreateObject();
    cJSON_AddBoolToObject(res, "success", true);
    cJSON_AddStringToObject(res, "id", rec->id);
    cJSON_AddStringToObject(res, "original_filename", rec->original_filename);
    cJSON_AddStringToObject(res, "compressed_filename", comp_download_name);
    cJSON_AddNumberToObject(res, "original_size", (double)rec->original_size);
    cJSON_AddNumberToObject(res, "compressed_size", (double)rec->compressed_size);
    cJSON_AddNumberToObject(res, "compression_ratio", rec->compression_ratio);
    cJSON_AddNumberToObject(res, "space_saved_percent", rec->space_saved_percent);
    cJSON_AddNumberToObject(res, "space_saved_bytes", (double)rec->space_saved_bytes);
    cJSON_AddStringToObject(res, "original_hash", rec->original_hash);
    
    send_json_response(conn, 200, res);
    cJSON_Delete(res);
    return 200;
}

/* POST /api/decompress */
static int handle_decompress(struct mg_connection *conn, void *cbdata) {
    (void)cbdata;
    const struct mg_request_info *ri = mg_get_request_info(conn);
    
    if (strcmp(ri->request_method, "OPTIONS") == 0) {
        mg_printf(conn,
                  "HTTP/1.1 200 OK\r\n"
                  "Access-Control-Allow-Origin: *\r\n"
              "Access-Control-Allow-Private-Network: true\r\n"
                  "Access-Control-Allow-Private-Network: true\r\n"
                  "Access-Control-Allow-Methods: POST, OPTIONS\r\n"
                  "Access-Control-Allow-Headers: *\r\n"
                  "Content-Length: 0\r\n\r\n");
        return 200;
    }
    
    if (strcmp(ri->request_method, "POST") != 0) {
        send_json_error(conn, 400, "Method not allowed. Use POST.");
        return 400;
    }
    
    struct UploadCtx uctx;
    memset(&uctx, 0, sizeof(uctx));
    
    struct mg_form_data_handler fdh = {
        on_field_found,
        on_field_get,
        on_field_store,
        &uctx
    };
    
    int files_handled = mg_handle_form_request(conn, &fdh);
    if (files_handled <= 0 || !uctx.has_file) {
        send_json_error(conn, 400, "No .huff file uploaded.");
        return 400;
    }
    
    FileRecord *rec = file_record_create(uctx.original_filename);
    strncpy(rec->compressed_path, uctx.stored_path, sizeof(rec->compressed_path));
    
    /* Determine recovered filename */
    char recovered_name[MAX_PATH_LEN];
    strncpy(recovered_name, uctx.original_filename, sizeof(recovered_name));
    size_t len = strlen(recovered_name);
    if (len > 5 && strcmp(recovered_name + len - 5, ".huff") == 0) {
        recovered_name[len - 5] = '\0';
    } else {
        strncat(recovered_name, ".recovered", sizeof(recovered_name) - strlen(recovered_name) - 1);
    }
    
    snprintf(rec->decompressed_path, sizeof(rec->decompressed_path),
             "output/%s_%s", rec->id, recovered_name);
             
    uint64_t rec_size = 0;
    int err = huffman_decompress_file(rec->compressed_path, rec->decompressed_path, &rec_size);
    if (err != HUFF_OK) {
        const char *errmsg = "Failed to decompress file.";
        if (err == HUFF_ERR_INVALID_HEADER) errmsg = "Invalid file header: not a valid .huff file.";
        else if (err == HUFF_ERR_UNSUPPORTED_VERSION) errmsg = "Unsupported .huff format version.";
        else if (err == HUFF_ERR_CORRUPT_DATA) errmsg = "Corrupted or truncated .huff file.";
        
        send_json_error(conn, 400, errmsg);
        return 400;
    }
    
    rec->decompressed_size = rec_size;
    rec->is_decompressed = true;
    file_sha256(rec->decompressed_path, rec->decompressed_hash);
    
    uint64_t comp_sz = 0;
    file_exists(rec->compressed_path, &comp_sz);
    rec->compressed_size = comp_sz;
    
    cJSON *res = cJSON_CreateObject();
    cJSON_AddBoolToObject(res, "success", true);
    cJSON_AddStringToObject(res, "id", rec->id);
    cJSON_AddStringToObject(res, "compressed_filename", uctx.original_filename);
    cJSON_AddStringToObject(res, "recovered_filename", recovered_name);
    cJSON_AddNumberToObject(res, "compressed_size", (double)rec->compressed_size);
    cJSON_AddNumberToObject(res, "recovered_size", (double)rec->decompressed_size);
    cJSON_AddStringToObject(res, "hash", rec->decompressed_hash);
    
    send_json_response(conn, 200, res);
    cJSON_Delete(res);
    return 200;
}

/* GET /api/tree/:id */
static int handle_tree(struct mg_connection *conn, void *cbdata) {
    (void)cbdata;
    char id[MAX_ID_LEN] = {0};
    extract_id_from_request(conn, "/api/tree", id, sizeof(id));
    
    FileRecord *rec = file_record_find(id);
    if (!rec || !rec->tree_json) {
        send_json_error(conn, 404, "Tree data not found for specified ID.");
        return 404;
    }
    
    cJSON *res = cJSON_CreateObject();
    cJSON_AddBoolToObject(res, "success", true);
    cJSON_AddStringToObject(res, "id", rec->id);
    cJSON_AddStringToObject(res, "filename", rec->original_filename);
    cJSON_AddItemReferenceToObject(res, "tree", rec->tree_json);
    
    send_json_response(conn, 200, res);
    cJSON_DetachItemViaPointer(res, rec->tree_json);
    cJSON_Delete(res);
    return 200;
}

/* GET /api/result/:id */
static int handle_result(struct mg_connection *conn, void *cbdata) {
    (void)cbdata;
    char id[MAX_ID_LEN] = {0};
    extract_id_from_request(conn, "/api/result", id, sizeof(id));
    
    FileRecord *rec = file_record_find(id);
    if (!rec) {
        send_json_error(conn, 404, "Result not found for specified ID.");
        return 404;
    }
    
    cJSON *res = cJSON_CreateObject();
    cJSON_AddBoolToObject(res, "success", true);
    cJSON_AddStringToObject(res, "id", rec->id);
    cJSON_AddStringToObject(res, "original_filename", rec->original_filename);
    cJSON_AddNumberToObject(res, "original_size", (double)rec->original_size);
    cJSON_AddNumberToObject(res, "compressed_size", (double)rec->compressed_size);
    cJSON_AddNumberToObject(res, "decompressed_size", (double)rec->decompressed_size);
    cJSON_AddNumberToObject(res, "compression_ratio", rec->compression_ratio);
    cJSON_AddNumberToObject(res, "space_saved_percent", rec->space_saved_percent);
    cJSON_AddNumberToObject(res, "space_saved_bytes", (double)rec->space_saved_bytes);
    cJSON_AddStringToObject(res, "original_hash", rec->original_hash);
    cJSON_AddStringToObject(res, "decompressed_hash", rec->decompressed_hash);
    cJSON_AddBoolToObject(res, "is_compressed", rec->is_compressed);
    cJSON_AddBoolToObject(res, "is_decompressed", rec->is_decompressed);
    
    send_json_response(conn, 200, res);
    cJSON_Delete(res);
    return 200;
}

/* GET /api/download/:id */
static int handle_download(struct mg_connection *conn, void *cbdata) {
    (void)cbdata;
    const struct mg_request_info *ri = mg_get_request_info(conn);
    
    char id[MAX_ID_LEN] = {0};
    extract_id_from_request(conn, "/api/download", id, sizeof(id));
    
    char type[32] = "compressed";
    if (ri->query_string) {
        mg_get_var(ri->query_string, strlen(ri->query_string), "type", type, sizeof(type));
    }
    
    FileRecord *rec = file_record_find(id);
    if (!rec) {
        send_json_error(conn, 404, "File not found.");
        return 404;
    }
    
    const char *target_file = NULL;
    char download_filename[MAX_PATH_LEN];
    
    if (strcmp(type, "decompressed") == 0) {
        target_file = rec->decompressed_path;
        char base[MAX_PATH_LEN];
        sanitize_filename(rec->original_filename, base, sizeof(base));
        size_t blen = strlen(base);
        if (blen > 5 && strcmp(base + blen - 5, ".huff") == 0) {
            base[blen - 5] = '\0';
        }
        snprintf(download_filename, sizeof(download_filename), "%s", base);
    } else {
        target_file = rec->compressed_path;
        snprintf(download_filename, sizeof(download_filename), "%s.huff", rec->original_filename);
    }
    
    FILE *fp = fopen(target_file, "rb");
    if (!fp) {
        send_json_error(conn, 404, "Target download file is missing on server.");
        return 404;
    }
    
    fseek(fp, 0, SEEK_END);
    long file_len = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    
    mg_printf(conn,
              "HTTP/1.1 200 OK\r\n"
              "Content-Type: application/octet-stream\r\n"
              "Content-Disposition: attachment; filename=\"%s\"\r\n"
              "Content-Length: %ld\r\n"
              "Access-Control-Allow-Origin: *\r\n"
              "Access-Control-Allow-Private-Network: true\r\n"
              "Connection: close\r\n\r\n",
              download_filename, file_len);
              
    uint8_t buffer[CHUNK_SIZE];
    size_t n;
    while ((n = fread(buffer, 1, sizeof(buffer), fp)) > 0) {
        mg_write(conn, buffer, n);
    }
    
    fclose(fp);
    return 200;
}

static int handle_health(struct mg_connection *conn, void *cbdata) {
    (void)cbdata;
    cJSON *res = cJSON_CreateObject();
    cJSON_AddBoolToObject(res, "success", true);
    cJSON_AddStringToObject(res, "status", "ok");
    send_json_response(conn, 200, res);
    cJSON_Delete(res);
    return 200;
}

int server_start(int port, const char *document_root) {
    char port_str[16];
    snprintf(port_str, sizeof(port_str), "%d", port);
    
    const char *options[] = {
        "listening_ports", port_str,
        "document_root", document_root,
        "enable_directory_listing", "no",
        "num_threads", "8",
        NULL
    };
    
    g_ctx = mg_start(NULL, NULL, options);
    if (!g_ctx) {
        return -1;
    }
    
    mg_set_request_handler(g_ctx, "/api/analyze", handle_analyze, NULL);
    mg_set_request_handler(g_ctx, "/api/compress", handle_compress, NULL);
    mg_set_request_handler(g_ctx, "/api/decompress", handle_decompress, NULL);
    mg_set_request_handler(g_ctx, "/api/tree", handle_tree, NULL);
    mg_set_request_handler(g_ctx, "/api/result", handle_result, NULL);
    mg_set_request_handler(g_ctx, "/api/download", handle_download, NULL);
    mg_set_request_handler(g_ctx, "/api/health", handle_health, NULL);
    
    return 0;
}

void server_stop(void) {
    if (g_ctx) {
        mg_stop(g_ctx);
        g_ctx = NULL;
    }
    for (size_t i = 0; i < g_record_count; i++) {
        if (g_records[i].tree_json) {
            cJSON_Delete(g_records[i].tree_json);
            g_records[i].tree_json = NULL;
        }
    }
}
