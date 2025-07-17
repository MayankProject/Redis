#pragma once
#include <stddef.h>   /* offsetof */
#include "server.h"

#define LOG_INT(val) printf("log: %i\n", val)

#define LOG(level, fmt, ...) \
    fprintf(stderr, "[%s] %s:%d:%s(): " fmt "\n", \
        level, __FILE__, __LINE__, __func__, ##__VA_ARGS__)

#define container_of(ptr, type, member) ({                  \
    const typeof( ((type *)0)->member ) *__mptr = (ptr);    \
    (type *)( (char *)__mptr - offsetof(type, member) );})

static Entry *hnode_to_entry(HNode *n) {
    return container_of(n, Entry, node);
}
