// Albero binario di ricerca contenente i file ordinati lessicograficamente rispetto a pathname (chiave).

#ifndef FSP_FILES_BST_H
#define FSP_FILES_BST_H

#include <stdio.h>

#include <fsp_file.h>

/**
 * \brief Aggiunge file all'albero binario di ricerca con radice *root.
 *        Non fa niente se il pathname del file è già presente in uno dei file nell'albero.
 *
 * \return 0 in caso di successo,
 *         -1 se root == NULL || file == NULL || file->pathname == NULL,
 *         -2 se il file è già presente nell'albero.
 */
int fsp_files_bst_insert(struct fsp_file** root, struct fsp_file* file);

/**
 * \brief Cerca il file con nome pathname nell'albero binario di ricerca con radice root
 *        e lo restituisce.
 *
 * \return Il file con nome pathname,
 *         NULL se non ha trovato il file.
 */
struct fsp_file* fsp_files_bst_search(struct fsp_file* root, const char* pathname);

/**
 * \brief Rimuove dall'albero binario di ricerca con radice *root il file con nome pathname.
 *        Il file, se presente, viene liberato dalla memoria.
 *
 * \return 0 in caso di successo,
 *         -1 se root == NULL || pathname == NULL
 *         -2 se il file non è presente nell'albero.
 */
int fsp_files_bst_delete(struct fsp_file** root, const char* pathname);

/**
 * \brief Rimuove tutti i file dall'albero binario di ricerca con radice *root.
 */
void fsp_files_bst_deleteAll(struct fsp_file* root);

#endif
