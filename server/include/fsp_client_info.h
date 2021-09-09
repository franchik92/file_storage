#ifndef FSP_CLIENT_INFO_H
#define FSP_CLIENT_INFO_H

#include <fsp_files_list.h>

struct fsp_client_info {
    // Socket file descriptor
    int sfd;
    // Il buffer
    void* buf;
    // Dimensione del buffer buf
    size_t size;
    // Lista dei file aperti
    struct fsp_files_list* openedFiles;
    // Nodo precedente
    struct fsp_client_info* prev;
    // Nodo successivo
    struct fsp_client_info* next;
};

/**
 * \brief Alloca memoria per un nuovo client e lo restituisce.
 *        I campi della struttura fsp_client_info conterranno sfd e buf_size (campo size)
 *        e un buffer (campo buf) di dimensione buf_size.
 *        Usare la funzione fsp_client_info_free per liberare il client dalla memoria.
 *
 * \return Il nuovo client,
 *         NULL se non è stato possibile allocare la memoria.
 */
struct fsp_client_info* fsp_client_info_new(int sfd, size_t buf_size);

/**
 * \brief Libera client dalla memoria.
 */
void fsp_client_info_free(struct fsp_client_info* client);

#endif
