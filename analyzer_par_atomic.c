
/*
 * analyzer_par_atomic.c
 * Versão Paralela com Atomic — Analisador de Cache de CDN
 *
 * Funcionamento:
 *   1. Lê o manifest.txt e constrói a tabela hash com hit_count = 0
 *   2. Lê o arquivo de log linha por linha e armazena em um vetor de strings
 *   3. Paraleliza o processamento do vetor de linhas, onde cada thread extrai a URL e incrementa o contador usando #pragma omp atomic
 *   4. Localiza a URL na tabela hash e incrementa o contador
 *   5. Salva os resultados em results.csv
 *
 * Compilação:
 *   gcc -O2 analyzer_par_atomic.c hash_table.c -o analyzer_par_atomic
 *
 * Uso:
 *   ./analyzer_par_atomic log_distribuido.txt
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <omp.h>
#include "hash_table.h"

#define TABLE_SIZE 131071
#define MAX_LINE 256
#define MAX_URL 128
#define INITIAL_CAPACITY 1024

static HashTable* build_table_from_manifest(const char* manifest_path) {
    FILE* fp = fopen(manifest_path, "r");
    if (!fp) { perror("Erro ao abrir manifest.txt"); exit(EXIT_FAILURE); }
    HashTable* ht = ht_create(TABLE_SIZE);
    char url[MAX_URL];
    while (fgets(url, sizeof(url), fp)) {
        url[strcspn(url, "\n")] = '\0';
        if (strlen(url) > 0) ht_put(ht, url);
    }
    fclose(fp);
    return ht;
}

static int parse_url(const char* line, char* url_out, size_t max_len) {
    const char* quote = strchr(line, '"'); if (!quote) return 0; quote++;
    const char* space = strchr(quote, ' '); if (!space) return 0;
    const char* url_start = space + 1;
    const char* url_end = strchr(url_start, ' '); if (!url_end) return 0;
    size_t len = url_end - url_start;
    if (len == 0 || len >= max_len) return 0;
    strncpy(url_out, url_start, len); url_out[len] = '\0';
    return 1;
}

static char** load_log_lines(const char* log_path, long* count) {
    FILE* fp = fopen(log_path, "r");
    if (!fp) { perror("Erro ao abrir arquivo de log"); exit(EXIT_FAILURE); }
    long capacity = INITIAL_CAPACITY;
    char** lines = (char**)malloc(sizeof(char*) * capacity);
    char buffer[MAX_LINE]; long n = 0;
    while (fgets(buffer, sizeof(buffer), fp)) {
        if (n == capacity) {
            capacity *= 2;
            lines = (char**)realloc(lines, sizeof(char*) * capacity);
        }
        lines[n] = (char*)malloc(strlen(buffer) + 1);
        strcpy(lines[n], buffer); n++;
    }
    fclose(fp); *count = n;
    return lines;
}

int main(int argc, char* argv[]) {
    if (argc < 2) { fprintf(stderr, "Uso: %s <arquivo_de_log>\n", argv[0]); return EXIT_FAILURE; }
    
    const char* log_path = argv[1];
    HashTable* ht = build_table_from_manifest("manifest.txt");
    
    long line_count = 0;
    char** lines = load_log_lines(log_path, &line_count);

    long lines_processed = 0;
    double t_start = omp_get_wtime();
    #pragma omp parallel for schedule(dynamic, 512) reduction(+:lines_processed)
    for (long i = 0; i < line_count; i++) {
        char url[MAX_URL];
        if (!parse_url(lines[i], url, sizeof(url))) continue;
        CacheNode* node = ht_get(ht, url);
        if (node) {
            #pragma omp atomic update
            node->hit_count++;
        }
        lines_processed++;
    }
    double t_end = omp_get_wtime();

    printf("Linhas processadas       : %ld\n", lines_processed);
    printf("Tempo de processamento   : %.4f segundos\n", t_end - t_start);

    ht_save_results(ht, "results.csv");
    for (long i = 0; i < line_count; i++) free(lines[i]); free(lines);
    ht_destroy(ht);
    return EXIT_SUCCESS;
}
