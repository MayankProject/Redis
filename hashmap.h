#pragma once
#include <unistd.h>
#include <stdint.h>

#define MAX_LOAD_FACTOR 1
#define ORIGNAL_SIZE 4
#define k_rehashing_work 20

typedef struct HNode {
    uint64_t hash;
    struct Entry *next;
    struct Entry *prev;
} HNode;

typedef struct Entry {
    char *key;
    char *value;
    struct HNode node;
} Entry;

typedef struct HTab {
    // size - 1
    int mask;
    int load_factor;
    int size;
    // total size of the hashmap ( 2^n )
    int total_size;
    Entry **entries;
} HTab;

typedef struct HMap {
    HTab *older;
    HTab *newer;
    int entries;
    int migrate_cursor;
} HMap;


// Initializes a new hashmap of two tables
HMap *init_hashmap();

// Initializes a new table
void init_HTab(HTab *db, int size);

// Returns the current load factor of the hashmap's newer table
float current_load_factor(HMap *db);

// FNV hash
uint64_t str_hash(const uint8_t *data, size_t len);

// Starts the rehashing process
int trigger_resize(HMap *HashDB);

// Prints the contents of the table
void scan_tab(HTab *db);
// Prints the contents of the hashmap
void scan_map(HMap *HashDB);


// CRUD 

// adds (Entry*) to the hashmap's newer table
int insert_entry(Entry *entry, HMap *HashDB);

// Looks up an entry in the table
Entry *htab_lookup(HTab *db, const char *key, uint64_t hash);

// Looks up an entry in the hashmap
Entry *h_lookup(HMap *HashDB, const char *key, uint64_t hash);

// Node*, HTab* -> detaches node from the table
int detach_node(HNode *node, HTab *db);

// moves the entries from old table to new table
void h_rehash(HMap *HashDB);

