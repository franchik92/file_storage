// Coda FIFO contenente i file.
// Struttura dati che tiene traccia dell'ordine cronologico con il quale i file vengono salvati sul server.

#ifndef FSP_FILES_QUEUE_H
#define FSP_FILES_QUEUE_H

#include <stdio.h>

#include <fsp_file.h>

struct fsp_files_queue {
    // Testa
    struct fsp_file* head;
    // Coda
    struct fsp_file* tail;
};

/**
 * \brief Aggiunge file in fondo alla coda queue.
 *
 * \return 0 in caso di successo,
 *         -1 se queue == NULL || file == NULL.
 */
int fsp_files_queue_enqueue(struct fsp_files_queue* queue, struct fsp_file* file);

/**
 * \brief Rimuove il file in testa alla coda queue e lo restituisce.
 *
 * \return Il file,
 *         NULL se queue == NULL || la coda queue è vuota.
 */
struct fsp_file* fsp_files_queue_dequeue(struct fsp_files_queue* queue);

/**
 * \brief Rimuove e restituisce il primo file di nome pathname che trova a partire dalla testa della coda queue.
 *
 * \return Il file rimosso,
 *         NULL se queue == NULL || pathname == NULL || file non trovato.
 */
struct fsp_file* fsp_files_queue_remove(struct fsp_files_queue* queue, const char* pathname);

#endif
