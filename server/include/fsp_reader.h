/*
 * Autore: Francesco Gallicchio
 * Matricola: 579131
 */

#ifndef FSP_READER_H
#define FSP_READER_H

#include <fsp_parser.h>

// Dimensione massima del buffer (256MB)
#define FSP_READER_BUF_MAX_SIZE 268435456

/**
 * \brief Legge i byte da sfd che compongono un messaggio di richiesta fsp e salva i campi del
 *        messaggio in req.
 *
 * Il comportamento della funzione è indefinito se il messaggio da leggere non è un messaggio di
 * richiesta fsp. Fa uso di un buffer *buf di lunghezza *size per la lettura dei byte da sfd che in
 * caso di necessità può riallocare (il puntatore a *buf e il valore di *size vengono aggiornati di
 * conseguenza). La massima dimensione del buffer è definita in FSP_READER_BUF_MAX_SIZE.
 * Se la funzione termina con successo, successive modifiche a *buf rendono i campi in
 * req non significativi.
 * \return 0 in caso di successo,
 *         -1 se buf == NULL || *buf == NULL || size == NULL || req == NULL ||
 *               *size > FSP_READER_BUF_MAX_SIZE,
 *         -2 in caso di errori durante la lettura (read() setta errno appropriatamente),
 *         -3 se sfd ha raggiunto EOF senza aver letto un messaggio di richiesta,
 *         -4 se il messaggio contiene errori sintattici,
 *         -5 se è stato impossibile riallocare il buffer (memoria insufficiente).
 */
int fsp_reader_readRequest(int sfd, void** buf, size_t* size, struct fsp_request* req);

/**
 * \brief Legge i byte da sfd che compongono un messaggio di risposta fsp e salva i campi del
 *        messaggio in resp.
 *
 * Il comportamento della funzione è indefinito se il messaggio da leggere non è un messaggio di
 * risposta fsp. Fa uso di un buffer *buf di lunghezza *size per la lettura dei byte da sfd che in
 * caso di necessità può riallocare (il puntatore a *buf e il valore di *size vengono aggiornati di
 * conseguenza). La massima dimensione del buffer è definita in FSP_READER_BUF_MAX_SIZE.
 * Se la funzione termina con successo, successive modifiche a *buf rendono i campi in
 * req non significativi.
 * \return 0 in caso di successo,
 *         -1 se buf == NULL || *buf == NULL || size == NULL || resp == NULL ||
 *               *size > FSP_READER_BUF_MAX_SIZE,
 *         -2 in caso di errori durante la lettura (read() setta errno appropriatamente),
 *         -3 se sfd ha raggiunto EOF senza aver letto un messaggio di risposta,
 *         -4 se il messaggio contiene errori sintattici,
 *         -5 se è stato impossibile riallocare il buffer (memoria insufficiente).
 */
int fsp_reader_readResponse(int sfd, void** buf, size_t* size, struct fsp_response* resp);

#endif
