#pragma once
#include <stdbool.h>
#include <unistd.h>
#include <stdint.h>

#define MAX_LOAD_FACTOR 1
#define ORIGNAL_SIZE 4
#define k_rehashing_work 20

typedef struct HNode {
    uint64_t hash;
    struct HNode *next;
    struct HNode *prev;
} HNode;

typedef struct HTab {
    // size - 1
    int mask;
    float load_factor;
    int size;
    // total size of the hashmap ( 2^n )
    int total_size;
    HNode **nodes;
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
void scan_tab(HTab *db, void (*print_val)(HNode*));
// Prints the contents of the hashmap
void scan_map(HMap *HashDB, void (*print_val)(HNode*));


// CRUD 

// adds (HNode*) to the hashmap's newer table
int insert_node(HNode *node, HMap *HashDB);

// Looks up an key in the table
HNode *htab_lookup(HTab *db, const char *key, bool (*eq)(const char*, HNode*));
//
// // Looks up an key in the hashmap
HNode *h_lookup(HMap *HashDB, const char *key, bool (*eq)(const char*, HNode*));

char **all_keys(HMap *HashDB);

// Node*, HTab* -> detaches node from the table
int detach_node(HNode *node, HTab *db);

// moves the entries from old table to new table
void h_rehash(HMap *HashDB);

void possibly_resize(HMap *map);
