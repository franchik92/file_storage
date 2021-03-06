/*
 * Autore: Francesco Gallicchio
 * Matricola: 579131
 */

// Tabella hash contenente i file.
// Struttura dati usata per mantenere in memoria tutti i file salvati sul server.
// Le collisioni vengono risolte mediante concatenamento.
// Le liste sono ordinate lessicograficamente in ordine crescente dei nomi dei file.

#ifndef FSP_FILES_HASH_TABLE_H
#define FSP_FILES_HASH_TABLE_H

#include <stdio.h>

#include <fsp_file.h>

struct fsp_files_hash_table {
    // Dimensione della tabella
    size_t size;
    // Numero dei file presenti nella tabella
    unsigned int files_num;
    // La tabella (vettore)
    struct fsp_file** table;
};

struct fsp_files_hash_table_iterator {
    // Indice della tabella
    int index;
    // File successivo
    struct fsp_file* next;
    // Dimensione della tabella
    size_t table_size;
    // La tabella (vettore)
    struct fsp_file** table;
};

/**
 * \brief Restituisce una nuova tabella hash di dimensione size.
 *
 * \return Una nuova tabella hash di dimensione size,
 *         NULL se non è stato possibile allocare la memoria.
 */
struct fsp_files_hash_table* fsp_files_hash_table_new(size_t size);

/**
 * \brief Libera hash_table dalla memoria.
 */
void fsp_files_hash_table_free(struct fsp_files_hash_table* hash_table);

/**
 * \brief Aggiunge file nella tabella hash_table.
 *
 * \return 0 in caso di successo,
 *         -1 se hash_table == NULL || file == NULL,
 *         -2 se il file è già presente nella tabella.
 */
int fsp_files_hash_table_insert(struct fsp_files_hash_table* hash_table, struct fsp_file* file);

/**
 * \brief Cerca nella tabella hash_table il file con nome pathname e lo restituisce.
 *
 * \return Il file,
 *         NULL se hash_table == NULL || pathname == NULL || file non trovato.
 */
struct fsp_file* fsp_files_hash_table_search(const struct fsp_files_hash_table* hash_table, const char* pathname);

/**
 * \brief Rimuove il file con nome pathname dalla tabella hash_table e lo restituisce.
 *
 * \return Il file rimosso,
 *         NULL se hash_table == NULL || pathname == NULL || file non trovato.
 */
struct fsp_file* fsp_files_hash_table_delete(struct fsp_files_hash_table* hash_table, const char* pathname);

/**
 * \brief Rimuove tutti i file presenti nella tabella hash_table.
 *        Se completionHandler != NULL, passa come argomento alla funzione completionHandler ogni file dopo averlo rimosso.
 */
void fsp_files_hash_table_deleteAll(struct fsp_files_hash_table* hash_table, void (*completionHandler) (struct fsp_file*));

/**
 * \brief Restituisce un iteratore per la tabella hash_table.
 *        Dopo l'utilizzo liberare l'iteratore dalla memoria con free().
 *
 * \return Un iteratore,
 *         NULL se hash_table == NULL || non è stato possibile allocare la memoria.
 */
struct fsp_files_hash_table_iterator* fsp_files_hash_table_getIterator(const struct fsp_files_hash_table* hash_table);

/**
 * \brief Restituisce il file successivo in iterator.
 *
 * \return il file,
 *         NULL se iterator == NULL || non ci sono altri file.
 */
struct fsp_file* fsp_files_hash_table_getNext(struct fsp_files_hash_table_iterator* iterator);

#endif
