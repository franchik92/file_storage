#include <stdlib.h>

#include <fsp_sfd_queue.h>

struct fsp_sfd_queue* fsp_sfd_queue_new(size_t len) {
    struct fsp_sfd_queue* queue;
    if((queue = malloc(sizeof(struct fsp_sfd_queue))) == NULL) return NULL;
    if((queue->arr = malloc(sizeof(int)*len)) == NULL) {
        free(queue);
        return NULL;
    }
    queue->len = len;
    queue->head = 0;
    queue->tail = 0;
    return queue;
}

void fsp_sfd_queue_free(struct fsp_sfd_queue* queue) {
    if(queue == NULL) return;
    free(queue->arr);
    free(queue);
}

int fsp_sfd_queue_enqueue(struct fsp_sfd_queue* queue, int sfd) {
    if(queue->head == queue->tail + 1) {
        // Overflow
        return -1;
    }
    (queue->arr)[queue->tail] = sfd;
    queue->tail = (queue->tail + 1)%queue->len;
    return 0;
}

int fsp_sfd_queue_dequeue(struct fsp_sfd_queue* queue) {
    if(queue->head == queue->tail) {
        // Underflow
        return -1;
    }
    int sfd = (queue->arr)[queue->head];
    queue->head = (queue->head + 1)%queue->len;
    return sfd;
}

int fsp_sfd_queue_isEmpty(struct fsp_sfd_queue* queue) {
    return queue->head == queue->tail;
}
