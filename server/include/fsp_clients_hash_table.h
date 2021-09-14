// Tabella hash contenente i client.
// Struttura dati che contiene le informazioni sui client connessi al server.
// Le collisioni vengono risolte mediante concatenamento.
// Le liste sono ordinate in ordine crescente dei sfd associati ai client.

#ifndef FSP_CLIENTS_HASH_TABLE_H
#define FSP_CLIENTS_HASH_TABLE_H

#include <stdio.h>

#include <fsp_client.h>

struct fsp_clients_hash_table {
    // Dimensione della tabella
    size_t size;
    // Numero dei client presenti nella tabella
    unsigned int clients_num;
    // La tabella (vettore)
    struct fsp_client** table;
};

/**
 * \brief Restituisce una nuova tabella hash di dimensione size.
 *
 * \return Una nuova tabella hash di dimensione size,
 *         NULL se non è stato possibile allocare la memoria.
 */
struct fsp_clients_hash_table* fsp_clients_hash_table_new(size_t size);

/**
 * \brief Libera dalla memoria hash_table.
 */
void fsp_clients_hash_table_free(struct fsp_clients_hash_table* hash_table);

/**
 * \brief Aggiunge client nella tabella hash_table.
 *
 * \return 0 in caso di successo,
 *         -1 se hash_table == NULL || client == NULL,
 *         -2 se il client è già presente nella tabella.
 */
int fsp_clients_hash_table_insert(struct fsp_clients_hash_table* hash_table, struct fsp_client* client);

/**
 * \brief Cerca nella tabella hash_table il client con chiave sfd.
 *
 * \return Il client con chiave sfd,
 *         NULL altrimenti.
 */
struct fsp_client* fsp_clients_hash_table_search(const struct fsp_clients_hash_table* hash_table, int sfd);

/**
 * \brief Rimuove il client con chiave sfd dalla tabella e lo restituisce.
 *
 * \return Il client rimosso,
 *         NULL se hash_table == NULL || client non trovato.
 */
struct fsp_client* fsp_clients_hash_table_delete(struct fsp_clients_hash_table* hash_table, int sfd);

/**
 * \brief Rimuove tutti i client presenti nella tabella hash_table.
 *        Se completionHandler != NULL, passa come argomento alla funzione completionHandler ogni client dopo averlo rimosso.
 */
void fsp_clients_hash_table_deleteAll(struct fsp_clients_hash_table* hash_table, void (*completionHandler) (struct fsp_client*));

#endif
