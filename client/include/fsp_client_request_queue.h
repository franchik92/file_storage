/*
 * Autore: Francesco Gallicchio
 * Matricola: 579131
 */

// Ogni richiesta del client viene salvata in una struttura fsp_client_request e
// in seguito aggiunta ad una coda definita dalla struttura fsp_client_request_queue.

#ifndef FSP_CLIENT_REQUEST_QUEUE_H
#define FSP_CLIENT_REQUEST_QUEUE_H

struct fsp_client_request {
    // Tipo di richiesta: w, W, r, R, l, u, c
    char opt;
    // Argomento (o argomenti separati dalla virgola)
    char* arg;
    // Tempo in millisecondi che intercorre tra l'invio di due richieste successive al server (0 di default)
    char* time;
    // Cartella in memoria secondaria in cui vengono scritti i file (NULL di default) specificata con le opzioni -D e -d
    char* dirname;
    struct fsp_client_request* prev;
    struct fsp_client_request* next;
};

struct fsp_client_request_queue {
    struct fsp_client_request* head;
    struct fsp_client_request* tail;
};

/**
 * \brief Aggiunge req in fondo alla coda queue se queue != NULL && req != NULL.
 *        Non fa nulla altrimenti.
 */
void fsp_client_request_queue_enqueue(struct fsp_client_request_queue* queue, struct fsp_client_request* req);

/**
 * \brief Rimuove e restituisce l'elemento in testa alla coda queue se queue != NULL.
 *        Restituisce NULL se la coda è vuota o queue == NULL.
 */
struct fsp_client_request* fsp_client_request_queue_dequeue(struct fsp_client_request_queue* queue);

/**
 * \brief Alloca memoria e restituisce un puntatore alla nuova request con request.opt = opt e request.arg = arg,
 *        ponendo gli altri campi a NULL.
 *        Restituisce NULL se non c'è sufficiente memoria nello heap.
 *        Liberare dalla memoria il puntatore con fsp_client_request_queue_freeRequest quando non più necessario.
 */
struct fsp_client_request* fsp_client_request_queue_newRequest(char opt, char* arg);

/**
 * \brief Libera req dalla memoria se req != NULL.
 *        Non fa nulla altrimenti.
 */
void fsp_client_request_queue_freeRequest(struct fsp_client_request* req);

/**
 * \brief Rimuove dalla coda queue tutte le request presenti in essa e le libera dalla memoria se queue != NULL.
 *        Non fa nulla altrimenti.
 */
void fsp_client_request_queue_freeAllRequests(struct fsp_client_request_queue* queue);

#endif
