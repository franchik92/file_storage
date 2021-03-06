/*
 * Autore: Francesco Gallicchio
 * Matricola: 579131
 */

#ifndef FSP_PARSER_H
#define FSP_PARSER_H

#include <stdlib.h>

// Dimensione massima del buffer (128MB)
#define FSP_PARSER_BUF_MAX_SIZE 134217728

// Comandi dei messaggi di richiesta
enum fsp_command { APPEND, CLOSE, LOCK, OPEN, OPENC, OPENCL, OPENL, QUIT, READ, READN, REMOVE, UNLOCK, WRITE };

// Messaggio di richiesta
struct fsp_request {
    enum fsp_command cmd;
    char* arg;
    size_t data_len;
    void* data;
};

// Messaggio di risposta
struct fsp_response {
    int code;
    char* description;
    size_t data_len;
    void* data;
};

// Il campo dati contenuto in un messaggio fsp di risposta (lista di dati)
struct fsp_data {
    // Nome del dato
    char* pathname;
    // Dimensione (in byte) del dato
    size_t size;
    // Dato
    void* data;
    // Dato successivo
    struct fsp_data* next;
};

/**
 * \brief Legge i byte contenuti in buf di lunghezza size e ne fa il parsing per determinare
 *        i campi del messaggio di richiesta FSP che salva in *req.
 *
 * La funzione modifica il contenuto di buf se termina con successo. Dopo la chiamata della funzione
 * qualsiasi modifica a buf può comportare cambiamenti anche al contenuto di *req.
 * I valori in *req non sono significativi se la funzione non termina con successo.
 * \return 0 in caso di successo,
 *         -1 se buf == NULL || req == NULL,
 *         -2 se il messaggio è incompleto,
 *         -3 se il messaggio contiene errori sintattici.
 */
int fsp_parser_parseRequest(void* buf, size_t size, struct fsp_request* req);

/**
 * \brief Genera un messaggio di richiesta fsp e lo salva in *buf.
 *
 * Se i campi arg e data sono vuoti, è possibile passare NULL come argomenti.
 * *buf deve essere allocato nello heap. Se necessario rialloca la memoria di *buf nello heap
 * (*buf e *size vengono modificati di conseguenza). La massima dimensione del buffer è definita in FSP_PARSER_BUF_MAX_SIZE.
 * Se termina con successo *buf conterrà nel campo data del messaggio i primi data_len byte di data.
 * Il comportamento è indefinito se la lunghezza dei dati data_len è superiore a quella effettiva in data.
 * \return valore maggiore o uguale a 0 in caso di successo (tale valore indica il numero di byte scritti in *buf),
 *         -1 se buf == NULL || *buf == NULL || size == NULL || data_len < 0 || (data_len > 0 && data == NULL) || *size > FSP_PARSER_BUF_MAX_SIZE,
 *         -2 se non è stato possibile riallocare la memoria per *buf.
 */
long int fsp_parser_makeRequest(void** buf, size_t* size, enum fsp_command cmd, const char* arg, size_t data_len, void* data);

/**
 * \brief Legge i byte contenuti in buf di lunghezza size e ne fa il parsing per determinare
 *        i campi del messaggio di risposta FSP che salva in *resp.
 *
 * La funzione modifica il contenuto di buf se termina con successo. Dopo la chiamata della funzione
 * qualsiasi modifica a buf può comportare cambiamenti anche al contenuto di *resp.
 * I valori in *resp non sono significativi se la funzione non termina con successo.
 * \return 0 in caso di successo,
 *         -1 se buf == NULL || resp == NULL,
 *         -2 se il messaggio è incompleto,
 *         -3 se il messaggio contiene errori sintattici.
 */
int fsp_parser_parseResponse(void* buf, size_t size, struct fsp_response* resp);

/**
 * \brief Genera un messaggio di risposta fsp e lo salva in *buf.
 *
 * Se i campi description e data sono vuoti, è possibile passare NULL come argomenti.
 * *buf deve essere allocato nello heap. Se necessario rialloca la memoria di *buf nello heap
 * (*buf e *size vengono modificati di conseguenza). La massima dimensione del buffer è definita in FSP_PARSER_BUF_MAX_SIZE.
 * Se termina con successo *buf conterrà nel campo data del messaggio i primi data_len byte di data.
 * Il comportamento è indefinito se la lunghezza dei dati data_len è superiore a quella effettiva in data.
 * \return valore maggiore o uguale a 0 in caso di successo (tale valore indica il numero di byte scritti in *buf),
 *         -1 se buf == NULL || *buf == NULL || size == NULL || data_len < 0 || (data_len > 0 && data == NULL) || *size > FSP_PARSER_BUF_MAX_SIZE,
 *         -2 se non è stato possibile riallocare la memoria per *buf.
 */
long int fsp_parser_makeResponse(void** buf, size_t* size, int code, const char* description, size_t data_len, void* data);

/**
 * \brief Fa il parse dei dati data (campo DATA dei messaggi fsp) di lunghezza data_len e salva nella lista *parsed_data il loro contenuto.
 *
 * La funzione modifica il contenuto di data. Dopo la chiamata della funzione qualsiasi modifica a data può comportare cambiamenti
 * anche al contenuto dei dati nella lista *parsed_data. Usare il campo next in *parsed_data per passare da un dato al successivo.
 * La lista *parsed_data è vuota (*parsed_data == NULL) se la funzione non termina con successo.
 * Alloca memoria nello heap per costruire la lista *parsed_data: invocare il metodo fsp_parser_freeData() su *parsed_data per liberarla.
 * \return 0 in caso di successo,
 *         -1 se data_len <= 0 || data == NULL || parsed_data == NULL,
 *         -2 se il campo data del messaggio è incompleto,
 *         -3 se il campo data del messaggio contiene errori sintattici,
 *         -4 se è stato impossibile allocare memoria nello heap.
 */
int fsp_parser_parseData(size_t data_len, void* data, struct fsp_data** parsed_data);

/**
 * \brief Genera il campo data contenuto nei messaggi fsp e lo salva in *buf.
 *
 * *buf deve essere allocato nello heap. Se necessario rialloca la memoria di *buf nello heap
 * (*buf e *size vengono modificati di conseguenza). La massima dimensione del buffer è definita in FSP_PARSER_BUF_MAX_SIZE.
 * Aggiunge a *buf i campi pathname (PATHNAME), data_size (SIZE) e data (data). Se data == NULL, allora il campo relativo
 * al dato sarà vuoto (data_size deve essere uguale a zero).
 * Inizia a scrivere a partire da (*buf)[offset].
 * Il comportamento è indefinito se la dimensione del dato data_size è superiore a quella effettiva in data.
 * \return valore maggiore o uguale a 0 in caso di successo (tale valore indica il numero di byte scritti in *buf),
 *         -1 se buf == NULL || *buf == NULL || size == NULL || offset < 0 || pathname == NULL || data_size < 0 || (data_size > 0 && data == NULL) || *size > FSP_PARSER_BUF_MAX_SIZE,
 *         -2 se non è stato possibile riallocare la memoria per *buf.
 */
long int fsp_parser_makeData(void** buf, size_t* size, unsigned long int offset, const char* pathname, size_t data_size, void* data);

/**
 * \brief Libera dalla memoria i nodi della lista *parsed_data.
 */
void fsp_parser_freeData(struct fsp_data* parsed_data);

#endif
