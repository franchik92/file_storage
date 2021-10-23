/*
 * Autore: Francesco Gallicchio
 * Matricola: 579131
 */

#include <stdlib.h>
#include <string.h>

#include <fsp_file.h>

struct fsp_file* fsp_file_new(const char* pathname, const void* data, size_t size, int links, int locked, int remove) {
    struct fsp_file* file = NULL;
    if((file = malloc(sizeof(struct fsp_file))) == NULL) return NULL;
    if(pathname != NULL && (file->pathname = calloc(strlen(pathname)+1, sizeof(char))) == NULL) {
        free(file);
        return NULL;
    }
    if(data != NULL && (file->data = malloc(size)) == NULL) {
        free(file->pathname);
        free(file);
    }
    
    pathname == NULL ? file->pathname = NULL : strcpy(file->pathname, pathname);
    file->data == NULL ? file->data = NULL : memcpy(file->data, data, size);
    file->size = size;
    file->links = links;
    file->remove = remove;
    file->hash_table_next = NULL;
    file->queue_next = NULL;
    
    return file;
}

void fsp_file_free(struct fsp_file* file) {
    if(file == NULL) return;
    if(file->pathname != NULL) free(file->pathname);
    if(file->data != NULL) free(file->data);
    free(file);
}
