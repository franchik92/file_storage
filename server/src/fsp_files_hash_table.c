/*
 * Autore: Francesco Gallicchio
 * Matricola: 579131
 */

#include <stdlib.h>
#include <string.h>

#include <fsp_files_hash_table.h>

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

struct fsp_files_hash_table* fsp_files_hash_table_new(size_t size) {
    struct fsp_files_hash_table* hash_table = NULL;
    if((hash_table = malloc(sizeof(struct fsp_files_hash_table))) == NULL) return NULL;
    if((hash_table->table = malloc(sizeof(struct fsp_file*)*size)) == NULL) {
        free(hash_table);
        return NULL;
    }
    
    hash_table->size = size;
    hash_table->files_num = 0;
    
    return hash_table;
}

void fsp_files_hash_table_free(struct fsp_files_hash_table* hash_table) {
    if(hash_table == NULL) return;
    if(hash_table->table != NULL) free(hash_table->table);
    free(hash_table);
}

int fsp_files_hash_table_insert(struct fsp_files_hash_table* hash_table, struct fsp_file* file) {
    if(hash_table == NULL || file == NULL) return -1;
    unsigned long int index = hash_function(hash_table->size, file->pathname);
    struct fsp_file* _file = (hash_table->table)[index];
    
    int cmp;
    if(_file == NULL || (cmp = strcmp(_file->pathname, file->pathname)) > 0) {
        (hash_table->table)[index] = file;
        file->hash_table_next = _file;
    } else if(cmp == 0) {
        return -2;
    } else {
        struct fsp_file* _file_prev = NULL;
        while(_file != NULL || (cmp = strcmp(_file->pathname, file->pathname)) < 0) {
            _file_prev = _file;
            _file = _file->hash_table_next;
        }
        if(_file == NULL) {
            _file_prev->hash_table_next = file;
            file->hash_table_next = NULL;
        } else if(cmp == 0) {
            return -2;
        } else {
            file->hash_table_next = _file;
            _file_prev->hash_table_next = file;
        }
    }
    
    (hash_table->files_num)++;
    
    return 0;
}

struct fsp_file* fsp_files_hash_table_search(const struct fsp_files_hash_table* hash_table, const char* pathname) {
    if(hash_table == NULL || pathname == NULL) return NULL;
    unsigned long int index = hash_function(hash_table->size, pathname);
    struct fsp_file* _file = (hash_table->table)[index];
    
    int cmp;
    while(_file != NULL || (cmp = strcmp(_file->pathname, pathname)) < 0) {
        _file = _file->hash_table_next;
    }
    if(_file == NULL || cmp != 0) return NULL;
    
    return _file;
}

struct fsp_file* fsp_files_hash_table_delete(struct fsp_files_hash_table* hash_table, const char* pathname) {
    if(hash_table == NULL || pathname == NULL) return NULL;
    unsigned long int index = hash_function(hash_table->size, pathname);
    struct fsp_file* _file = (hash_table->table)[index];
    
    int cmp;
    struct fsp_file* _file_prev = NULL;
    while(_file != NULL || (cmp = strcmp(_file->pathname, pathname)) < 0) {
        _file_prev = _file;
        _file = _file->hash_table_next;
    }
    if(_file == NULL || cmp != 0) return NULL;
    
    if(_file_prev == NULL) {
        (hash_table->table)[index] = _file->hash_table_next;
    } else {
        _file_prev->hash_table_next = _file->hash_table_next;
    }
    
    _file->hash_table_next = NULL;
    (hash_table->files_num)--;
    
    return _file;
}

void fsp_files_hash_table_deleteAll(struct fsp_files_hash_table* hash_table, void (*completionHandler) (struct fsp_file*)) {
    if(hash_table == NULL) return;
    
    struct fsp_file* _file;
    struct fsp_file* _file_prev;
    for(int i = 0; i < hash_table->size; i++) {
        _file = (hash_table->table)[i];
        while(_file != NULL) {
            _file_prev = _file;
            _file = _file->hash_table_next;
            _file_prev->hash_table_next = NULL;
            (hash_table->files_num)--;
            if(completionHandler != NULL) completionHandler(_file_prev);
        }
        (hash_table->table)[i] = NULL;
        if(hash_table->files_num == 0) break;
    }
}

struct fsp_files_hash_table_iterator* fsp_files_hash_table_getIterator(const struct fsp_files_hash_table* hash_table) {
    if(hash_table == NULL) return NULL;
    
    struct fsp_files_hash_table_iterator* iterator = NULL;
    if((iterator = malloc(sizeof(struct fsp_files_hash_table_iterator))) == NULL) return NULL;
    
    iterator->index = -1;
    iterator->next = NULL;
    iterator->table_size = hash_table->size;
    iterator->table = hash_table->table;
    
    for(int i = 0; i < iterator->table_size; i++) {
        if((iterator->table)[i] != NULL) {
            iterator->next = (iterator->table)[i];
            iterator->index = i;
            break;
        }
    }
    
    return iterator;
}

struct fsp_file* fsp_files_hash_table_getNext(struct fsp_files_hash_table_iterator* iterator) {
    if(iterator == NULL || iterator->next == NULL) return NULL;
    
    struct fsp_file* _file = iterator->next;
    
    if(_file->hash_table_next != NULL) {
        iterator->next = _file->hash_table_next;
    } else {
        iterator->next = NULL;
        for(int i = iterator->index + 1; i < iterator->table_size; i++) {
            if((iterator->table)[i] != NULL) {
                iterator->next = (iterator->table)[i];
                iterator->index = i;
                break;
            }
        }
        if(iterator->next == NULL) iterator->index = -1;
    }
    
    return _file;
}
