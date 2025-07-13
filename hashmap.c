#include "hashmap.h"
#include "common.h"
#include "server.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// FNV hash
uint64_t str_hash(const uint8_t *data, size_t len) {
    uint64_t h = 0x811C9DC5;
    for (size_t i = 0; i < len; i++) {
        h = (h + data[i]) * 0x01000193;
    }
    return h;
}

void init_HTab(HTab *db, int size){
    assert(size > 0 && (size & (size - 1)) == 0);
    db->load_factor = 1;
    db->size = 0;
    db->mask = size - 1;
    db->total_size = size;
    db->nodes = calloc(size, sizeof(HNode*));
}

float current_load_factor(HMap *db){
    return (float) db->entries / (float) db->newer->total_size;
}

void scan_tab(HTab *db){
    for (int i = 0; i < db->total_size; i++) {
        HNode *n = db->nodes[i];
        printf("Tab (%i) %p\n", i, n);
        while (n != NULL) {
            Entry *entry = hnode_to_entry(n);
            printf("> %s, %s\n", entry->key, entry->value);
            n = n->next;
        }
    }
}

void scan_map(HMap *HashDB){
    scan_tab(HashDB->newer);
    if (HashDB->older) {
        printf("> older table:\n");
        scan_tab(HashDB->older);
    }
}

int trigger_resize(HMap *HashDB){
    printf("triggering resize\n");
    HashDB->older = HashDB->newer;
    HashDB->newer = calloc(1, sizeof(HTab));
    init_HTab(HashDB->newer, HashDB->older->total_size * 2);
    return 1;
}

HNode *htab_lookup(HTab *db, const char *key, uint64_t hash){
    if (hash == 0) {
        hash = str_hash((uint8_t*)key, strlen(key));
    }
    uint32_t tab = hash & db->mask;
    HNode *n = db->nodes[tab];
    if (n == NULL) {
        return NULL;
    }
    while (n != NULL) {
        Entry *e = hnode_to_entry(n); 
        if (n->hash == hash && strcmp(e->key, key) == 0) {
            return n;
        }
        n = n->next;
    }
    return NULL;
}

HNode *h_lookup(HMap *HashDB, const char *key, uint64_t hash){
    HTab *db = HashDB->newer;
    if (!hash) {
        hash = str_hash((uint8_t*)key, strlen(key));
    }
    HNode *n = htab_lookup(db, key, hash);
    if ( n == NULL && HashDB->older) {
        n = htab_lookup(HashDB->older, key, hash);
    }
    return n;
}

char **all_keys(HMap *HashDB){
    char **keys = calloc(HashDB->entries, sizeof(char*));
    int counter = 0;
    // in new table
    for (int x = 0; x < HashDB->newer->total_size; x++) {
        HNode *node = HashDB->newer->nodes[x];
        while(node){
            Entry *entry = hnode_to_entry(node);
            keys[counter] = strdup(entry->key);
            counter++;
            node = node->next;
        }
    }
    if (!HashDB->older){
        return keys;
    }

    // in older one
    for (int x = 0; x < HashDB->older->total_size; x++) {
        HNode *node = HashDB->older->nodes[x];
        while(node){
            Entry *entry = hnode_to_entry(node);
            keys[counter] = strdup(entry->key);
            counter++;
            node = node->next;
        }
    }
    return keys;
}

// Takes Entry* and inserts it into the hashmap's newer(est) table
int insert_entry(HNode *node, HMap *HashDB){
    HTab *db = HashDB->newer;
    uint32_t tab_pos = node->hash & db->mask;
    HNode *last = db->nodes[tab_pos];

    node->next = last;
    node->prev = NULL;
    if ( last != NULL ) {
        last->prev = node;
    };
    db->nodes[tab_pos] = node;
    db->size++;
    return 1;
};

int detach_node(HNode *node, HTab *db){
    const int tab_pos = node->hash & db->mask;
    HNode *prev = node->prev;
    HNode *next = node->next;

    if (prev) {
        prev->next = next;
    }
    else {
        db->nodes[tab_pos] = next;
    }
    if (next) {
        next->prev = prev;
    }
    return 1;
}

// moves the entries from older table to newer table
void h_rehash(HMap *HashDB){
    HTab *old_db = HashDB->older;
    int work = 0;
    while (old_db && work < k_rehashing_work && HashDB->migrate_cursor < old_db->total_size) {
        HNode *n = old_db->nodes[HashDB->migrate_cursor];
        if (!n) {
            HashDB->migrate_cursor++;
            continue;
        }
        detach_node(n, old_db);
        insert_entry(n, HashDB);
        HashDB->older->size--;
        work++;
    }
    if (old_db && old_db->size == 0) {
        HashDB->migrate_cursor = 0;
        HashDB->older = NULL;
        printf("older table freed\n");
    }
}

HMap *init_hashmap(){
    HMap *db = calloc(1, sizeof(HMap));
    db->migrate_cursor = 0;
    db->entries = 0;
    db->newer = calloc(1, sizeof(HTab));
    db->older = NULL;
    init_HTab(db->newer, ORIGNAL_SIZE);
    return db;
}

