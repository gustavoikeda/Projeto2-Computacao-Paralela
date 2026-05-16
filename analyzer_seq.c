/*
 * analyzer_seq.c
 * Versão Sequencial — Analisador de Cache de CDN
 *
 * Funcionamento:
 *   1. Lê o manifest.txt e constrói a tabela hash com hit_count = 0
 *   2. Lê o arquivo de log linha por linha
 *   3. Extrai a URL de cada linha
 *   4. Localiza a URL na tabela hash e incrementa o contador
 *   5. Salva os resultados em results.csv
 *
 * Compilação:
 *   gcc -O2 analyzer_seq.c hash_table.c -o analyzer_seq
 *
 * Uso:
 *   ./analyzer_seq log_distribuido.txt
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hash_table.h"

/* Tamanho da tabela hash — primo próximo de 2^17, reduz colisões */
#define TABLE_SIZE 131071

/* Tamanho máximo de uma linha do log (~256 bytes é suficiente) */
#define MAX_LINE 256

/* Tamanho máximo de uma URL */
#define MAX_URL 128

/* ---------------------------------------------------------------
 * Fase 1: Construção da tabela hash a partir do manifest.txt
 * Cada URL é inserida com hit_count = 0
 * --------------------------------------------------------------- */
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
    /* Cada linha do manifest é uma URL — lemos e inserimos */
    while (fgets(url, sizeof(url), fp)) {
        /* Remove o '\n' do final, se existir */
        url[strcspn(url, "\n")] = '\0';

        if (strlen(url) > 0) {
            ht_put(ht, url);
        }
    }

    fclose(fp);
    return ht;
}

/* ---------------------------------------------------------------
 * Extrai a URL de uma linha de log no formato Apache/Nginx:
 *   127.0.0.1 - - [timestamp] "GET /url HTTP/1.1" 200 1500
 *
 * Estratégia: procura a primeira aspa ", avança "GET " (ou outro
 * método), e copia até o próximo espaço.
 * --------------------------------------------------------------- */
int parse_url(const char* line, char* url_out, size_t max_len) {
    /* Acha a primeira aspa dupla */
    const char* quote = strchr(line, '"');
    if (!quote) return 0;

    /* Avança para depois da aspa */
    quote++;

    /* Pula o método HTTP (GET, POST, etc.) até o espaço */
    const char* space = strchr(quote, ' ');
    if (!space) return 0;

    /* URL começa logo após o espaço */
    const char* url_start = space + 1;

    /* URL termina no próximo espaço */
    const char* url_end = strchr(url_start, ' ');
    if (!url_end) return 0;

    size_t len = url_end - url_start;
    if (len == 0 || len >= max_len) return 0;

    strncpy(url_out, url_start, len);
    url_out[len] = '\0';
    return 1;
}

/* ---------------------------------------------------------------
 * Fase 2: Processa o arquivo de log linha a linha
 * Para cada linha: extrai URL → busca na hash → incrementa contador
 * --------------------------------------------------------------- */
void process_log(HashTable* ht, const char* log_path) {
    FILE* fp = fopen(log_path, "r");
    if (!fp) {
        perror("Erro ao abrir arquivo de log");
        exit(EXIT_FAILURE);
    }

    char line[MAX_LINE];
    char url[MAX_URL];
    long lines_processed = 0;
    long lines_not_found = 0;

    while (fgets(line, sizeof(line), fp)) {
        /* Extrai a URL da linha */
        if (!parse_url(line, url, sizeof(url))) {
            continue; /* Linha malformada, ignora */
        }

        /* Busca o nó na tabela hash */
        CacheNode* node = ht_get(ht, url);

        if (node) {
            node->hit_count++;
        } else {
            /* Não deve acontecer: todas as URLs do log estão no manifest */
            lines_not_found++;
        }

        lines_processed++;
    }

    fclose(fp);

    printf("Linhas processadas : %ld\n", lines_processed);
    if (lines_not_found > 0) {
        printf("URLs nao encontradas na hash: %ld\n", lines_not_found);
    }
}

/* ---------------------------------------------------------------
 * main
 * --------------------------------------------------------------- */
int main(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Uso: %s <arquivo_de_log>\n", argv[0]);
        fprintf(stderr, "Exemplo: ./analyzer_seq log_distribuido.txt\n");
        return EXIT_FAILURE;
    }

    const char* log_path      = argv[1];
    const char* manifest_path = "manifest.txt";
    const char* output_path   = "results.csv";

    /* --- Fase 1: Constrói a tabela hash --- */
    printf("Carregando manifest: %s\n", manifest_path);
    HashTable* ht = build_table_from_manifest(manifest_path);
    printf("Tabela hash criada com %d buckets\n", TABLE_SIZE);

    /* --- Fase 2: Processa o log --- */
    printf("Processando log: %s\n", log_path);
    process_log(ht, log_path);

    /* --- Fase 3: Salva resultados --- */
    printf("Salvando resultados em: %s\n", output_path);
    ht_save_results(ht, output_path);

    /* --- Libera memória --- */
    ht_destroy(ht);

    printf("Concluido.\n");
    return EXIT_SUCCESS;
}