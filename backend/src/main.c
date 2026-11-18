#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif
#include "common.h"
#include "server.h"
#include "compress.h"
#include "decompress.h"
#include "file_handler.h"

static volatile int g_running = 1;

static void handle_sigint(int sig) {
    (void)sig;
    g_running = 0;
}

static void print_banner(int port) {
    printf("====================================================================\n");
    printf("                     HUFFMAN LAB SERVER                             \n");
    printf("            Data Compression & Algorithm Visualizer                 \n");
    printf("         Understand the algorithm. See the compression.             \n");
    printf("====================================================================\n");
    printf("  Status:        ONLINE\n");
    printf("  Port:          %d\n", port);
    printf("  Web Interface: http://localhost:%d/\n", port);
    printf("  Document Root: frontend/\n");
    printf("  API Endpoints: /api/analyze, /api/compress, /api/decompress\n");
    printf("                 /api/tree/:id, /api/result/:id, /api/download/:id\n");
    printf("====================================================================\n");
    printf("Press Ctrl+C to stop server.\n\n");
}

int main(int argc, char *argv[]) {
    int port = 8080;
    const char *doc_root = "frontend";
    
    /* CLI compression mode */
    if (argc >= 4 && strcmp(argv[1], "--compress") == 0) {
        CompressionStats stats;
        printf("Compressing '%s' -> '%s'...\n", argv[2], argv[3]);
        int err = huffman_compress_file(argv[2], argv[3], &stats, NULL);
        if (err == HUFF_OK) {
            printf("Success! Original: %lu B, Compressed: %lu B (Saved: %.2f%%)\n",
                   (unsigned long)stats.original_size, (unsigned long)stats.compressed_size, stats.space_saved_percent);
            return 0;
        } else {
            fprintf(stderr, "Error: Compression failed with code %d\n", err);
            return 1;
        }
    }
    
    /* CLI decompression mode */
    if (argc >= 4 && strcmp(argv[1], "--decompress") == 0) {
        uint64_t orig_sz = 0;
        printf("Decompressing '%s' -> '%s'...\n", argv[2], argv[3]);
        int err = huffman_decompress_file(argv[2], argv[3], &orig_sz);
        if (err == HUFF_OK) {
            printf("Success! Recovered %lu bytes.\n", (unsigned long)orig_sz);
            return 0;
        } else {
            fprintf(stderr, "Error: Decompression failed with code %d\n", err);
            return 1;
        }
    }
    
    /* Parse port if provided: ./server [port] */
    if (argc >= 2) {
        int p = atoi(argv[1]);
        if (p > 0 && p <= 65535) {
            port = p;
        }
    }
    
    signal(SIGINT, handle_sigint);
    signal(SIGTERM, handle_sigint);
    
    if (server_start(port, doc_root) != 0) {
        fprintf(stderr, "Fatal Error: Failed to start server on port %d.\n", port);
        return 1;
    }
    
    print_banner(port);
    
    while (g_running) {
#ifdef _WIN32
        Sleep(500);
#else
        usleep(500000);
#endif
    }
    
    printf("\nShutting down server gracefully...\n");
    server_stop();
    printf("Server stopped.\n");
    return 0;
}
