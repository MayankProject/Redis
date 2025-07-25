#include "avl.h"
#include "common.h"
#include <stdio.h>
#include <unistd.h>
#ifndef TREE_DUMP_H
#define TREE_DUMP_H

#ifndef BRANCH_RT
# define BRANCH_RT "┌──"   /* child is right of its parent (printed above)   */
# define BRANCH_LF "└──"   /* child is left  of its parent (printed below)   */
# define VERT_BAR  "│   "  /* vertical spine while winding back up the stack */
# define SPACE_BAR "    "
#endif

static void
_scan_tree(const AvlNode *node, const char *prefix, int is_left)
{
    if (!node) return;

    /* first print right subtree so it appears on top */
    char new_prefix[256];
    snprintf(new_prefix, sizeof new_prefix, "%s%s",
             prefix, is_left ? VERT_BAR : SPACE_BAR);
    _scan_tree(node->right, new_prefix, 0);

    /* print this node */
    printf("%s%s(%s,%f)(%i)(%i)\n",
           prefix,
           is_left ? BRANCH_LF : BRANCH_RT,
           avlNode_to_znode(node)->name,
           node->data, node->height, node->subnodes);

    /* then print left subtree */
    snprintf(new_prefix, sizeof new_prefix, "%s%s",
             prefix, is_left ? SPACE_BAR : VERT_BAR);
    _scan_tree(node->left, new_prefix, 1);
}

static inline void dump_tree(const AvlNode *root)
{
    if (!root) {
        puts("(empty)");
        return;
    }

    printf("---------------------------\n");
    _scan_tree(root->right, "", 0);
    // sleep(2);
    printf("(%s,%f)(%i)(%i)\n", avlNode_to_znode(root)->name,root->data, root->height, root->subnodes);          /* root itself */
           
    _scan_tree(root->left,  "", 1);
    printf("---------------------------\n");
}

#endif 
