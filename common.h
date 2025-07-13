#pragma once
#include <stddef.h>   /* offsetof */
#include "server.h"
#define container_of(ptr, type, member) ({                  \
    const typeof( ((type *)0)->member ) *__mptr = (ptr);    \
    (type *)( (char *)__mptr - offsetof(type, member) );})

static Entry *hnode_to_entry(HNode *n) {
    return container_of(n, Entry, node);
}
