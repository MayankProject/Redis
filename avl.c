#include "avl.h"
#include "treedump.h"
#include "common.h"
#include "zset.h"
#include <stdio.h>
#include <stdlib.h>

AvlNode *lookup(const int val, AvlNode* head){
    if (!head){
        return NULL;
    }
    if (head->data < val){
        return lookup(val, head->right);
    }
    else if (val < head->data){
        return lookup(val, head->left);
    }
    else{
        return head;
    }
}
int height(AvlNode *node){
   return node ? node->height : -1;
}
int updateNode(AvlNode *node){
    int leftheight = height(node->left);
    int rightheight = height(node->right);
    int leftSubNodes = node->left ? node->left->subnodes + 1: 0;
    int rightSubNodes = node->right ? node->right->subnodes + 1: 0;
    int max = leftheight > rightheight ? leftheight : rightheight;
    node->height = max + 1;
    node->subnodes = leftSubNodes + rightSubNodes;
    return 1;
}
int attach_avl_node(AvlNode *node, AvlNode *tree){
    node->parent = tree;
    int cmp = compareZnodes(tree, node);
    if(cmp < 0){
        tree->right = node;
    }
    else if (cmp > 0){
        tree->left = node;
    };
    while(node->parent){
        node = node->parent;
        updateNode(node);
    }
    return 1;
}
AvlNode *detatch_avl_node(AvlNode *node, AvlNode **root){
    AvlNode *parent = node->parent;
    AvlNode *next = NULL;
    if(node->subnodes == 0){
        if (parent){
            if(parent->data > node->data){
                parent->left = NULL;
            }
            else {
                parent->right = NULL;
            }
        }
    }
    else if(node->left && node->right){
        next = node->right;
        while(next->left){
            next = next->left;
        }
        AvlNode *nextParent = next->parent;
        detatch_avl_node(next, root);
        node->right->parent = NULL;
        next->parent = NULL;
        node->left->parent = NULL;
        attach_avl_node(node->left, next);
        attach_avl_node(node->right, next);
        if(parent){
            attach_avl_node(next, parent);
        }
        while(nextParent){
            updateNode(nextParent);
            balance(nextParent, root);
            nextParent = nextParent->parent;
        }
    }
    else{
        next = node->left ? node->left : node->right;
        next->parent = NULL;
        parent && attach_avl_node(next, parent);
    }
    return next;
}

AvlNode *rotateRight(AvlNode *node){
    AvlNode *parent = node->parent;
    AvlNode *right = node->right;
    AvlNode *left = node->left;
    node->left = left->right;
    left->right = node;
    node->parent = left;
    if(node->left){
        node->left->parent = node;
    }
    left->parent = parent;
    if(parent){
        if(parent->data < left->data){
            parent->right = left;
        }
        else{
            parent->left = left;
        }
    }
    while(node){
        updateNode(node);
        node = node->parent;
    }
    return left;
}
AvlNode *rotateLeft(AvlNode *node){
    AvlNode *parent = node->parent;
    AvlNode *right = node->right;
    AvlNode *left = node->left;
    node->right = right->left;
    right->left = node;
    node->parent = right;
    if(node->right){
        node->right->parent = node;
    }
    right->parent = parent;
    if(parent){
        if(parent->data < right->data){
            parent->right = right;
        }
        else{
            parent->left = right;
        }
    }
    while(node){
        updateNode(node);
       node = node->parent;
    }
    return right;
}
AvlNode *balance(AvlNode *node, AvlNode **root){
    int leftHeight = height(node->left);
    int rightHeight = height(node->right);
    // 1< is right larger, <-1 means left larger;
    int balance = rightHeight - leftHeight;
    // R
    if (1 < balance){
        printf("Unbalanced right\n");
        int s_leftHeight = height(node->right->left);
        int s_rightHeight = height(node->right->right);
        // R
        if (s_leftHeight < s_rightHeight){
            node = rotateLeft(node);
        }
        // L
        else{
            rotateRight(node->right);
            node = rotateLeft(node);
        }
    }
    // L
    else if(balance < -1){
        printf("Unbalanced left\n");
        int s_leftHeight = height(node->left->left);
        int s_rightHeight = height(node->left->right);
        // R
        if (s_leftHeight < s_rightHeight){
            rotateLeft(node->left);
            node = rotateRight(node);
        }
        // L
        else{
            node = rotateRight(node);
        }
    }
    *root = node;
    return node;
}
int insert_avl_node(AvlNode *node, AvlNode *tree){
    int cmp = compareZnodes(tree, node);
    if(cmp < 0){
        if (tree->right){
            return insert_avl_node(node, tree->right);
        }
        attach_avl_node(node, tree);
        return 1;
    }
    else if (cmp > 0){
        if (tree->left){
            return insert_avl_node(node, tree->left);
        }
        attach_avl_node(node, tree);
        return 1;
    }
    else {
        printf("Duplicate entry\n");
        return 0;
    };
    return 1;
}

int find_rank(AvlNode *node, AvlNode *root){
    int rank = root->left ? root->left->subnodes + 1: 0;
    if(root->parent){
        printf("expected root!\n");
    };
    while (true){
        int cmp = compareZnodes(root, node);
        if (cmp < 0){
            if(!root->right) break;
            root = root->right;
            rank += (root->left ? root->left->subnodes + 1: 0) + 1;
        }
        else if (cmp > 0){
            if(!root->left) break;
            root = root->left;
            rank -= (root->right ? root->right->subnodes + 1: 0) + 1;
        }
        else {
            return rank;
        }
    }
    return -1;
}

AvlNode *init_tree_node(const int val){
    AvlNode *node = calloc(1, sizeof(AvlNode));
    node->parent = NULL;
    node->data = val;
    node->left = NULL;
    node->right = NULL;
    node->height = 0;
    node->subnodes = 0;
    return node;
}

