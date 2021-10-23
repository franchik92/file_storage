/*
 * Autore: Francesco Gallicchio
 * Matricola: 579131
 */

#ifndef FSP_OPENED_FILE_H
#define FSP_OPENED_FILE_H

#define FSP_OPENED_FILE_NAME_MAX_LEN 256

struct fsp_opened_file {
    // Nome del file
    char filename[FSP_OPENED_FILE_NAME_MAX_LEN];
    // Valori dei flag: O_DEFAULT, O_CREATE, O_LOCK, O_CREATE | O_LOCK
    int flags;
    // Nodo succcessivo
    struct fsp_opened_file* next;
};

/**
 * \brief Alloca memoria per un nuovo file aperto e lo restituisce.
 *        I campi della struttura fsp_opened_file conterranno filename e flags.
 *        Usare la funzione fsp_opened_file_free per liberare il file aperto dalla memoria.
 *
 * \return Il nuovo file aperto,
 *         NULL se filename == NULL || strlen(filename) >= FSP_OPENED_FILE_NAME_MAX_LEN || non è stato possibile allocare la memoria.
 */
struct fsp_opened_file* fsp_opened_file_new(const char* filename, int flags);

/**
 * \brief Libera opened_file dalla memoria.
 */
void fsp_opened_file_free(struct fsp_opened_file* opened_file);

#endif
