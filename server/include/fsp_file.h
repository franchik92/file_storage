#ifndef FSP_FILE_H
#define FSP_FILE_H

struct fsp_file {
    // Nome del file
    char* pathname;
    // File
    // Se data == NULL, allora il file non è ancora stato creato
    // e verrà rimosso se links == 0
    void* data;
    // Dimensione del file
    size_t size;
    // Numero degli utenti che hanno aperto il file
    unsigned int links;
    // sfd del client che ha la lock sul file
    // locked < 0 se la lock non è stata settata
    int locked;
    // Indica se il file deve essere rimosso quando links == 0
    // Se remove == 1 non sarà possibile eseguire operazioni su di esso tranne la chiusura
    unsigned short int remove;
    
    // Nodo successivo (usato per la gestione della tabella hash)
    struct fsp_file* hash_table_next;
    
    // Nodo successivo (usato per la gestione della coda FIFO)
    struct fsp_file* queue_next;
};

/**
 * \brief Alloca memoria per un nuovo file e lo restituisce.
 *        I campi della struttura fsp_file conterranno i rispettivi valori degli argomenti
 *        passati alla funzione. Usare la funzione fsp_file_free per liberare il file dalla memoria.
 *
 * \return Il nuovo file,
 *         NULL se non è stato possibile allocare la memoria.
 */
struct fsp_file* fsp_file_new(const char* pathname, const void* data, size_t size, int links, int locked, int remove);

/**
 * \brief Libera file dalla memoria.
 */
void fsp_file_free(struct fsp_file* file);

#endif
