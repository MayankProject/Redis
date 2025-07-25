#pragma once
typedef struct AvlNode {
    double data;
    struct AvlNode* right;
    struct AvlNode* left;
    struct AvlNode* parent;
    int height;
    int subnodes;
} AvlNode;

AvlNode *init_tree_node(const int val);
AvlNode *insert_val(int val, AvlNode **tree);
int insert_avl_node(AvlNode *node, AvlNode *tree);
AvlNode *detatch_avl_node(AvlNode *node, AvlNode **root);
int find_rank(double val, AvlNode *root);
AvlNode *balance(AvlNode *node, AvlNode **root);
