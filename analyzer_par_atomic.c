#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hash_table.h"
#include <omp.h>

#define TABLE_SIZE 131071
#define MAX_LINE 256
#define MAX_URL 128

HashTable* build_table_from_manifest(const char* manifest_path) {
    FILE* fp = fopen(manifest_path, "r");
    if (!fp) {
        perror("Erro ao abrir manifest.txt");
        exit(EXIT_FAILURE);
    }

    HashTable* ht = ht_create(TABLE_SIZE);
    if (!ht) {
        fprintf(stderr, "Erro ao criar tabela hash\n");
        fclose(fp);
        exit(EXIT_FAILURE);
    }

    char url[MAX_URL];
    while (fgets(url, sizeof(url), fp)) {
        url[strcspn(url, "\n")] = '\0';
        if (strlen(url) > 0) {
            ht_put(ht, url);
        }
    }

    fclose(fp);
    return ht;
}

int parse_url(const char* line, char* url_out, size_t max_len) {
    const char* quote = strchr(line, '"');
    if (!quote) return 0;
    quote++;
    const char* space = strchr(quote, ' ');
    if (!space) return 0;
    const char* url_start = space + 1;
    const char* url_end = strchr(url_start, ' ');
    if (!url_end) return 0;
    size_t len = url_end - url_start;
    if (len == 0 || len >= max_len) return 0;
    strncpy(url_out, url_start, len);
    url_out[len] = '\0';
    return 1;
}

void process_log(HashTable* ht, const char* log_path) {
    FILE* fp = fopen(log_path, "r");
    if (!fp) { perror("Erro ao abrir log"); exit(EXIT_FAILURE); }

    char** lines = malloc(sizeof(char*) * 10000000);
    long total = 0;
    char buffer[MAX_LINE];

    while (fgets(buffer, sizeof(buffer), fp)) {
        lines[total] = strdup(buffer);
        total++;
    }
    fclose(fp);

    #pragma omp parallel for schedule(static)
    for (long i = 0; i < total; i++) {
        char url[MAX_URL];
        if (!parse_url(lines[i], url, sizeof(url)))
            continue;
        CacheNode* node = ht_get(ht, url);
        if (node) {
            #pragma omp atomic update
            node->hit_count++;
        }
    }

    for (long i = 0; i < total; i++) free(lines[i]);
    free(lines);

    printf("Linhas processadas: %ld\n", total);
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Uso: %s <arquivo_de_log>\n", argv[0]);
        fprintf(stderr, "Exemplo: ./analyzer_par_atomic log_distribuido.txt\n");
        return EXIT_FAILURE;
    }

    const char* log_path      = argv[1];
    const char* manifest_path = "manifest.txt";
    const char* output_path   = "results.csv";

    printf("Threads: %d\n", omp_get_max_threads());

    printf("Carregando manifest: %s\n", manifest_path);
    HashTable* ht = build_table_from_manifest(manifest_path);
    printf("Tabela hash criada com %d buckets\n", TABLE_SIZE);

    printf("Processando log: %s\n", log_path);
    double t_start = omp_get_wtime();

    process_log(ht, log_path);

    double t_end = omp_get_wtime();
    printf("Tempo de processamento: %.4f segundos\n", t_end - t_start);

    printf("Salvando resultados em: %s\n", output_path);
    ht_save_results(ht, output_path);

    ht_destroy(ht);

    printf("Concluido.\n");
    return EXIT_SUCCESS;
}