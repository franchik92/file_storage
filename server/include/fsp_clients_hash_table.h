// Tabella hash contenente le informazioni sui client connessi al server.

#ifndef FSP_CLIENTS_HASH_TABLE_H
#define FSP_CLIENTS_HASH_TABLE_H

#include <stdio.h>
#include <fsp_files_list.h>

struct client_info {
    // Socket file descriptor
    int sfd;
    // Il buffer
    void* buf;
    // Dimensione del buffer buf
    size_t size;
    // Lista dei file aperti
    struct fsp_files_list* openedFiles;
    struct client_info* next;
};

struct clients_hash_table {
    // Dimensione della tabella
    size_t size;
    // Numero dei client presenti nella tabella
    unsigned int clients;
    // La tabella (vettore)
    struct client_info** table;
};

/**
 * \brief Restituisce una nuova tabella hash di dimensione size.
 *
 * \return Una nuova tabella hash di dimensione size,
 *         NULL se non è stato possibile allocare la memoria.
 */
struct clients_hash_table* fsp_clients_hash_table_new(size_t size);

/**
 * \brief Libera dalla memoria hash_table e tutti gli elementi (i client) contenuti in essa.
 */
void fsp_clients_hash_table_free(struct clients_hash_table* hash_table);

/**
 * \brief Aggiunge un nuovo client (con sfd, un buffer di dimenzione size e una lista vuota di file aperti)
 *        nella tabella hash_table.
 *
 * \return 0 in caso di successo,
 *         -1 se hash_table == NULL,
 *         -2 se non è stato possibile allocare la memoria,
 *         -3 se il client è già presente nella tabella.
 */
int fsp_clients_hash_table_insert(struct clients_hash_table* hash_table, int sfd, size_t buf_size);

/**
 * \brief Cerca nella tabella hash_table il client con chiave sfd.
 *        Qualsiasi modifica al campo next della struttura restituita potrebbe compromettere il corretto
 *        funzionamento della tabella hash_table. Non invocare la free sulla struttura restituita.
 *
 * \return Il client con chiave sfd,
 *         NULL altrimenti.
 */
struct client_info* fsp_clients_hash_table_search(struct clients_hash_table* hash_table, int sfd);

/**
 * \brief Rimuove il client con chiave sfd dalla tabella.
 *
 * \return 0 in caso di successo,
 *         -1 se hash_table == NULL,
 *         -2 se il client non è presente nella tabella.
 */
int fsp_clients_hash_table_delete(struct clients_hash_table* hash_table, int sfd);

#endif
