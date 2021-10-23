/*
 * Autore: Francesco Gallicchio
 * Matricola: 579131
 */

#ifndef FSP_CLIENT_H
#define FSP_CLIENT_H

#include <fsp_files_list.h>

struct fsp_client {
    // Socket file descriptor
    int sfd;
    // Il buffer
    void* buf;
    // Dimensione del buffer buf
    size_t size;
    // Lista dei file aperti
    struct fsp_files_list* openedFiles;
    // Nodo successivo
    struct fsp_client* next;
};

/**
 * \brief Alloca memoria per un nuovo client e lo restituisce.
 *        I campi della struttura fsp_client conterranno sfd, buf_size (size)
 *        e un buffer (buf) di dimensione buf_size.
 *        Usare la funzione fsp_client_free per liberare il client dalla memoria.
 *
 * \return Il nuovo client,
 *         NULL se non è stato possibile allocare la memoria.
 */
struct fsp_client* fsp_client_new(int sfd, size_t buf_size);

/**
 * \brief Libera client dalla memoria.
 */
void fsp_client_free(struct fsp_client* client);

#endif
