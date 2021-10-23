/*
 * Autore: Francesco Gallicchio
 * Matricola: 579131
 */

#include <stdlib.h>
#include <string.h>

#include <fsp_opened_file.h>

struct fsp_opened_file* fsp_opened_file_new(const char* filename, int flags) {
    if(filename == NULL || strlen(filename) >= FSP_OPENED_FILE_NAME_MAX_LEN) return NULL;
    
    struct fsp_opened_file* opened_file = NULL;
    if((opened_file = malloc(sizeof(struct fsp_opened_file))) == NULL) return NULL;
    
    strcpy(opened_file->filename, filename);
    opened_file->flags = flags;
    
    return opened_file;
}

void fsp_opened_file_free(struct fsp_opened_file* opened_file) {
    if(opened_file == NULL) return;
    free(opened_file);
}
