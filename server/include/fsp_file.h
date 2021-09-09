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
    // loccked < 0 se la lock non è stata settata
    int locked;
    // Indica se il file deve essere rimosso quando links == 0
    // Non è possibile aprire il file se remove == 1
    int remove;
    
    // Campi usati per la gestione dell'albero binario di ricerca
    // Padre
    struct fsp_file* parent;
    // Sottoalbero sinistro
    struct fsp_file* left;
    // Sottoalbero destro
    struct fsp_file* right;
};

/**
 * \brief Alloca memoria per un nuovo file e lo restituisce.
 *        I campi della struttura fsp_file conterranno i rispettivi valori degli argomenti
 *        passati alla funzione.
 *
 * \return Il nuovo file,
 *         NULL se non è stato possibile allocare la memoria.
 */
struct fsp_file* fsp_file_new(const char* pathname, void* data, size_t size, int links, int locked, int remove);

/**
 * \brief Libera file dalla memoria.
 */
void fsp_file_free(struct fsp_file* file);

#endif
