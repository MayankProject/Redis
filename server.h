#pragma once
#include "hashmap.h"

typedef struct Entry {
    char *key;
    char *value;
    struct HNode node;
} Entry;
