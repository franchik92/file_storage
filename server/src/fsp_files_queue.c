/*
 * Autore: Francesco Gallicchio
 * Matricola: 579131
 */

#include <stdlib.h>
#include <string.h>

#include <fsp_files_queue.h>

struct fsp_files_queue* fsp_files_queue_new() {
    struct fsp_files_queue* queue = NULL;
    if((queue = malloc(sizeof(struct fsp_files_queue))) == NULL) return NULL;
    
    queue->head = NULL;
    queue->tail = NULL;
    
    return queue;
}

void fsp_files_queue_free(struct fsp_files_queue* queue) {
    if(queue == NULL) return;
    free(queue);
}

int fsp_files_queue_enqueue(struct fsp_files_queue* queue, struct fsp_file* file) {
    if(queue == NULL || file == NULL) return -1;
    
    if(queue->head == NULL) {
        queue->head = file;
        queue->tail = file;
    } else {
        (queue->tail)->queue_next = file;
        queue->tail = file;
    }
    file->queue_next = NULL;
    
    return 0;
}

struct fsp_file* fsp_files_queue_dequeue(struct fsp_files_queue* queue) {
    if(queue == NULL || queue->head == NULL) return NULL;
    
    struct fsp_file* _file = queue->head;
    if(queue->head == queue->tail) {
        queue->head = NULL;
        queue->tail = NULL;
    } else {
        queue->head = (queue->head)->queue_next;
    }
    _file->queue_next = NULL;
    
    return _file;
}

struct fsp_file* fsp_files_queue_remove(struct fsp_files_queue* queue, const char* pathname) {
    if(queue == NULL || pathname == NULL) return NULL;
    
    struct fsp_file* _file = queue->head;
    struct fsp_file* _file_prev = NULL;
    while(_file != NULL && strcmp(_file->pathname, pathname) != 0) {
        _file_prev = _file;
        _file = _file->queue_next;
    }
    if(_file == NULL) return NULL;
    
    if(queue->head == queue->tail) {
        queue->head = NULL;
        queue->tail = NULL;
    } else if(_file == queue->head) {
        queue->head = (queue->head)->queue_next;
    } else if(_file == queue->tail) {
        _file_prev->queue_next = NULL;
        queue->tail = _file_prev;
    } else {
        _file_prev->queue_next = _file->queue_next;
    }
    _file->queue_next = NULL;
    
    return _file;
}
