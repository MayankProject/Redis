#include "../include/zset.h"
#include "../include/treedump.h"
#include "../include/avl.h"
#include "../include/common.h"
#include "../include/hashmap.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool eq_znode(const char* key, HNode *node){
    return strcmp(hnode_to_znode(node)->name, key) == 0;
}

// takes HNode and prints out (name, score) for Znode 
void print_val_znode(HNode* n){
    ZNode *entry = hnode_to_znode(n);
    printf("> %s, %f\n", entry->name, entry->score);
}

int compareZnodes(AvlNode *n1, AvlNode *n2){
    if(n1->data == n2->data){
        ZNode *z_n1 = avlNode_to_znode(n1);
        ZNode *z_n2 = avlNode_to_znode(n2);
        return strcmp(z_n1->name, z_n2->name);
    }
    return n1->data - n2->data;
}

ZNode *add_zset(zset *Set, char *name, double score){
    ZNode *znode = malloc(sizeof(ZNode));
    znode->name = strdup(name);
    znode->score = score;
    AvlNode *new_avl_node = init_tree_node(score);
    znode->tree_node = *new_avl_node;
    free(new_avl_node);
    znode->node.hash = str_hash((uint8_t*) name, strlen(name));
    // for tree
    if(!Set->root){
        Set->root = &znode->tree_node;
    }
    else {
        if(!insert_avl_node(&znode->tree_node, Set->root)){
            return NULL;
        };
    }
    // for hashtable
    insert_node(&znode->node, Set->map);
    possibly_resize(Set->map);
    Set->map->entries++;
    AvlNode *iter = &znode->tree_node;
    while(iter){
        balance(iter, &Set->root);
        iter = iter->parent;
    }
    printf("---------------------------\n");
    scan_map(Set->map, print_val_znode);
    dump_tree(Set->root);
    printf("---------------------------\n");
    return znode;
}

ZNode *lookup_zset(zset Set, char *name){
    HNode *node = h_lookup(Set.map, name, eq_znode);
    if(!node){
        return NULL;
    }
    return hnode_to_znode(node);
}

int remove_zset(zset *Set, char* name){
    HTab *table = Set->map->newer;
    HNode *n = htab_lookup(table, name, eq_znode);
    if (Set->map->older && n == NULL) {
        table = Set->map->older;
        n = htab_lookup(table, name, eq_znode);
    };
    if (n == NULL){
        return 0;
    }
    ZNode *e = hnode_to_znode(n);
    AvlNode *parent = e->tree_node.parent;
    detach_node(n, table);
    Set->map->entries--;

    AvlNode *successor = detatch_avl_node(&e->tree_node, &Set->root);
    // trying to remove root
    if (!successor || !successor->parent){
        Set->root = successor;
    }
    while(parent){
        balance(parent, &Set->root);
        parent = parent->parent;
    }
    dump_tree(Set->root);
    free(e->name);
    free(e);
    return 1;
}

int lookup_zset_rank(zset *Set, char *name){
    ZNode *n = lookup_zset(*Set, name);
    if(!n) return -1;
    return find_rank(&n->tree_node, Set->root);
}
