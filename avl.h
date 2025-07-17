typedef struct AvlNode {
    int data;
    struct AvlNode* right;
    struct AvlNode* left;
    struct AvlNode* parent;
    int height;
} AvlNode;

