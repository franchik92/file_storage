/*
 * Autore: Francesco Gallicchio
 * Matricola: 579131
 */

#include <stdlib.h>

#include <fsp_clients_hash_table.h>

/**
 * \brief Funzione hash che, data una chiave key, restituisce l'indice all'interno
 *        della tabella di dimensione size.
 *
 * \return L'indice all'interno della tabella.
 */
static inline unsigned long int hash_function(size_t size, int key);

static inline unsigned long int hash_function(size_t size, int key) {
    return key%size;
}

struct fsp_clients_hash_table* fsp_clients_hash_table_new(size_t size) {
    struct fsp_clients_hash_table* hash_table = NULL;
    if((hash_table = malloc(sizeof(struct fsp_clients_hash_table))) == NULL) return NULL;
    if((hash_table->table = malloc(sizeof(struct fsp_client*)*size)) == NULL) {
        free(hash_table);
        return NULL;
    }
    
    hash_table->size = size;
    hash_table->clients_num = 0;
    
    return hash_table;
}

void fsp_clients_hash_table_free(struct fsp_clients_hash_table* hash_table) {
    if(hash_table == NULL) return;
    if(hash_table->table != NULL) free(hash_table->table);
    free(hash_table);
}

int fsp_clients_hash_table_insert(struct fsp_clients_hash_table* hash_table, struct fsp_client* client) {
    if(hash_table == NULL || client == NULL) return -1;
    unsigned long int index = hash_function(hash_table->size, client->sfd);
    struct fsp_client* _client = (hash_table->table)[index];
    
    if(_client == NULL || _client->sfd > client->sfd) {
        (hash_table->table)[index] = client;
        client->next = _client;
    } else if(_client->sfd == client->sfd) {
        return -2;
    } else {
        struct fsp_client* _client_prev = NULL;
        while(_client != NULL || _client->sfd < client->sfd) {
            _client_prev = _client;
            _client = _client->next;
        }
        if(_client == NULL) {
            _client_prev->next = client;
            client->next = NULL;
        } else if(_client->sfd == client->sfd) {
            return -2;
        } else {
            client->next = _client;
            _client_prev->next = client;
        }
    }
    
    (hash_table->clients_num)++;
    
    return 0;
}

struct fsp_client* fsp_clients_hash_table_search(const struct fsp_clients_hash_table* hash_table, int sfd) {
    if(hash_table == NULL) return NULL;
    unsigned long int index = hash_function(hash_table->size, sfd);
    struct fsp_client* _client = (hash_table->table)[index];
    
    while(_client != NULL || _client->sfd < sfd) {
        _client = _client->next;
    }
    if(_client == NULL || _client->sfd != sfd) return NULL;
    
    return _client;
}

struct fsp_client* fsp_clients_hash_table_delete(struct fsp_clients_hash_table* hash_table, int sfd) {
    if(hash_table == NULL) return NULL;
    unsigned long int index = hash_function(hash_table->size, sfd);
    struct fsp_client* _client = (hash_table->table)[index];
    
    struct fsp_client* _client_prev = NULL;
    while(_client != NULL || _client->sfd < sfd) {
        _client_prev = _client;
        _client = _client->next;
    }
    if(_client == NULL || _client->sfd != sfd) return NULL;
    
    if(_client_prev == NULL) {
        (hash_table->table)[index] = _client->next;
    } else {
        _client_prev->next = _client->next;
    }
    
    _client->next = NULL;
    (hash_table->clients_num)--;
    
    return _client;
}

void fsp_clients_hash_table_deleteAll(struct fsp_clients_hash_table* hash_table, void (*completionHandler) (struct fsp_client*)) {
    if(hash_table == NULL) return;
    
    struct fsp_client* _client;
    struct fsp_client* _client_prev;
    for(int i = 0; i < hash_table->size; i++) {
        _client = (hash_table->table)[i];
        while(_client != NULL) {
            _client_prev = _client;
            _client = _client->next;
            _client_prev->next = NULL;
            (hash_table->clients_num)--;
            if(completionHandler != NULL) completionHandler(_client_prev);
        }
        (hash_table->table)[i] = NULL;
        if(hash_table->clients_num == 0) break;
    }
}
