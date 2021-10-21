/*
 * Albero binario di ricerca contenente i file mantenuti aperti.
 */

#ifndef FSP_OPENED_FILES_BST_H
#define FSP_OPENED_FILES_BST_H

#define FSP_OPENED_FILES_BST_FILE_MAX_LEN 256

// Ogni nodo dell'albero binario di ricerca è definito dalla struttura opened_file
struct opened_file {
    // Nome del file
    char filename[FSP_OPENED_FILES_BST_FILE_MAX_LEN];
    // Valori dei flag: O_DEFAULT, O_CREATE, O_LOCK, O_CREATE | O_LOCK
    int flags;
    // Padre
    struct opened_file* parent;
    // Sottoalbero sinistro
    struct opened_file* left;
    // Sottoalbero destro
    struct opened_file* right;
};

/**
 * \brief Aggiunge all'albero di radice *root un nodo con i valori filename e flags.
 *
 * \return 0 in caso di successo,
 *         -1 se root == NULL || filename == NULL,
 *         -2 se filename è già presente nell'albero,
 *         -3 se la lunghezza di filename è maggiore o uguale a FSP_CLIENT_OPENED_FILES_ABR_FILE_MAX_LEN,
 *         -4 se non è stato possibile allocare memoria per il nuovo nodo.
 */
int fsp_opened_files_bst_insert(struct opened_file** root, const char* filename, int flags);

/**
 * \brief Rimuove dall'albero di radice *root il nodo con nome del file filename.
 *
 * \return 0 in caso di successo,
 *         -1 se root == NULL || filename == NULL,
 *         -2 altrimenti (nodo non presente nell'albero).
 */
int fsp_opened_files_bst_delete(struct opened_file** root, const char* filename);

/**
 * \brief Rimuove dall'albero di radice root tutti i nodi in esso presenti.
 *        Non fa nulla altrimenti.
 *        Prima di rimuovere un nodo passa il nome del file contenuto in esso come argomento alla funzione handler se handler != NULL.
 */
void fsp_opened_files_bst_deleteAll(struct opened_file* root, void (*handler) (const char* filename));

/**
 * \brief Cerca e restituisce il nodo dell'albero di radice root con nome del file filename.
 *
 * \return Il puntatore al nodo,
 *         NULL altrimenti (nodo non presente nell'albero o filename == NULL).
 */
struct opened_file* fsp_opened_files_bst_search(struct opened_file* root, const char* filename);

#endif
