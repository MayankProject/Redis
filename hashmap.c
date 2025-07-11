#include "hashmap.h"
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
    db->entries = calloc(size, sizeof(Entry*));
}

float current_load_factor(HMap *db){
    return (float) db->entries / (float) db->newer->total_size;
}

void scan_tab(HTab *db){
    for (int i = 0; i < db->total_size; i++) {
        Entry *e = db->entries[i];
        printf("Tab (%i) %p\n", i, e);
        while (e != NULL) {
            printf("> %s, %s\n", e->key, e->value);
            e = e->node.next;
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
    HashDB->older = HashDB->newer;
    HashDB->newer = calloc(1, sizeof(HTab));
    init_HTab(HashDB->newer, HashDB->older->total_size * 2);
    return 1;
}

Entry *htab_lookup(HTab *db, const char *key, uint64_t hash){
    if (hash == 0) {
        hash = str_hash((uint8_t*)key, strlen(key));
    }
    uint32_t tab = hash & db->mask;
    Entry *e = db->entries[tab];
    if (e == NULL) {
        return NULL;
    }
    while (e != NULL) {
        if (e->node.hash == hash && strcmp(e->key, key) == 0) {
            return e;
        }
        e = e->node.next;
    }
    return NULL;
}

Entry *h_lookup(HMap *HashDB, const char *key, uint64_t hash){
    HTab *db = HashDB->newer;
    if (!hash) {
        hash = str_hash((uint8_t*)key, strlen(key));
    }
    Entry *e = htab_lookup(db, key, hash);
    if ( e == NULL && HashDB->older) {
        e = htab_lookup(HashDB->older, key, hash);
    }
    return e;
}

char **all_keys(HMap *HashDB){
    char **keys = calloc(HashDB->entries, sizeof(char*));
    int counter = 0;
    // in new table
    for (int x = 0; x < HashDB->newer->total_size; x++) {
        Entry *entry = HashDB->newer->entries[x];
        while(entry){
            keys[counter] = strdup(entry->key);
            counter++;
            entry = entry->node.next;
        }
    }
    if (!HashDB->older){
        return keys;
    }

    // in older one
    for (int x = 0; x < HashDB->older->total_size; x++) {
        Entry *entry = HashDB->older->entries[x];
        while(entry){
            keys[counter] = entry->key;
            counter++;
            entry = entry->node.next;
        }
    }
    return keys;
}

// Takes Entry* and inserts it into the hashmap's newer(est) table
int insert_entry(Entry *entry, HMap *HashDB){
    HTab *db = HashDB->newer;
    const char *key = entry->key;
    uint64_t hash = str_hash((uint8_t*)key, strlen(key));
    uint32_t tab_pos = hash & db->mask;
    entry->node.hash = hash;
    Entry *last = db->entries[tab_pos];

    entry->node.next = last;
    entry->node.prev = NULL;
    if ( last != NULL ) {
        last->node.prev = entry;
    }
    db->entries[tab_pos] = entry;
    db->size++;
    return 1;
};


int detach_node(HNode *node, HTab *db){
    const int tab_pos = node->hash & db->mask;
    Entry *prev = node->prev;
    Entry *next = node->next;

    if (prev) {
        prev->node.next = next;
    }
    else {
        db->entries[tab_pos] = next;
    }
    if (next) {
        next->node.prev = prev;
    }
    return 1;
}

// moves the entries from older table to newer table
void h_rehash(HMap *HashDB){
    HTab *old_db = HashDB->older;
    int work = 0;
    while (old_db && work < k_rehashing_work && HashDB->migrate_cursor < old_db->total_size) {
        Entry *entry = old_db->entries[HashDB->migrate_cursor];
        if (!entry) {
            HashDB->migrate_cursor++;
            continue;
        }
        detach_node(&entry->node, old_db);
        insert_entry(entry, HashDB);
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

