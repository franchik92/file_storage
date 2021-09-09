// Lista di file.

#ifndef FSP_FILES_LIST_H
#define FSP_FILES_LIST_H

#include <stdio.h>
#include <fsp_file.h>

struct fsp_files_list {
    struct fsp_file* file;
    struct file_list* next;
};

/**
 * \brief Aggiunge file alla lista *list.
 *
 * \return 0 in caso di successo,
 *         -1 se list == NULL || file == NULL,
 *         -2 se non è stato possibile allocare la memoria.
 */
int fsp_files_list_add(struct fsp_files_list** list, struct fsp_file* file);

/**
 * \brief Controlla se la lista *list contiene o meno un file con nome pathname.
 *
 * \return 0 se list == NULL || pathname == NULL || il file non è presente,
 *         1 se il file è presente.
 */
int fsp_files_list_contains(struct fsp_files_list* list, const char* pathname);

/**
 * \brief Rimuove dalla lista *list il file con nome pathname.
 *
 * \return Il file,
 *         NULL se list == NULL || pathname == NULL || il file non è presente.
 */
struct fsp_file* fsp_files_list_remove(struct fsp_files_list** list, const char* pathname);

/**
 * \brief Rimuove tutti i file presenti nella lista *list.
 */
void fsp_files_list_removeAll(struct fsp_files_list** list);

#endif
