#include <stdlib.h>

#include <fsp_client.h>

struct fsp_client* fsp_client_new(int sfd, size_t buf_size) {
    struct fsp_client* client = NULL;
    if((client = malloc(sizeof(struct fsp_client))) == NULL) return NULL;
    if((client->buf = malloc(buf_size)) == NULL) {
        free(client);
        return NULL;
    }
    
    client->sfd = sfd;
    client->size = buf_size;
    client->openedFiles = NULL;
    client->next = NULL;
    
    return client;
}

void fsp_client_free(struct fsp_client* client) {
    if(client == NULL) return;
    if(client->buf != NULL) free(client->buf);
    free(client);
}
