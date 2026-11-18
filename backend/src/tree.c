#include "tree.h"
#include "minheap.h"
#include <ctype.h>

HuffmanNode* huffman_node_create_leaf(uint32_t id, uint8_t symbol, uint64_t freq) {
    HuffmanNode *node = (HuffmanNode*)malloc(sizeof(HuffmanNode));
    if (!node) return NULL;
    node->node_id = id;
    node->freq = freq;
    node->symbol = symbol;
    node->is_leaf = 1;
    node->min_symbol = symbol;
    node->left = NULL;
    node->right = NULL;
    return node;
}

HuffmanNode* huffman_node_create_internal(uint32_t id, HuffmanNode *left, HuffmanNode *right) {
    HuffmanNode *node = (HuffmanNode*)malloc(sizeof(HuffmanNode));
    if (!node) return NULL;
    node->node_id = id;
    node->freq = (left ? left->freq : 0) + (right ? right->freq : 0);
    node->symbol = 256;
    node->is_leaf = 0;
    
    uint16_t min_s = 256;
    if (left && left->min_symbol < min_s) min_s = left->min_symbol;
    if (right && right->min_symbol < min_s) min_s = right->min_symbol;
    node->min_symbol = min_s;
    
    node->left = left;
    node->right = right;
    return node;
}

void huffman_free_tree(HuffmanNode *root) {
    if (!root) return;
    huffman_free_tree(root->left);
    huffman_free_tree(root->right);
    free(root);
}

void construction_history_init(ConstructionHistory *history) {
    if (!history) return;
    history->steps = NULL;
    history->count = 0;
    history->capacity = 0;
}

void construction_history_add_step(ConstructionHistory *history,
                                   uint32_t left_id, uint32_t right_id,
                                   uint32_t parent_id, uint64_t parent_freq,
                                   const uint32_t *active_ids, uint32_t active_count) {
    if (!history) return;
    if (history->count >= history->capacity) {
        size_t new_cap = history->capacity == 0 ? 32 : history->capacity * 2;
        ConstructionStep *new_steps = (ConstructionStep*)realloc(history->steps, sizeof(ConstructionStep) * new_cap);
        if (!new_steps) return;
        history->steps = new_steps;
        history->capacity = new_cap;
    }
    
    ConstructionStep *step = &history->steps[history->count];
    step->step_index = (uint32_t)history->count;
    step->left_id = left_id;
    step->right_id = right_id;
    step->parent_id = parent_id;
    step->parent_freq = parent_freq;
    step->active_count = active_count > (MAX_SYMBOLS * 2) ? (MAX_SYMBOLS * 2) : active_count;
    for (uint32_t i = 0; i < step->active_count; i++) {
        step->active_ids[i] = active_ids[i];
    }
    history->count++;
}

void construction_history_free(ConstructionHistory *history) {
    if (!history) return;
    if (history->steps) {
        free(history->steps);
        history->steps = NULL;
    }
    history->count = 0;
    history->capacity = 0;
}

HuffmanNode* huffman_build_tree(const FrequencyTable *ft, ConstructionHistory *history) {
    if (!ft || ft->unique_symbols == 0) {
        return NULL;
    }
    
    if (history) {
        construction_history_init(history);
    }
    
    uint32_t next_id = 0;
    
    /* Special Case: Exactly 1 unique symbol */
    if (ft->unique_symbols == 1) {
        uint8_t single_symbol = 0;
        uint64_t single_freq = 0;
        for (int i = 0; i < MAX_SYMBOLS; i++) {
            if (ft->counts[i] > 0) {
                single_symbol = (uint8_t)i;
                single_freq = ft->counts[i];
                break;
            }
        }
        HuffmanNode *leaf = huffman_node_create_leaf(next_id++, single_symbol, single_freq);
        HuffmanNode *parent = huffman_node_create_internal(next_id++, leaf, NULL);
        
        if (history) {
            uint32_t active[1] = { parent->node_id };
            construction_history_add_step(history, leaf->node_id, 0xFFFFFFFF, parent->node_id, parent->freq, active, 1);
        }
        return parent;
    }
    
    MinHeap *heap = minheap_create(ft->unique_symbols * 2);
    if (!heap) return NULL;
    
    /* Populate MinHeap with leaf nodes */
    for (int i = 0; i < MAX_SYMBOLS; i++) {
        if (ft->counts[i] > 0) {
            HuffmanNode *leaf = huffman_node_create_leaf(next_id++, (uint8_t)i, ft->counts[i]);
            minheap_insert(heap, leaf);
        }
    }
    
    /* Build tree by repeatedly combining 2 lowest frequency nodes */
    while (minheap_size(heap) > 1) {
        HuffmanNode *left = minheap_extract_min(heap);
        HuffmanNode *right = minheap_extract_min(heap);
        
        /* Tie breaker order: left child has smaller frequency / symbol */
        if (!huffman_node_is_less(left, right)) {
            HuffmanNode *temp = left;
            left = right;
            right = temp;
        }
        
        HuffmanNode *parent = huffman_node_create_internal(next_id++, left, right);
        minheap_insert(heap, parent);
        
        if (history) {
            uint32_t active_ids[MAX_SYMBOLS * 2];
            uint32_t active_count = 0;
            minheap_get_active_ids(heap, active_ids, &active_count);
            construction_history_add_step(history, left->node_id, right->node_id,
                                          parent->node_id, parent->freq,
                                          active_ids, active_count);
        }
    }
    
    HuffmanNode *root = minheap_extract_min(heap);
    minheap_destroy(heap);
    return root;
}

