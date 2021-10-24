/*
 * Autore: Francesco Gallicchio
 * Matricola: 579131
 */

#include <stdlib.h>
#include <string.h>

#include <fsp_opened_files_hash_table.h>

/**
 * \brief Funzione hash che, data una chiave key, restituisce l'indice all'interno
 *        della tabella di dimensione size.
 *
 * \return L'indice all'interno della tabella.
 */
static inline unsigned long int hash_function(size_t size, const char* key);

static inline unsigned long int hash_function(size_t size, const char* key) {
    if(key == NULL || *key == '\0') return 0;
    
    unsigned long int index = *key;
    unsigned long int pow = size;
    key++;
    while(*key != '\0') {
        index += (*key)*pow;
        pow *= size;
        key++;
    }
    return index%size;
}

struct fsp_opened_files_hash_table* fsp_opened_files_hash_table_new(size_t size) {
    struct fsp_opened_files_hash_table* hash_table = NULL;
    if((hash_table = malloc(sizeof(struct fsp_opened_files_hash_table))) == NULL) return NULL;
    if((hash_table->table = malloc(sizeof(struct fsp_opened_file*)*size)) == NULL) {
        free(hash_table);
        return NULL;
    }
    
    for(int i = 0; i < size; i++) {
        (hash_table->table)[i] = NULL;
    }
    hash_table->size = size;
    hash_table->files_num = 0;
    
    return hash_table;
}

void fsp_opened_files_hash_table_free(struct fsp_opened_files_hash_table* hash_table) {
    if(hash_table == NULL) return;
    if(hash_table->table != NULL) free(hash_table->table);
    free(hash_table);
}

int fsp_opened_files_hash_table_insert(struct fsp_opened_files_hash_table* hash_table, const char* filename, int flags) {
    if(hash_table == NULL || filename == NULL) return -1;
    unsigned long int index = hash_function(hash_table->size, filename);
    struct fsp_opened_file* _opened_file = (hash_table->table)[index];
    
    struct fsp_opened_file* opened_file = NULL;
    if((opened_file = fsp_opened_file_new(filename, flags)) == NULL) {
        return -2;
    }
    
    int cmp;
    if(_opened_file == NULL || (cmp = strcmp(_opened_file->filename, opened_file->filename)) > 0) {
        (hash_table->table)[index] = opened_file;
        opened_file->next = _opened_file;
    } else if(cmp == 0) {
        fsp_opened_file_free(opened_file);
        return -3;
    } else {
        struct fsp_opened_file* _opened_file_prev = NULL;
        while(_opened_file != NULL && (cmp = strcmp(_opened_file->filename, opened_file->filename)) < 0) {
            _opened_file_prev = _opened_file;
            _opened_file = _opened_file->next;
        }
        if(_opened_file == NULL) {
            _opened_file_prev->next = opened_file;
            opened_file->next = NULL;
        } else if(cmp == 0) {
            fsp_opened_file_free(opened_file);
            return -3;
        } else {
            opened_file->next = _opened_file;
            _opened_file_prev->next = opened_file;
        }
    }
    
    (hash_table->files_num)++;
    
    return 0;
}

struct fsp_opened_file* fsp_opened_files_hash_table_search(const struct fsp_opened_files_hash_table* hash_table, const char* filename) {
    if(hash_table == NULL || filename == NULL) return NULL;
    unsigned long int index = hash_function(hash_table->size, filename);
    struct fsp_opened_file* _opened_file = (hash_table->table)[index];
    
    int cmp = -1;
    while(_opened_file != NULL && (cmp = strcmp(_opened_file->filename, filename)) < 0) {
        _opened_file = _opened_file->next;
    }
    if(_opened_file == NULL || cmp != 0) return NULL;
    
    return _opened_file;
}

void fsp_opened_files_hash_table_delete(struct fsp_opened_files_hash_table* hash_table, const char* filename) {
    if(hash_table == NULL || filename == NULL) return;
    unsigned long int index = hash_function(hash_table->size, filename);
    struct fsp_opened_file* _opened_file = (hash_table->table)[index];
    
    int cmp = -1;
    struct fsp_opened_file* _opened_file_prev = NULL;
    while(_opened_file != NULL && (cmp = strcmp(_opened_file->filename, filename)) < 0) {
        _opened_file_prev = _opened_file;
        _opened_file = _opened_file->next;
    }
    if(_opened_file == NULL || cmp != 0) return;
    
    if(_opened_file_prev == NULL) {
        (hash_table->table)[index] = _opened_file->next;
    } else {
        _opened_file_prev->next = _opened_file->next;
    }
    
    fsp_opened_file_free(_opened_file);
    (hash_table->files_num)--;
}

void fsp_opened_files_hash_table_deleteAll(struct fsp_opened_files_hash_table* hash_table, void (*completionHandler) (const char* filename)) {
    if(hash_table == NULL) return;
    
    struct fsp_opened_file* _opened_file;
    struct fsp_opened_file* _opened_file_prev;
    for(int i = 0; i < hash_table->size; i++) {
        _opened_file = (hash_table->table)[i];
        while(_opened_file != NULL) {
            _opened_file_prev = _opened_file;
            _opened_file = _opened_file->next;
            _opened_file_prev->next = NULL;
            (hash_table->files_num)--;
            if(completionHandler != NULL) completionHandler(_opened_file_prev->filename);
            fsp_opened_file_free(_opened_file_prev);
        }
        (hash_table->table)[i] = NULL;
        if(hash_table->files_num == 0) break;
    }
}
