#pragma once
#include "hashmap.h"
#include "avl.h"

// for every key ("leaderboard"), has a separate tree and hashmap;
typedef struct zset {
    AvlNode *root ;
    HMap *map;
} zset;

typedef struct ZNode {
    char *name;
    double score;

    // embedded
    AvlNode tree_node;
    HNode node;
} ZNode;

bool eq_znode(const char* key, HNode *node);
ZNode *add_zset(zset *Set, char *name, double score);
ZNode *lookup_zset(zset Set, char *name);
int remove_zset(zset *Set, char* name);
int lookup_zset_rank(zset *Set, char *name);
int compareZnodes(AvlNode *n1, AvlNode *n2);
