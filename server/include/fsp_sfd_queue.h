// Coda FIFO contenente i socket file descriptor.
// Struttura dati usata per comunicare i socket file descriptor ai thread worker.

#ifndef FSP_SFD_QUEUE_H
#define FSP_SFD_QUEUE_H

#include <stdio.h>

struct fsp_sfd_queue {
    size_t len;
    unsigned int head;
    unsigned int tail;
    int* arr;
};

/**
 * \brief Restituisce una nuova coda di lunghezza len.
 *
 * \return Una nuova coda di lunghezza len,
 *         NULL se non è stato possibile allocare la memoria.
 */
struct fsp_sfd_queue* fsp_sfd_queue_new(size_t len);

/**
 * \brief Libera la coda queue dalla memoria.
 */
void fsp_sfd_queue_free(struct fsp_sfd_queue* queue);

/**
 * \brief Aggiunge sfd in fondo alla coda queue.
 *
 * \return 0 in caso di successo.
 *         -1 in caso di overflow.
 */
int fsp_sfd_queue_enqueue(struct fsp_sfd_queue* queue, int sfd);

/**
 * \brief Restituisce l'elemento in testa alla coda queue.
 *
 * \return 0 in caso di successo.
 *         -1 in caso di underflow.
 */
int fsp_sfd_queue_dequeue(struct fsp_sfd_queue* queue);

/**
 * \brief Controlla se la coda queue è vuota o meno.
 *
 * \return 0 se non è vuota,
 *         1 se è vuota.
 */
int fsp_sfd_queue_isEmpty(const struct fsp_sfd_queue* queue);

#endif
