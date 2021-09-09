// Coda FIFO contenente i nomi dei file.

#ifndef FSP_PATHNAMES_QUEUE_H
#define FSP_PATHNAMES_QUEUE_H

#include <stdio.h>
#include <fsp_file.h>

struct fsp_file_pathname {
    const char* pathname;
    struct fsp_file_pathname* next;
    struct fsp_file_pathname* prev;
}

struct fsp_pathnames_queue {
    struct fsp_file_pathname* head;
    struct fsp_file_pathname* tail;
};

/**
 * \brief Alloca memoria per una nuova coda vuota e la restituisce.
 *
 * \return Una coda vuota,
 *         NULL se non è stato possibile allocare la memoria.
 */
struct fsp_pathnames_queue* fsp_pathnames_queue_new();

/**
 * \brief Libera dalla memoria la coda queue assieme a tutti gli elementi in essa contenuti.
 */
void fsp_pathnames_queue_free(struct fsp_pathnames_queue* queue);

/**
 * \brief Alloca memoria per un nuovo elemento della coda con pathname e lo restituisce.
 *
 * \return Un nuovo elemento per la coda con pathname,
 *         NULL se non è stato possibile allocare la memoria.
 */
struct fsp_file_pathname* fsp_pathnames_queue_newFilePathname(const char* pathname);

/**
 * \brief Libera dalla memoria file_pathname.
 */
void fsp_pathnames_queue_freeFilePathname(struct fsp_file_pathname* file_pathname);

/**
 * \brief Aggiunge file alla coda queue.
 *
 * \return 0 in caso di successo,
 *         -1 se queue == NULL || file == NULL,
 *         -2 se non è stato possibile allocare la memoria.
 */
int fsp_pathnames_queue_enqueue(struct fsp_pathnames_queue* queue, struct fsp_pathname* file);

/**
 * \brief Rimuove il file in testa alla coda e lo restituisce.
 *
 * \return Il file,
 *         NULL se queue == NULL o la coda è vuota.
 */
struct fsp_file_pathname* fsp_pathnames_queue_dequeue(struct fsp_pathnames_queue* queue);

/**
 * \brief Rimuove il file di nome pathname dalla coda partendo dalla testa per la sua ricerca.
 */
void fsp_pathnames_queue_remove(const char* pathname);



#endif
