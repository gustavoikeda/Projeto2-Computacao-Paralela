#include "hash_table_padded.h"

#define FNV_OFFSET_BASIS 2166136261UL
#define FNV_PRIME 16777619UL

static size_t hash_djb2(const char* str, size_t size) {
    unsigned long hash = 5381;
    int c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c;
    }
    return hash % size;
}

static CacheNode* create_node(const char* url) {
    CacheNode* node = (CacheNode*)malloc(sizeof(CacheNode));
    if (!node) { perror("Erro ao alocar CacheNode"); exit(EXIT_FAILURE); }
    node->url = (char*)malloc(strlen(url) + 1);
    if (!node->url) { perror("Erro ao alocar string da URL"); free(node); exit(EXIT_FAILURE); }
    strcpy(node->url, url);
    node->hit_count = 0;
    node->next = NULL;
    return node;
}

HashTable* ht_create(size_t size) {
    if (size < 1) { fprintf(stderr, "Tamanho deve ser >= 1\n"); return NULL; }
    HashTable* ht = (HashTable*)malloc(sizeof(HashTable));
    if (!ht) { perror("Erro ao alocar HashTable"); return NULL; }
    ht->table = (CacheNode**)calloc(size, sizeof(CacheNode*));
    if (!ht->table) { perror("Erro ao alocar buckets"); free(ht); return NULL; }
    ht->size = size;
    return ht;
}

void ht_destroy(HashTable* ht) {
    if (!ht) return;
    for (size_t i = 0; i < ht->size; i++) {
        CacheNode* current = ht->table[i];
        while (current) {
            CacheNode* next = current->next;
            free(current->url);
            free(current);
            current = next;
        }
    }
    free(ht->table);
    free(ht);
}

void ht_put(HashTable* ht, const char* url) {
    if (!ht || !url) return;
    size_t index = hash_djb2(url, ht->size);
    CacheNode* current = ht->table[index];
    while (current) {
        if (strcmp(current->url, url) == 0) return;
        current = current->next;
    }
    CacheNode* new_node = create_node(url);
    new_node->next = ht->table[index];
    ht->table[index] = new_node;
}

CacheNode* ht_get(HashTable* ht, const char* url) {
    if (!ht || !url) return NULL;
    size_t index = hash_djb2(url, ht->size);
    CacheNode* current = ht->table[index];
    while (current) {
        if (strcmp(current->url, url) == 0) return current;
        current = current->next;
    }
    return NULL;
}

void ht_save_results(HashTable* ht, const char* filename) {
    if (!ht || !filename) return;
    FILE* fp = fopen(filename, "w");
    if (!fp) { perror("Erro ao abrir arquivo de resultados"); return; }
    for (size_t i = 0; i < ht->size; i++) {
        CacheNode* current = ht->table[i];
        while (current) {
            fprintf(fp, "%s,%ld\n", current->url, current->hit_count);
            current = current->next;
        }
    }
    fclose(fp);
}

void ht_print(HashTable* ht) {
    if (!ht) return;
    printf("--- Tabela Hash (Size: %zu) ---\n", ht->size);
    for (size_t i = 0; i < ht->size; i++) {
        CacheNode* current = ht->table[i];
        while (current) {
            printf("Bucket[%zu]: \"%s\" (%ld)\n", i, current->url, current->hit_count);
            current = current->next;
        }
    }
}
