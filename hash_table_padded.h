#ifndef HASH_TABLE_PADDED_H
#define HASH_TABLE_PADDED_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Struct com padding — cada nó ocupa exatamente 64 bytes (1 linha de cache)
 * Evita false sharing entre threads que acessam nós vizinhos */
typedef struct CacheNode {
    char* url;                // 8 bytes
    long  hit_count;          // 8 bytes
    struct CacheNode* next;   // 8 bytes
    long  padding[5];         // 40 bytes → total: 64 bytes
} CacheNode;

typedef struct {
    size_t size;
    CacheNode** table;
} HashTable;

HashTable* ht_create(size_t size);
void       ht_destroy(HashTable* ht);
void       ht_put(HashTable* ht, const char* url);
CacheNode* ht_get(HashTable* ht, const char* url);
void       ht_save_results(HashTable* ht, const char* filename);
void       ht_print(HashTable* ht);

#endif
