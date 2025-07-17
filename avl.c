#include "avl.h"
#include "treedump.h"
#include "common.h"
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
    int max = leftheight > rightheight ? leftheight : rightheight;
    node->height = max + 1;
    return 1;
}
int attach_avl_node(AvlNode *node, AvlNode *tree){
    node->parent = tree;
    if(tree->data < node->data){
        tree->right = node;
    }
    else if (node->data < tree->data){
        tree->left = node;
    };
    while(node->parent){
        node = node->parent;
        updateNode(node);
    }
    return 1;
}
int detatch_avl_node(AvlNode *node){
    AvlNode *parent = node->parent;
    if(!parent){
        printf("Can't remove root");
        return 0;
    }
    AvlNode *next;
    if(node->height == 0){
        if(parent->left->data == node->data){
            parent->left = NULL;
        }
        else {
            parent->right = NULL;
        }
    }
    else if(node->left && node->right){
        next = node->right;
        while(next->left){
            next = next->left;
        }
        node->right->parent = NULL;
        next->parent = NULL;
        node->left->parent = NULL;
        attach_avl_node(node->left, next);
        if(node->right->data != next->data){
            attach_avl_node(node->right, next);
        }
        attach_avl_node(next, parent);
    }
    else{
        next = node->left ? node->left : node->right;
        attach_avl_node(next, parent);
    }
    free(node);
    return 1;
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
        printf("Unbalanced\n");
        int s_leftHeight = height(node->right->left);
        int s_rightHeight = height(node->right->right);
        // R
        if (s_leftHeight < s_rightHeight){
            node = rotateLeft(node);
        }
        // L
        else{
            node = rotateRight(node);
            node = rotateLeft(node);
        }
    }
    // L
    else if(balance < -1){
        printf("Unbalanced\n");
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
    if(tree->data < node->data){
        if (tree->right){
            return insert_avl_node(node, tree->right);
        }
        attach_avl_node(node, tree);
        return 1;
    }
    else if (node->data < tree->data ){
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

AvlNode *init(const int val){
    AvlNode *node = calloc(1, sizeof(AvlNode));
    node->parent = NULL;
    node->data = val;
    node->left = NULL;
    node->right = NULL;
    node->height = 0;
    return node;
}

int insert_val(int val, AvlNode **tree){
    AvlNode *node;
    node = init(val);
    insert_avl_node(node, *tree);
    AvlNode *parent = node->parent;
    while(parent){
        balance(parent, tree);
        parent = parent->parent;
    }
    return 1;
}
int remove_val(int val, AvlNode **tree){
    AvlNode *node = lookup(val, *tree);
    if(!node) return 0;
    AvlNode *parent = node->parent;
    detatch_avl_node(node);
    while(parent){
        balance(parent, tree);
        parent = parent->parent;
    }
    return 1;
}
int main(){
    AvlNode *tree = init(20);
    insert_val(10, &tree);
    dump_tree(tree);

    insert_val(5, &tree);     // causes no rotation
    dump_tree(tree);

    insert_val(6, &tree);     // causes LR rotation at 10
    dump_tree(tree);

    insert_val(4, &tree);     // no rotation
    dump_tree(tree);

    insert_val(7, &tree);     // causes RR at 5 or no rotation depending on balance
    dump_tree(tree);

    insert_val(-20, &tree);   // deepens left subtree
    dump_tree(tree);

    insert_val(15, &tree);    // new right-heavy
    dump_tree(tree);

    insert_val(20, &tree);    // causes RR at 15
    dump_tree(tree);

    insert_val(25, &tree);    // causes RR at 20
    dump_tree(tree);

    insert_val(13, &tree);    // causes LR at 15
    dump_tree(tree);

    insert_val(14, &tree);    // further triggers balancing
    dump_tree(tree);

    remove_val(5, &tree); // delete node with 2 children
    dump_tree(tree);

    remove_val(6, &tree); // delete node with 2 children
    dump_tree(tree);

    remove_val(-20, &tree); // delete node with 1 child
    dump_tree(tree);

    LOG_INT(tree->data);
    return 1;
}
