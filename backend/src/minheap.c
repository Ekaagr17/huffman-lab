#include "minheap.h"

bool huffman_node_is_less(const HuffmanNode *a, const HuffmanNode *b) {
    if (!a || !b) return false;
    if (a->freq != b->freq) {
        return a->freq < b->freq;
    }
    if (a->min_symbol != b->min_symbol) {
        return a->min_symbol < b->min_symbol;
    }
    return a->node_id < b->node_id;
}

MinHeap* minheap_create(size_t capacity) {
    if (capacity == 0) capacity = 16;
    MinHeap *heap = (MinHeap*)malloc(sizeof(MinHeap));
    if (!heap) return NULL;
    
    heap->nodes = (HuffmanNode**)malloc(sizeof(HuffmanNode*) * capacity);
    if (!heap->nodes) {
        free(heap);
        return NULL;
    }
    heap->size = 0;
    heap->capacity = capacity;
    return heap;
}

void minheap_destroy(MinHeap *heap) {
    if (!heap) return;
    if (heap->nodes) {
        free(heap->nodes);
    }
    free(heap);
}

static void minheap_swap(MinHeap *heap, size_t i, size_t j) {
    HuffmanNode *tmp = heap->nodes[i];
    heap->nodes[i] = heap->nodes[j];
    heap->nodes[j] = tmp;
}

static void minheap_heapify_up(MinHeap *heap, size_t idx) {
    while (idx > 0) {
        size_t parent = (idx - 1) / 2;
        if (huffman_node_is_less(heap->nodes[idx], heap->nodes[parent])) {
            minheap_swap(heap, idx, parent);
            idx = parent;
        } else {
            break;
        }
    }
}

static void minheap_heapify_down(MinHeap *heap, size_t idx) {
    while (true) {
        size_t left = 2 * idx + 1;
        size_t right = 2 * idx + 2;
        size_t smallest = idx;
        
        if (left < heap->size && huffman_node_is_less(heap->nodes[left], heap->nodes[smallest])) {
            smallest = left;
        }
        if (right < heap->size && huffman_node_is_less(heap->nodes[right], heap->nodes[smallest])) {
            smallest = right;
        }
        
        if (smallest != idx) {
            minheap_swap(heap, idx, smallest);
            idx = smallest;
        } else {
            break;
        }
    }
}

bool minheap_insert(MinHeap *heap, HuffmanNode *node) {
    if (!heap || !node) return false;
    
    if (heap->size >= heap->capacity) {
        size_t new_cap = heap->capacity * 2;
        HuffmanNode **new_nodes = (HuffmanNode**)realloc(heap->nodes, sizeof(HuffmanNode*) * new_cap);
        if (!new_nodes) return false;
        heap->nodes = new_nodes;
        heap->capacity = new_cap;
    }
    
    heap->nodes[heap->size] = node;
    minheap_heapify_up(heap, heap->size);
    heap->size++;
    return true;
}

HuffmanNode* minheap_extract_min(MinHeap *heap) {
    if (!heap || heap->size == 0) return NULL;
    
    HuffmanNode *min_node = heap->nodes[0];
    heap->nodes[0] = heap->nodes[heap->size - 1];
    heap->size--;
    
    if (heap->size > 0) {
        minheap_heapify_down(heap, 0);
    }
    
    return min_node;
}

HuffmanNode* minheap_peek(const MinHeap *heap) {
    if (!heap || heap->size == 0) return NULL;
    return heap->nodes[0];
}

size_t minheap_size(const MinHeap *heap) {
    return heap ? heap->size : 0;
}

void minheap_get_active_ids(const MinHeap *heap, uint32_t *ids, uint32_t *count) {
    if (!heap || !ids || !count) return;
    *count = (uint32_t)heap->size;
    for (size_t i = 0; i < heap->size; i++) {
        ids[i] = heap->nodes[i]->node_id;
    }
}