/* Helper to format symbol label for display */
static void format_symbol_label(uint16_t symbol, char *out, size_t out_len) {
    if (symbol > 255) {
        snprintf(out, out_len, "Internal");
        return;
    }
    uint8_t b = (uint8_t)symbol;
    if (b == ' ') {
        snprintf(out, out_len, "' ' (0x20)");
    } else if (b == '\n') {
        snprintf(out, out_len, "\\n (0x0A)");
    } else if (b == '\r') {
        snprintf(out, out_len, "\\r (0x0D)");
    } else if (b == '\t') {
        snprintf(out, out_len, "\\t (0x09)");
    } else if (isprint(b)) {
        snprintf(out, out_len, "'%c' (0x%02X)", (char)b, b);
    } else {
        snprintf(out, out_len, "0x%02X", b);
    }
}

/* Helper to generate codes recursively for JSON representation */
static void collect_codes_json(const HuffmanNode *node, char *prefix, int depth, cJSON *codes_obj) {
    if (!node) return;
    if (node->is_leaf) {
        prefix[depth] = '\0';
        char key[16];
        snprintf(key, sizeof(key), "%u", (unsigned int)node->symbol);
        
        cJSON *code_info = cJSON_CreateObject();
        cJSON_AddStringToObject(code_info, "code", depth == 0 ? "0" : prefix);
        cJSON_AddNumberToObject(code_info, "length", depth == 0 ? 1 : depth);
        char label[32];
        format_symbol_label(node->symbol, label, sizeof(label));
        cJSON_AddStringToObject(code_info, "label", label);
        cJSON_AddNumberToObject(code_info, "freq", (double)node->freq);
        cJSON_AddItemToObject(codes_obj, key, code_info);
        return;
    }
    
    prefix[depth] = '0';
    collect_codes_json(node->left, prefix, depth + 1, codes_obj);
    
    prefix[depth] = '1';
    collect_codes_json(node->right, prefix, depth + 1, codes_obj);
}

/* Helper to collect all nodes into a flat list for visualizer quick lookup */
static void collect_nodes_json(const HuffmanNode *node, cJSON *nodes_arr) {
    if (!node) return;
    
    cJSON *item = cJSON_CreateObject();
    cJSON_AddNumberToObject(item, "id", node->node_id);
    cJSON_AddNumberToObject(item, "freq", (double)node->freq);
    cJSON_AddBoolToObject(item, "is_leaf", node->is_leaf);
    
    char label[32];
    format_symbol_label(node->symbol, label, sizeof(label));
    cJSON_AddStringToObject(item, "label", label);
    
    if (node->is_leaf) {
        cJSON_AddNumberToObject(item, "symbol", node->symbol);
        cJSON_AddNullToObject(item, "left_id");
        cJSON_AddNullToObject(item, "right_id");
    } else {
        cJSON_AddNullToObject(item, "symbol");
        if (node->left) {
            cJSON_AddNumberToObject(item, "left_id", node->left->node_id);
        } else {
            cJSON_AddNullToObject(item, "left_id");
        }
        if (node->right) {
            cJSON_AddNumberToObject(item, "right_id", node->right->node_id);
        } else {
            cJSON_AddNullToObject(item, "right_id");
        }
    }
    cJSON_AddItemToArray(nodes_arr, item);
    
    collect_nodes_json(node->left, nodes_arr);
    collect_nodes_json(node->right, nodes_arr);
}

