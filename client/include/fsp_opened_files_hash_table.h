/*
 * Autore: Francesco Gallicchio
 * Matricola: 579131
 */

// Tabella hash contenente i file aperti.
// Struttura dati usata per tenere traccia dei file aperti dal client.
// Le collisioni vengono risolte mediante concatenamento.
// Le liste sono ordinate lessicograficamente in ordine crescente dei nomi dei file.

#ifndef FSP_OPENED_FILES_HASH_TABLE_H
#define FSP_OPENED_FILES_HASH_TABLE_H

#include <stdio.h>

#include <fsp_opened_file.h>

struct fsp_opened_files_hash_table {
    // Dimensione della tabella
    size_t size;
    // Numero dei file presenti nella tabella
    unsigned int files_num;
    // La tabella (vettore)
    struct fsp_opened_file** table;
};

/**
 * \brief Restituisce una nuova tabella hash di dimensione size.
 *
 * \return Una nuova tabella hash di dimensione size,
 *         NULL se non è stato possibile allocare la memoria.
 */
struct fsp_opened_files_hash_table* fsp_opened_files_hash_table_new(size_t size);

/**
 * \brief Libera dalla memoria hash_table.
 */
void fsp_opened_files_hash_table_free(struct fsp_opened_files_hash_table* hash_table);

/**
 * \brief Aggiunge il file aperto di nome filename con i relativi flags nella tabella hash_table.
 *
 * \return 0 in caso di successo,
 *         -1 se hash_table == NULL || filename == NULL,
 *         -2 se non è stato possibile allocare la memoria per il file aperto,
 *         -3 se opened_file è già presente nella tabella.
 */
int fsp_opened_files_hash_table_insert(struct fsp_opened_files_hash_table* hash_table, const char* filename, int flags);

/**
 * \brief Cerca nella tabella hash_table il file aperto opened_file di nome filename.
 *
 * \return Il file aperto opened_file di nome filename,
 *         NULL se hash_table == NULL || filename == NULL || file aperto non trovato.
 */
struct fsp_opened_file* fsp_opened_files_hash_table_search(const struct fsp_opened_files_hash_table* hash_table, const char* filename);

/**
 * \brief Rimuove il file aperto di nome filename dalla tabella hash_table.
 */
void fsp_opened_files_hash_table_delete(struct fsp_opened_files_hash_table* hash_table, const char* filename);

/**
 * \brief Rimuove tutti i file aperti nella tabella hash_table.
 *        Se completionHandler != NULL, passa come argomento alla funzione completionHandler ogni nome del file dopo averlo rimosso.
 */
void fsp_opened_files_hash_table_deleteAll(struct fsp_opened_files_hash_table* hash_table, void (*completionHandler) (const char* filename));

#endif
