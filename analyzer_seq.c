#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <omp.h>
#include "hash_table.h"

#define TABLE_SIZE 131071
#define MAX_LINE 256
#define MAX_URL  128

static size_t hash_djb2(const char* str, size_t size) {
    unsigned long hash = 5381;
    int c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c;
    }
    return hash % size;
}

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
    const char* url_end   = strchr(url_start, ' ');
    if (!url_end) return 0;

    size_t len = url_end - url_start;
    if (len == 0 || len >= max_len) return 0;

    strncpy(url_out, url_start, len);
    url_out[len] = '\0';
    return 1;
}

char** load_lines(const char* log_path, long* count) {
    FILE* fp = fopen(log_path, "r");
    if (!fp) {
        perror("Erro ao abrir arquivo de log");
        exit(EXIT_FAILURE);
    }

    long total = 0;
    char tmp[MAX_LINE];
    while (fgets(tmp, sizeof(tmp), fp)) {
        total++;
    }

    char** lines = (char**)malloc(sizeof(char*) * total);
    if (!lines) {
        perror("Erro ao alocar vetor de linhas");
        fclose(fp);
        exit(EXIT_FAILURE);
    }

    rewind(fp);
    long i = 0;
    while (i < total && fgets(tmp, sizeof(tmp), fp)) {
        lines[i] = (char*)malloc(strlen(tmp) + 1);
        if (!lines[i]) {
            perror("Erro ao alocar linha");
            fclose(fp);
            exit(EXIT_FAILURE);
        }
        strcpy(lines[i], tmp);
        i++;
    }

    fclose(fp);
    *count = total;
    return lines;
}

void free_lines(char** lines, long count) {
    for (long i = 0; i < count; i++) {
        free(lines[i]);
    }
    free(lines);
}

omp_lock_t* create_locks(size_t count) {
    omp_lock_t* locks = (omp_lock_t*)malloc(sizeof(omp_lock_t) * count);
    if (!locks) {
        perror("Erro ao alocar locks");
        exit(EXIT_FAILURE);
    }

    for (size_t i = 0; i < count; i++) {
        omp_init_lock(&locks[i]);
    }

    return locks;
}

void destroy_locks(omp_lock_t* locks, size_t count) {
    for (size_t i = 0; i < count; i++) {
        omp_destroy_lock(&locks[i]);
    }
    free(locks);
}

void process_log_parallel(HashTable* ht, char** lines, long count,
                           omp_lock_t* locks) {
    long lines_not_found = 0;

    #pragma omp parallel for schedule(dynamic, 1024) reduction(+:lines_not_found)
    for (long i = 0; i < count; i++) {
        char url[MAX_URL];

        if (!parse_url(lines[i], url, sizeof(url))) {
            continue;
        }

        size_t bucket = hash_djb2(url, TABLE_SIZE);

        omp_set_lock(&locks[bucket]);

        CacheNode* node = ht_get(ht, url);
        if (node) {
            node->hit_count++;
        } else {
            lines_not_found++;
        }

        omp_unset_lock(&locks[bucket]);
    }

    if (lines_not_found > 0) {
        printf("URLs nao encontradas na hash: %ld\n", lines_not_found);
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Uso: %s <arquivo_de_log>\n", argv[0]);
        fprintf(stderr, "Exemplo: ./analyzer_par_lock log_distribuido.txt\n");
        return EXIT_FAILURE;
    }

    const char* log_path      = argv[1];
    const char* manifest_path = "manifest.txt";
    const char* output_path   = "results.csv";

    printf("Threads em uso: %d\n", omp_get_max_threads());

    printf("Carregando manifest: %s\n", manifest_path);
    HashTable* ht = build_table_from_manifest(manifest_path);
    printf("Tabela hash criada com %d buckets\n", TABLE_SIZE);

    printf("Inicializando %d locks...\n", TABLE_SIZE);
    omp_lock_t* locks = create_locks(TABLE_SIZE);

    printf("Carregando log em memoria: %s\n", log_path);
    long count = 0;
    char** lines = load_lines(log_path, &count);
    printf("Linhas carregadas: %ld\n", count);

    printf("Processando com bucket lock...\n");
    double t_start = omp_get_wtime();

    process_log_parallel(ht, lines, count, locks);

    double t_end = omp_get_wtime();
    printf("Tempo de processamento: %.4f segundos\n", t_end - t_start);

    printf("Salvando resultados em: %s\n", output_path);
    ht_save_results(ht, output_path);

    destroy_locks(locks, TABLE_SIZE);
    free_lines(lines, count);
    ht_destroy(ht);

    printf("Concluido.\n");
    return EXIT_SUCCESS;
}