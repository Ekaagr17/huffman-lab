#ifndef TREE_H
#define TREE_H

#include "common.h"
#include "frequency.h"
#include "cJSON.h"

typedef struct HuffmanNode {
    uint32_t node_id;
    uint64_t freq;
    uint16_t symbol;      /* 0-255 for leaf, 256 for internal */
    uint8_t is_leaf;
    uint16_t min_symbol;  /* Smallest symbol under this node, for tie-breaking */
    struct HuffmanNode *left;
    struct HuffmanNode *right;
} HuffmanNode;

typedef struct {
    uint32_t step_index;
    uint32_t left_id;
    uint32_t right_id;
    uint32_t parent_id;
    uint64_t parent_freq;
    uint32_t active_count;
    uint32_t active_ids[MAX_SYMBOLS * 2];
} ConstructionStep;

typedef struct {
    ConstructionStep *steps;
    size_t count;
    size_t capacity;
} ConstructionHistory;

/* Node creation and disposal */
HuffmanNode* huffman_node_create_leaf(uint32_t id, uint8_t symbol, uint64_t freq);
HuffmanNode* huffman_node_create_internal(uint32_t id, HuffmanNode *left, HuffmanNode *right);
void huffman_free_tree(HuffmanNode *root);

/* History management */
void construction_history_init(ConstructionHistory *history);
void construction_history_add_step(ConstructionHistory *history,
                                   uint32_t left_id, uint32_t right_id,
                                   uint32_t parent_id, uint64_t parent_freq,
                                   const uint32_t *active_ids, uint32_t active_count);
void construction_history_free(ConstructionHistory *history);

/* Full tree build with step tracking */
HuffmanNode* huffman_build_tree(const FrequencyTable *ft, ConstructionHistory *history);

/* JSON serialization for the visualizer */
cJSON* huffman_tree_to_json(const HuffmanNode *root, const ConstructionHistory *history, const FrequencyTable *ft);

#endif /* TREE_H */
