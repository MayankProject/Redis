#pragma once
#include "hashmap.h"
#include "zset.h"

typedef enum EntryType {
    Z_SET = 0,
    K_V_PAIR
} EntryType;

typedef struct Entry {
    char *key;
    struct HNode node;
    EntryType type;
    union {
        // for simple key value pair
        char *value;

        // for sorted set
        zset set;
    };
} Entry;
