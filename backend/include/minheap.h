#ifndef MINHEAP_H
#define MINHEAP_H

#include "common.h"
#include "tree.h"

typedef struct {
    HuffmanNode **nodes;
    size_t size;
    size_t capacity;
} MinHeap;

MinHeap* minheap_create(size_t capacity);
void minheap_destroy(MinHeap *heap);
bool minheap_insert(MinHeap *heap, HuffmanNode *node);
HuffmanNode* minheap_extract_min(MinHeap *heap);
HuffmanNode* minheap_peek(const MinHeap *heap);
size_t minheap_size(const MinHeap *heap);
void minheap_get_active_ids(const MinHeap *heap, uint32_t *ids, uint32_t *count);

bool huffman_node_is_less(const HuffmanNode *a, const HuffmanNode *b);

#endif /* MINHEAP_H */