/* Recursive hierarchy builder for tree rendering */
static cJSON* build_hierarchy_json(const HuffmanNode *node, const char *edge_label) {
    if (!node) return NULL;
    
    cJSON *item = cJSON_CreateObject();
    cJSON_AddNumberToObject(item, "id", node->node_id);
    cJSON_AddNumberToObject(item, "freq", (double)node->freq);
    cJSON_AddBoolToObject(item, "is_leaf", node->is_leaf);
    
    char label[32];
    format_symbol_label(node->symbol, label, sizeof(label));
    cJSON_AddStringToObject(item, "label", label);
    
    if (edge_label) {
        cJSON_AddStringToObject(item, "edge", edge_label);
    }
    
    if (node->is_leaf) {
        cJSON_AddNumberToObject(item, "symbol", node->symbol);
    } else {
        cJSON *children = cJSON_CreateArray();
        if (node->left) {
            cJSON *left_json = build_hierarchy_json(node->left, "0");
            if (left_json) cJSON_AddItemToArray(children, left_json);
        }
        if (node->right) {
            cJSON *right_json = build_hierarchy_json(node->right, "1");
            if (right_json) cJSON_AddItemToArray(children, right_json);
        }
        cJSON_AddItemToObject(item, "children", children);
    }
    
    return item;
}

cJSON* huffman_tree_to_json(const HuffmanNode *root, const ConstructionHistory *history, const FrequencyTable *ft) {
    cJSON *root_obj = cJSON_CreateObject();
    if (!root) {
        cJSON_AddBoolToObject(root_obj, "empty", true);
        return root_obj;
    }
    
    cJSON_AddBoolToObject(root_obj, "empty", false);
    cJSON_AddNumberToObject(root_obj, "root_id", root->node_id);
    cJSON_AddNumberToObject(root_obj, "total_freq", (double)root->freq);
    cJSON_AddNumberToObject(root_obj, "unique_symbols", ft ? ft->unique_symbols : 0);
    
    /* Flat node registry */
    cJSON *nodes_arr = cJSON_CreateArray();
    collect_nodes_json(root, nodes_arr);
    cJSON_AddItemToObject(root_obj, "nodes", nodes_arr);
    
    /* Hierarchical tree structure */
    cJSON *hierarchy = build_hierarchy_json(root, NULL);
    cJSON_AddItemToObject(root_obj, "hierarchy", hierarchy);
    
    /* Code dictionary */
    cJSON *codes_obj = cJSON_CreateObject();
    char prefix[MAX_SYMBOLS + 1];
    collect_codes_json(root, prefix, 0, codes_obj);
    cJSON_AddItemToObject(root_obj, "codes", codes_obj);
    
    /* Construction steps */
    cJSON *steps_arr = cJSON_CreateArray();
    if (history && history->steps) {
        for (size_t i = 0; i < history->count; i++) {
            const ConstructionStep *s = &history->steps[i];
            cJSON *step_item = cJSON_CreateObject();
            cJSON_AddNumberToObject(step_item, "step", s->step_index);
            cJSON_AddNumberToObject(step_item, "left_id", s->left_id);
            if (s->right_id != 0xFFFFFFFF) {
                cJSON_AddNumberToObject(step_item, "right_id", s->right_id);
            } else {
                cJSON_AddNullToObject(step_item, "right_id");
            }
            cJSON_AddNumberToObject(step_item, "parent_id", s->parent_id);
            cJSON_AddNumberToObject(step_item, "parent_freq", (double)s->parent_freq);
            
            cJSON *active_arr = cJSON_CreateArray();
            for (uint32_t j = 0; j < s->active_count; j++) {
                cJSON_AddItemToArray(active_arr, cJSON_CreateNumber(s->active_ids[j]));
            }
            cJSON_AddItemToObject(step_item, "active_ids", active_arr);
            cJSON_AddItemToArray(steps_arr, step_item);
        }
    }
    cJSON_AddItemToObject(root_obj, "construction_steps", steps_arr);
    
    return root_obj;
}
