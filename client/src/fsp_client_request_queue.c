#include <fsp_client_request_queue.h>

#include <stdio.h>
#include <stdlib.h>

void fsp_client_request_queue_enqueue(struct fsp_client_request_queue* queue, struct fsp_client_request* req) {
    if(queue != NULL && req != NULL) {
        if(queue->head == NULL) {
            req->prev = NULL;
            req->next = NULL;
            queue->head = req;
            queue->tail = req;
        } else {
            req->prev = queue->tail;
            req->next = NULL;
            queue->tail->next = req;
            queue->tail = req;
        }
    }
}

struct fsp_client_request* fsp_client_request_queue_dequeue(struct fsp_client_request_queue* queue) {
    if(queue != NULL && queue->head != NULL) {
        struct fsp_client_request* req = queue->head;
        if(queue->head != queue->tail) {
            queue->head = req->next;
        } else {
            queue->head = NULL;
            queue->tail = NULL;
        }
        return req;
    }
    return NULL;
}

struct fsp_client_request* fsp_client_request_queue_newRequest(char opt, char* arg) {
    struct fsp_client_request* req = malloc(sizeof(struct fsp_client_request));
    if(req != NULL) {
        req->opt = opt;
        req->arg = arg;
        req->time = NULL;
        req->dirname = NULL;
        req->prev = NULL;
        req->next = NULL;
    }
    return req;
}

void fsp_client_request_queue_freeRequest(struct fsp_client_request* req) {
    if(req != NULL) free(req);
}

void fsp_client_request_queue_freeAllRequests(struct fsp_client_request_queue* queue) {
    if(queue != NULL) {
        struct fsp_client_request* req;
        while((req = fsp_client_request_queue_dequeue(queue)) != NULL) {
            fsp_client_request_queue_freeRequest(req);
        }
    }
}
