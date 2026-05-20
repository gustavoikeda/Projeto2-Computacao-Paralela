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

static int parse_url(const char* line, char* url_out, size_t max_len) {
    const char* quote = strchr(line, '"');
    if (!quote) return 0;
    quote++;

    const char* space = strchr(quote, ' ');
    if (!space) return 0;

    const char* url_start = space + 1;
    const char* url_end   = strchr(url_start, ' ');
    if (!url_end) return 0;

    size_t len = url_end - url_start;
    if (len == 0 || len >= max_len) return 0;

    strncpy(url_out, url_start, len);
    url_out[len] = '\0';
    return 1;
}

static char** load_log_lines(const char* log_path, long* count) {
    FILE* fp = fopen(log_path, "r");
    if (!fp) {
        perror("Erro ao abrir arquivo de log");
        exit(EXIT_FAILURE);
    }

    long capacity = INITIAL_CAPACITY;
    char** lines  = (char**)malloc(sizeof(char*) * capacity);
    if (!lines) {
        perror("Erro ao alocar vetor de linhas");
        fclose(fp);
        exit(EXIT_FAILURE);
    }

    char buffer[MAX_LINE];
    long n = 0;

    while (fgets(buffer, sizeof(buffer), fp)) {
        if (n == capacity) {
            capacity *= 2;
            char** tmp = (char**)realloc(lines, sizeof(char*) * capacity);
            if (!tmp) {
                perror("Erro ao realocar vetor de linhas");
                fclose(fp);
                exit(EXIT_FAILURE);
            }
            lines = tmp;
        }

        lines[n] = (char*)malloc(strlen(buffer) + 1);
        if (!lines[n]) {
            perror("Erro ao alocar linha");
            fclose(fp);
            exit(EXIT_FAILURE);
        }
        strcpy(lines[n], buffer);
        n++;
    }

    fclose(fp);
    *count = n;
    return lines;
}

static void free_log_lines(char** lines, long count) {
    for (long i = 0; i < count; i++) {
        free(lines[i]);
    }
    free(lines);
}

static void process_log_parallel(HashTable* ht, const char* log_path) {
    long line_count = 0;
    char** lines = load_log_lines(log_path, &line_count);

    long lines_processed = 0;
    long lines_not_found = 0;

    #pragma omp parallel for           \
        schedule(dynamic, 512)         \
        reduction(+:lines_processed)   \
        reduction(+:lines_not_found)
    for (long i = 0; i < line_count; i++) {
        char url[MAX_URL];

        if (!parse_url(lines[i], url, sizeof(url))) {
            continue;
        }

        CacheNode* node = ht_get(ht, url);

        if (node) {
            #pragma omp critical
            {
                node->hit_count++;
            }
        } else {
            lines_not_found++;
        }

        lines_processed++;
    }

    printf("Linhas processadas       : %ld\n", lines_processed);
    if (lines_not_found > 0) {
        printf("URLs nao encontradas na hash : %ld\n", lines_not_found);
    }

    free_log_lines(lines, line_count);
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Uso: %s <arquivo_de_log>\n", argv[0]);
        fprintf(stderr, "Exemplo: ./analyzer_par_critical log_distribuido.txt\n");
        return EXIT_FAILURE;
    }

    const char* log_path      = argv[1];
    const char* manifest_path = "manifest.txt";
    const char* output_path   = "results.csv";

    printf("Threads disponiveis: %d\n", omp_get_max_threads());

    printf("Carregando manifest: %s\n", manifest_path);
    HashTable* ht = build_table_from_manifest(manifest_path);
    printf("Tabela hash criada com %d buckets\n", TABLE_SIZE);

    printf("Processando log: %s\n", log_path);
    double t_start = omp_get_wtime();
    process_log_parallel(ht, log_path);
    double t_end = omp_get_wtime();
    printf("Tempo de processamento   : %.4f segundos\n", t_end - t_start);

    printf("Salvando resultados em: %s\n", output_path);
    ht_save_results(ht, output_path);

    ht_destroy(ht);

    printf("Concluido.\n");
    return EXIT_SUCCESS;
}