/*
 * Autore: Francesco Gallicchio
 * Matricola: 579131
 */

#include <stdlib.h>
#include <string.h>

#include <fsp_files_list.h>

int fsp_files_list_add(struct fsp_files_list** list, struct fsp_file* file) {
    if(list == NULL || file == NULL) return -1;
    
    struct fsp_files_list* _file_list = NULL;
    if((_file_list = malloc(sizeof(struct fsp_files_list))) == NULL) return -2;
    
    _file_list->file = file;
    
    if(*list == NULL) {
        *list = _file_list;
        _file_list->next = NULL;
    } else {
        struct fsp_files_list* _list = *list;
        struct fsp_files_list* _list_prev = NULL;
        int cmp = -1;
        while(_list != NULL && (cmp = strcmp((_list->file)->pathname, file->pathname)) < 0) {
            _list_prev = _list;
            _list = _list->next;
        }
        if(_list == NULL) {
            _list_prev->next = _file_list;
            _file_list->next = NULL;
        } else if(cmp == 0) {
            return -3;
        } else {
            _file_list->next = _list;
            if(_list_prev == NULL) {
                *list = _file_list;
            } else {
                _list_prev->next = _file_list;
            }
        }
    }
    
    return 0;
}

int fsp_files_list_contains(const struct fsp_files_list* list, const char* pathname) {
    if(list == NULL || pathname == NULL) return 0;
    
    int cmp = -1;
    while(list != NULL && (cmp = strcmp((list->file)->pathname, pathname)) < 0) {
        list = list->next;
    }
    if(list == NULL || cmp != 0) return 0;
    
    return 1;
}

struct fsp_file* fsp_files_list_remove(struct fsp_files_list** list, const char* pathname) {
    if(list == NULL || pathname == NULL) return NULL;
    
    struct fsp_files_list* _list = *list;
    struct fsp_files_list* _list_prev = NULL;
    int cmp = -1;
    while(_list != NULL && (cmp = strcmp((_list->file)->pathname, pathname)) < 0) {
        _list_prev = _list;
        _list = _list->next;
    }
    if(_list == NULL || cmp != 0) return NULL;
    
    struct fsp_file* _file = NULL;
    _file = _list->file;
    
    if(_list_prev == NULL) {
        *list = _list->next;
    } else {
        _list_prev->next = _list->next;
    }
    free(_list);
    
    return _file;
}

void fsp_files_list_removeAll(struct fsp_files_list** list) {
    if(list == NULL) return;
    
    struct fsp_files_list* _list = *list;
    struct fsp_files_list* _list_prev = NULL;
    while(_list != NULL) {
        _list_prev = _list;
        _list = _list->next;
        free(_list_prev);
    }
    *list = NULL;
}
