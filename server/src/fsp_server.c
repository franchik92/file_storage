#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/time.h>
#include <pthread.h>
#include <string.h>
#include <errno.h>

#include <fsp_files_bst.h>
#include <fsp_files_list.h>
#include <fsp_clients_hash_table.h>
#include <fsp_sfd_queue.h>
#include <fsp_files_queue.h>
#include <fsp_parser.h>
#include <fsp_reader.h>
#include <utils.h>

#ifndef UNIX_PATH_MAX
#define UNIX_PATH_MAX 104
#endif

#define CONFIG_FILE "./config.txt"
#define FSP_CLIENT_DEF_BUF_SIZE 4194304

typedef struct fsp_file* FILES;
typedef struct clients_hash_table* CLIENTS;
typedef struct fsp_sfd_queue* SFD_QUEUE;
typedef struct fsp_files_queue* PATHNAMES_QUEUE;
typedef struct fsp_files_list* OPENED_FILES;
typedef struct client_info* CLIENT_INFO;

static FILES files = NULL;
static CLIENTS clients = NULL;
static SFD_QUEUE sfd_queue = NULL;
static PATHNAMES_QUEUE pathnames_queue = NULL;

static pthread_mutex_t files_mutex;
static pthread_mutex_t clients_mutex;
static pthread_mutex_t sfd_queue_mutex;
static pthread_mutex_t pathnames_queue_mutex;

static pthread_cond_t sfd_queue_isNotEmpty;
static pthread_cond_t lock_cmd_isNotLocked;

// Valori letti dal file di configurazione
// Numero massimo di file che il server può mantenere in memoria
static unsigned int files_max_num;
// Capacità massima di memoria del server in byte
static unsigned long int storage_max_size;

// Numero attuale dei file presenti sul server
static unsigned int files_num;
// Dimensione della memoria occupata dai file
static unsigned long int storage_size;

/**
 * \brief Fa il parse del file di configurazione CONFIG_FILE e salva i valori contenuti in esso negli argomenti della funzione.
 *
 * \return 0 in caso di successo,
 *         -1 se non è stato possibile aprire il file,
 *         -2 se il file contiene errori sintattici.
 */
static int parseConfigFile(char* socket_file_name, char* log_file_name, unsigned int* _files_max_num, unsigned long int* _storage_max_size, unsigned int* max_conn, unsigned int* worker_threads_num);

/**
 * \brief Funzione eseguita dai thread worker.
 */
static void* worker(void* arg);

/**
 * \brief Legge da sfd una request fsp e la salva in req.
 *
 * \return 0 in caso di successo,
 *         -1 se client_info == NULL || req == NULL || client_info->buf == NULL ||
 *               client_info->size == NULL || client_info->size > FSP_READER_BUF_MAX_SIZE,
 *         -2 in caso di errori durante la lettura (read() setta errno appropriatamente),
 *         -3 se sfd ha raggiunto EOF senza aver letto un messaggio di richiesta,
 *         -4 se il messaggio contiene errori sintattici,
 *         -5 se è stato impossibile riallocare il buffer (memoria insufficiente).
 */
static int receiveFspReq(CLIENT_INFO client_info, struct fsp_request* req);

/**
 * \brief Scrive su sfd un messaggio di risposta fsp con i campi code, description, data_len e data.
 *
 * \return 0 in caso di successo,
 *         -1 altrimenti.
 */
static int sendFspResp(CLIENT_INFO client_info, int code, const char* description, size_t data_len, void* data);

/* Funzioni che eseguono i comandi richiesti dai client.
 * Prendono in input le informazioni del client (client_info) e il valore dell'argomento del relativo comando fsp (pathname o n).
 * Salvano in *code il codice di risposta e in description (di lunghezza massima descr_max_len) la relativa descrizione.
 * Alcune funzione salvano anche il campo *data (usare free per liberare *data dalla memoria) di lunghezza *data_len.
 * Restituiscono 0 in caso di successo e -1 altrimenti.
 * Se restituiscono -1, allora i valori salvati negli argomenti non sono significativi.
 */
int append_cmd(CLIENT_INFO client_info, char* pathname, struct fsp_data* parsed_data, int* code, char* description, const size_t descr_max_len, size_t* data_len, void** data);

int close_cmd(CLIENT_INFO client_info, char* pathname, int* code, char* description, const size_t descr_max_len);

int lock_cmd(CLIENT_INFO client_info, char* pathname, int* code, char* description, const size_t descr_max_len);

int open_cmd(CLIENT_INFO client_info, char* pathname, int* code, char* description, const size_t descr_max_len);

int openc_cmd(CLIENT_INFO client_info, char* pathname, int* code, char* description, const size_t descr_max_len);

int opencl_cmd(CLIENT_INFO client_info, char* pathname, int* code, char* description, const size_t descr_max_len);

int openl_cmd(CLIENT_INFO client_info, char* pathname, int* code, char* description, const size_t descr_max_len);

int read_cmd(CLIENT_INFO client_info, char* pathname, int* code, char* description, const size_t descr_max_len, size_t* data_len, void** data);

int readn_cmd(CLIENT_INFO client_info, long int n, int* code, char* description, const size_t descr_max_len, size_t* data_len, void** data);

int readn_rec(struct fsp_file* root_file, long int* n, long int* wrote_bytes_tot, size_t* data_len, void** data);

int remove_cmd(CLIENT_INFO client_info, char* pathname, int* code, char* description, const size_t descr_max_len);

int unlock_cmd(CLIENT_INFO client_info, char* pathname, int* code, char* description, const size_t descr_max_len);

int write_cmd(CLIENT_INFO client_info, char* pathname, struct fsp_data* parsed_data, int* code, char* description, const size_t descr_max_len, size_t* data_len, void** data);

int main(int argc, const char* argv[]) {
    // Valori letti dal file di configurazione
    // Nome del file socket
    char socket_file_name[UNIX_PATH_MAX];
    // Nome del file di log
    char log_file_name[UNIX_PATH_MAX];
    // Numero massimo di connessioni
    unsigned int max_conn;
    // Numero dei thread worker
    unsigned int worker_threads_num;
    
    // Fa il parse del file di configurazione
    switch(parseConfigFile(socket_file_name, log_file_name, &files_max_num, &storage_max_size, &max_conn, &worker_threads_num)) {
        case -1:
            // Impossibile aprire il file di configurazione
            fprintf(stderr, "Errore: impossibile aprire il file di configurazione %s.\n", CONFIG_FILE);
            return -1;
        case -2:
            // Errore di sintassi
            fprintf(stderr, "Errore: il file di configurazione %s contiene errori sintattici.\n", CONFIG_FILE);
            return -1;
        default:
            break;
    }
    
    // Inizializza le strutture dati
    if((clients = fsp_clients_hash_table_new(128)) == NULL) {
        fprintf(stderr, "Errore: memoria insufficiente.\n");
        return -1;
    }
    if((sfd_queue = fsp_sfd_queue_new(max_conn)) == NULL) {
        fprintf(stderr, "Errore: memoria insufficiente.\n");
        fsp_clients_hash_table_free(clients);
        return -1;
    }
    if((pathnames_queue = fsp_files_queue_new()) == NULL) {
        fprintf(stderr, "Errore: memoria insufficiente.\n");
        fsp_clients_hash_table_free(clients);
        fsp_sfd_queue_free(sfd_queue);
        return -1;
    }
    
    // Mutex
    if(pthread_mutex_init(&files_mutex, NULL) != 0) {
        // Errore
    }
    if(pthread_mutex_init(&clients_mutex, NULL) != 0) {
        // Errore
    }
    if(pthread_mutex_init(&sfd_queue_mutex, NULL) != 0) {
        // Errore
    }
    if(pthread_mutex_init(&pathnames_queue_mutex, NULL) != 0) {
        // Errore
    }
    
    // Condition variables
    if(pthread_cond_init(&sfd_queue_isNotEmpty, NULL) != 0) {
        // Errore
    }
    if(pthread_cond_init(&lock_cmd_isNotLocked, NULL) != 0) {
        // Errore
    }
    
    // Pipe senza nome per la comunicazione tra i thread worker e il thread master
    int pfd[2];
    if(pipe(pfd) != 0) {
        // Errore
    }
    
    // socket
    int sfd;
    if((sfd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
        perror(NULL);
        return -1;
    }
    // bind
    struct sockaddr_un sockaddr;
    sockaddr.sun_family = AF_UNIX;
    strncpy(sockaddr.sun_path, socket_file_name, UNIX_PATH_MAX);
    if(bind(sfd, (struct sockaddr*) &sockaddr, sizeof(sockaddr)) == -1) {
        perror(NULL);
        return -1;
    }
    // listen
    if(listen(sfd, SOMAXCONN) == -1) {
        perror(NULL);
        return -1;
    }
    // select
    fd_set set, rdset;
    FD_ZERO(&set);
    FD_SET(sfd, &set);
    FD_SET(pfd[0], &set);
    int fd_max = 0;
    if(pfd[0] > fd_max) fd_max = pfd[0];
    if(sfd > fd_max) fd_max = sfd;
    while(1) {
        rdset = set;
       if(select(fd_max+1, &rdset, NULL, NULL, NULL) == -1) {
            perror(NULL);
            return -1;
        } else {
            for(int fd = 0; fd < fd_max; fd++) {
                if(FD_ISSET(fd, &rdset)) {
                    if(fd == sfd) {
                        // Accetta una nuova connessione
                        int fd_c;
                        if((fd_c = accept(sfd, NULL, 0)) != -1) {
                            perror(NULL);
                            continue;
                        }
                        
                        pthread_mutex_lock(&sfd_queue_mutex);
                        fsp_clients_hash_table_insert(clients, fd_c, FSP_CLIENT_DEF_BUF_SIZE);
                        CLIENT_INFO client_info = fsp_clients_hash_table_search(clients, fd_c);
                        pthread_mutex_unlock(&sfd_queue_mutex);
                        
                        // Invia il messaggio di risposta fsp con codice 220
                        sendFspResp(client_info, 220, "Service ready.", 0, NULL);
                        FD_SET(fd_c, &set);
                        if(fd_c > fd_max) fd_max = fd_c;
                    } else if(fd == pfd[0]) {
                        // Descrittore da riaggiungere (comunicato dal thead worker)
                        int fd_c;
                        if(read(pfd[0], &fd_c, sizeof(int)) == 0) {
                            // Il descrittore di scrittura è stato chiuso
                            FD_CLR(pfd[0], &set);
                            if(pfd[0] == fd_max) {
                                int _fd = pfd[0];
                                // È sempre presente almeno il descrittore per accettare nuove connessioni
                                while(!FD_ISSET(_fd, &set)) _fd--;
                                fd_max = _fd;
                            }
                        } else {
                            FD_SET(fd_c, &set);
                            if(fd_c > fd_max) fd_max = fd_c;
                        }
                    } else {
                        // lettura request fsp
                        pthread_mutex_lock(&sfd_queue_mutex);
                        fsp_sfd_queue_enqueue(sfd_queue, fd);
                        pthread_cond_signal(&sfd_queue_isNotEmpty);
                        pthread_mutex_unlock(&sfd_queue_mutex);
                        FD_CLR(fd, &set);
                        if(fd == fd_max) {
                            int _fd = fd;
                            // È sempre presente almeno il descrittore per accettare nuove connessioni
                            while(!FD_ISSET(_fd, &set)) _fd--;
                            fd_max = _fd;
                        }
                    }
                }
            }
        }
    }
    
    
    
    return 0;
}

static int parseConfigFile(char* socket_file_name, char* log_file_name, unsigned int* _files_max_num, unsigned long int* _storage_max_size, unsigned int* max_conn, unsigned int* worker_threads_num) {
    FILE* file = NULL;
    if((file = fopen(CONFIG_FILE, "r")) == NULL) {
        return -1;
    }
    
    int lines = 6;
    const size_t buf_size = 256;
    char buf[buf_size];
    
    for(int line = 0; line < lines; line++) {
        if(fgets(buf, buf_size, file) == NULL) {
            fclose(file);
            return -2;
        }
        
        char* param_start = buf;
        char* end = param_start;
        while(*end != '\0' || *end != ':') end++;
        if(*end == '\0') {
            // Errore di sintassi
            fclose(file);
            return -2;
        }
        *end = '\0';
        
        end++;
        if(*end != ' ') return -2;
        end++;
        
        char* val_start = end;

        while(*end != '\0' || *end != '\n') end++;
        if(*end == '\0') {
            // Errore di sintassi
            fclose(file);
            return -2;
        }
        *end = '\0';
        
        long int val;
        switch(line) {
            case 0:
                // SOCKET_FILE_NAME
                if(strcmp("SOCKET_FILE_NAME", param_start) != 0) {
                    // Errore di sintassi
                    fclose(file);
                    return -2;
                }
                strncpy(socket_file_name, val_start, UNIX_PATH_MAX);
                break;
            case 1:
                // LOG_FILE_NAME
                if(strcmp("LOG_FILE_NAME", param_start) != 0) {
                    // Errore di sintassi
                    fclose(file);
                    return -2;
                }
                strncpy(log_file_name, val_start, UNIX_PATH_MAX);
                break;
            case 2:
                // FILES_MAX_NUM
                if(strcmp("FILES_MAX_NUM", param_start) != 0) {
                    // Errore di sintassi
                    fclose(file);
                    return -2;
                }
                if(!isNumber(val_start, &val)) {
                    // Errore di sintassi
                    fclose(file);
                    return -2;
                }
                *_files_max_num = (unsigned int) val;
                break;
            case 3:
                // STORAGE_MAX_SIZE
                if(strcmp("STORAGE_MAX_SIZE", param_start) != 0) {
                    // Errore di sintassi
                    fclose(file);
                    return -2;
                }
                if(!isNumber(val_start, &val)) {
                    // Errore di sintassi
                    fclose(file);
                    return -2;
                }
                *_storage_max_size = (unsigned long int) val;
                break;
            case 4:
                // MAX_CONN
                if(strcmp("MAX_CONN", param_start) != 0) {
                    // Errore di sintassi
                    fclose(file);
                    return -2;
                }
                if(!isNumber(val_start, &val)) {
                    // Errore di sintassi
                    fclose(file);
                    return -2;
                }
                *max_conn = (unsigned int) val;
                break;
            case 5:
                // WORKER_THREADS_NUM
                if(strcmp("WORKER_THREADS_NUM", param_start) != 0) {
                    // Errore di sintassi
                    fclose(file);
                    return -2;
                }
                if(!isNumber(val_start, &val)) {
                    // Errore di sintassi
                    fclose(file);
                    return -2;
                }
                *worker_threads_num = (unsigned int) val;
                break;
            default:
                break;
        }
    }
    fclose(file);
    
    return 0;
}

static void* worker(void* arg) {
    int pfd = *((int*) arg);
    
    while(1) {
        int sfd;
        CLIENT_INFO client_info = NULL;
        
        pthread_mutex_lock(&sfd_queue_mutex);
        while(fsp_sfd_queue_isEmpty(sfd_queue)) {
            pthread_cond_wait(&sfd_queue_isNotEmpty, &sfd_queue_mutex);
        }
        sfd = fsp_sfd_queue_dequeue(sfd_queue);
        pthread_mutex_unlock(&sfd_queue_mutex);
        
        pthread_mutex_lock(&clients_mutex);
        client_info = fsp_clients_hash_table_search(clients, sfd);
        pthread_mutex_unlock(&clients_mutex);
        
        if(client_info == NULL) {
            fprintf(stderr, "Errore: client con socket file descriptor %d non trovato.\n", sfd);
            continue;
        }
        
        int code = 200;
        const size_t descr_max_len = 128;
        char description[descr_max_len];
        size_t data_len = 0;
        void* data = NULL;
        
        // Legge il messaggio di richiesta
        struct fsp_request req;
        switch(receiveFspReq(client_info, &req)) {
            case -1:
                // client_info->buf == NULL || client_info->size == NULL
                // client_info->size > FSP_READER_BUF_MAX_SIZE
            case -2:
                // Errori durante la lettura
            case -3:
                // sfd ha raggiunto EOF senza aver letto un messaggio di richiesta
                
                // Chiude immediatamente la connessione
                close(client_info->sfd);
                pthread_mutex_lock(&clients_mutex);
                fsp_clients_hash_table_delete(clients, client_info->sfd);
                pthread_mutex_unlock(&clients_mutex);
                continue;
            case -4:
                // Il messaggio di richiesta contiene errori sintattici
                code = 501;
                strncpy(description, "Syntax error, message unrecognised.", descr_max_len);
                description[descr_max_len-1] = '\0';
                break;
            case -5:
                // Impossibile riallocare il buffer (memoria insufficiente)
                code = 421;
                strncpy(description, "Service not available, closing connection.", descr_max_len);
                description[descr_max_len-1] = '\0';
                break;
            default:
                break;
        }
        
        if(code != 421 && code != 501) {
            // Esegue il comando
            // Valore di ritorno
            int ret_val = 0;
            // Dati contenuti nel campo data
            struct fsp_data parsed_data;
            // Valore dell'argomento del comando READN
            long int n;
            switch(req.cmd) {
                case APPEND:
                    // Legge i dati
                    switch (fsp_parser_parseData(req.data_len, req.data, &parsed_data)) {
                        case -1:
                            // data_len <= 0 || data == NULL || parsed_data == NULL
                        case -2:
                            // Il campo data del messaggio è incompleto
                        case -3:
                            // Il campo data del messaggio contiene errori sintattici
                            code = 501;
                            strncpy(description, "Syntax error, message unrecognised.", descr_max_len);
                            description[descr_max_len-1] = '\0';
                            break;
                        case -4:
                            // Impossibile allocare memoria nello heap
                            // Chiude immediatamente la connessione
                            close(client_info->sfd);
                            pthread_mutex_lock(&clients_mutex);
                            fsp_clients_hash_table_delete(clients, client_info->sfd);
                            pthread_mutex_unlock(&clients_mutex);
                            continue;
                        default:
                            // Successo
                            break;
                    }
                    if(parsed_data.n != 1) {
                        fsp_parser_freeData(&parsed_data);
                        code = 501;
                        strncpy(description, "Syntax error, message unrecognised.", descr_max_len);
                        description[descr_max_len-1] = '\0';
                        break;
                    }
                    ret_val = append_cmd(client_info, req.arg, &parsed_data, &code, description, descr_max_len, &data_len, &data);
                    fsp_parser_freeData(&parsed_data);
                    break;
                case CLOSE:
                    ret_val = close_cmd(client_info, req.arg, &code, description, descr_max_len);
                    break;
                case LOCK:
                    ret_val = lock_cmd(client_info, req.arg, &code, description, descr_max_len);
                    break;
                case OPEN:
                    ret_val = open_cmd(client_info, req.arg, &code, description, descr_max_len);
                    break;
                case OPENC:
                    ret_val = openc_cmd(client_info, req.arg, &code, description, descr_max_len);
                    break;
                case OPENCL:
                    ret_val = opencl_cmd(client_info, req.arg, &code, description, descr_max_len);
                    break;
                case OPENL:
                    ret_val = openl_cmd(client_info, req.arg, &code, description, descr_max_len);
                    break;
                case QUIT:
                    code = 221;
                    strncpy(description, "Service closing connection.", descr_max_len);
                    description[descr_max_len-1] = '\0';
                    break;
                case READ:
                    if(isNumber(req.arg, &n)) {
                        ret_val = read_cmd(client_info, req.arg, &code, description, descr_max_len, &data_len, &data);
                    } else {
                        code = 501;
                        strncpy(description, "Syntax error, message unrecognised.", descr_max_len);
                        description[descr_max_len-1] = '\0';
                        break;
                    }
                    break;
                case READN:
                    ret_val = readn_cmd(client_info, n, &code, description, descr_max_len, &data_len, &data);
                    break;
                case REMOVE:
                    ret_val = remove_cmd(client_info, req.arg, &code, description, descr_max_len);
                    break;
                case UNLOCK:
                    ret_val = unlock_cmd(client_info, req.arg, &code, description, descr_max_len);
                    break;
                case WRITE:
                    // Legge i dati
                    switch (fsp_parser_parseData(req.data_len, req.data, &parsed_data)) {
                        case -1:
                            // data_len <= 0 || data == NULL || parsed_data == NULL
                        case -2:
                            // Il campo data del messaggio è incompleto
                        case -3:
                            // Il campo data del messaggio contiene errori sintattici
                            code = 501;
                            strncpy(description, "Syntax error, message unrecognised.", descr_max_len);
                            description[descr_max_len-1] = '\0';
                            break;
                        case -4:
                            // Impossibile allocare memoria nello heap
                            // Chiude immediatamente la connessione
                            close(client_info->sfd);
                            pthread_mutex_lock(&clients_mutex);
                            fsp_clients_hash_table_delete(clients, client_info->sfd);
                            pthread_mutex_unlock(&clients_mutex);
                            continue;
                        default:
                            // Successo
                            break;
                    }
                    if(parsed_data.n != 1) {
                        fsp_parser_freeData(&parsed_data);
                        code = 501;
                        strncpy(description, "Syntax error, message unrecognised.", descr_max_len);
                        description[descr_max_len-1] = '\0';
                        break;
                    }
                    ret_val = write_cmd(client_info, req.arg, &parsed_data, &code, description, descr_max_len, &data_len, &data);
                    fsp_parser_freeData(&parsed_data);
                    break;
                default:
                    // Mai eseguito
                    break;
            }
            if(ret_val != 0) {
                // Chiude immediatamente la connessione
                close(client_info->sfd);
                fsp_clients_hash_table_delete(clients, client_info->sfd);
                continue;
            }
        }
        
        // Invia il messaggio di risposta
        if(sendFspResp(client_info, code, description, data_len, data) != 0) {
            // Chiude immediatamente la connessione
            if(data != NULL) free(data);
            close(client_info->sfd);
            fsp_clients_hash_table_delete(clients, client_info->sfd);
            continue;
        }
        // Libera il campo data dalla memoria se necessario
        if(data != NULL) {
            free(data);
        }
        
        if(code == 221) {
            // Chiude la connessione
            if(data != NULL) free(data);
            close(client_info->sfd);
            fsp_clients_hash_table_delete(clients, client_info->sfd);
            continue;
        }
        
        // Comunica al master thread il valore sfd (attraverso una pipe senza nome)
        write(pfd, &(client_info->sfd), sizeof(int));
    }
    
    return 0;
}

static int receiveFspReq(CLIENT_INFO client_info, struct fsp_request* req) {
    if(client_info == NULL || req == NULL) return -1;
    
    int ret_val;
    struct fsp_request _req;
    if ((ret_val = fsp_reader_readRequest(client_info->sfd, &(client_info->buf), &(client_info->size), &_req)) != 0) {
        return ret_val;
    }
    
    *req = _req;
    
    return 0;
}

static int sendFspResp(CLIENT_INFO client_info, int code, const char* description, size_t data_len, void* data) {
    if(client_info == NULL) return -1;
    
    // Genera il messaggio di risposta
    long int bytes;
    switch(bytes = fsp_parser_makeResponse(&(client_info->buf), &(client_info->size), code, description, data_len, data)) {
        case -1:
            // buf == NULL || *buf == NULL || size == NULL || data_len < 0 ||
            // (data_len > 0 && data == NULL) || client_info->size > FSP_PARSER_BUF_MAX_SIZE
        case -2:
            // Non è stato possibile riallocare la memoria per client_info->buf
            return -1;
        default:
            // Successo
            break;
    }
    
    // Invia il messaggio di risposta
    char* _buf = (char*) client_info->buf;
    ssize_t w_bytes;
    while(bytes > 0) {
        if((w_bytes = write(client_info->sfd, _buf, bytes)) == -1) {
            // Errore durante la scrittura
            return -1;
        } else {
            bytes -= w_bytes;
            _buf += w_bytes;
        }
    }
    
    return 0;
}

int append_cmd(CLIENT_INFO client_info, char* pathname, struct fsp_data* parsed_data, int* code, char* description, const size_t descr_max_len, size_t* data_len, void** data) {
    if(client_info == NULL || pathname == NULL || code == NULL || description == NULL || data_len == NULL || data == NULL) return -1;
    
    struct fsp_file* file;
    int notOpened = 0;
    int removed = 0;
    int notLocked = 0;
    
    pthread_mutex_lock(&files_mutex);
    // Cerca il file
    file = fsp_files_bst_search(files, pathname);
    if(file != NULL) {
        if(!file->remove) {
            if(fsp_files_list_contains(client_info->openedFiles, pathname)) {
                // Il file è stato aperto dal client
                if(file->locked >= 0 && file->locked == client_info->sfd) {
                    // Si può scrivere sul file
                    size_t file_size_tot = file->size + parsed_data->sizes[0];
                    
                    struct fsp_file* file_tmp = file->data;
                    if((file_tmp = realloc(file->data, file_size_tot)) == NULL) {
                        pthread_mutex_unlock(&files_mutex);
                        return -1;
                    }
                    file->data = file_tmp;
                    memcpy((file->data)+file->size, parsed_data->data[0], parsed_data->sizes[0]);
                    file->size = file_size_tot;
                    storage_size += parsed_data->sizes[0];
                    
                    // Espelle i file dalla memoria se necessario
                    if(storage_size > storage_max_size) {
                        int n = 0;
                        size_t _tot_size = 0;
                        struct fsp_file* _file;
                        struct fsp_file_pathname* _file_pathname = pathnames_queue->head;
                        while(storage_size > storage_max_size) {
                            _file = fsp_files_bst_search(files, _file_pathname->pathname);
                            _file->remove = 1;
                            storage_size -= _file->size;
                            n++;
                            _tot_size += _file->size + strlen(_file->pathname) + 32;
                            _file_pathname = _file_pathname->next;
                        }
                        
                        if((*data = malloc(_tot_size)) == NULL) {
                            pthread_mutex_unlock(&files_mutex);
                            return -1;
                        }
                        
                        long int wrote_bytes_tot = 0;
                        long int wrote_bytes = 0;
                        _file_pathname = fsp_files_queue_dequeue(pathnames_queue);
                        _file = fsp_files_bst_search(files, _file_pathname->pathname);
                        wrote_bytes = fsp_parser_makeData(data, &_tot_size, 0, n, pathname, _file->size, _file->data);
                        if(wrote_bytes < 0) {
                            pthread_mutex_unlock(&files_mutex);
                            return -1;
                        }
                        wrote_bytes_tot += wrote_bytes;
                        for(int i = 0; i < n-1; i++) {
                            _file_pathname = fsp_files_queue_dequeue(pathnames_queue);
                            _file = fsp_files_bst_search(files, _file_pathname->pathname);
                            wrote_bytes = fsp_parser_makeData(data, &_tot_size, wrote_bytes, n, pathname, _file->size, _file->data);
                            if(wrote_bytes < 0) {
                                pthread_mutex_unlock(&files_mutex);
                                return -1;
                            }
                            wrote_bytes_tot += wrote_bytes;
                        }
                        *data_len = wrote_bytes;
                    }
                } else  {
                    notLocked = 1;
                }
            } else {
                notOpened = 1;
            }
        } else {
            removed = 1;
        }
    }
    pthread_mutex_unlock(&files_mutex);
    
    if(file == NULL || notOpened || removed) {
        // File inesistente o non aperto dal client
        *code = 556;
        strncpy(description, "Cannot perform the operation.", descr_max_len);
        description[descr_max_len-1] = '\0';
        return 0;
    }
    if(notLocked) {
        // Il client non ha settato la lock sul file
        *code = 554;
        strncpy(description, "Requested action not taken. No access.", descr_max_len);
        description[descr_max_len-1] = '\0';
        return 0;
    }
    
    *code = 200;
    strncpy(description, "The requested action has been successfully completed.", descr_max_len);
    description[descr_max_len-1] = '\0';
    return 0;
}

int close_cmd(CLIENT_INFO client_info, char* pathname, int* code, char* description, const size_t descr_max_len) {
    if(client_info == NULL || pathname == NULL || code == NULL || description == NULL) return -1;
    
    // Cerca il file
    struct fsp_file* file;
    int notOpened = 0;
    
    pthread_mutex_lock(&files_mutex);
    file = fsp_files_bst_search(files, pathname);
    if(file != NULL) {
        if(fsp_files_list_contains(client_info->openedFiles, pathname)) {
            // Rimuove il link dal file
            file->links--;
            // Rimuove il file dalla lista dei file aperti del client
            fsp_files_list_remove(&(client_info->openedFiles), pathname);
            // Rimuove il file se remove == 1 e links == 0
            if(file->remove && file->links == 0) {
                storage_size -= file->size;
                fsp_files_bst_delete(&files, pathname);
                fsp_file_free(file);
            } else {
                // Rimuove la lock se la detiene
                if(file->locked == client_info->sfd) {
                    file->locked = -1;
                    pthread_cond_broadcast(&lock_cmd_isNotLocked);
                }
            }
        } else {
            notOpened = 1;
        }
    }
    pthread_mutex_unlock(&files_mutex);
    
    if(file == NULL) {
        // File non trovato
        *code = 550;
        strncpy(description, "Requested action not taken. File not found.", descr_max_len);
        description[descr_max_len-1] = '\0';
        return 0;
    }
    if(notOpened) {
        // File non aperto dal client
        *code = 556;
        strncpy(description, "Cannot perform the operation.", descr_max_len);
        description[descr_max_len-1] = '\0';
        return 0;
    }
    
    *code = 200;
    strncpy(description, "The requested action has been successfully completed.", descr_max_len);
    
    return 0;
}

int lock_cmd(CLIENT_INFO client_info, char* pathname, int* code, char* description, const size_t descr_max_len) {
    if(client_info == NULL || pathname == NULL || code == NULL || description == NULL) return -1;
    
    // Cerca il file
    struct fsp_file* file;
    int file_not_opened = 0;
    int locked = 0;
    
    pthread_mutex_lock(&files_mutex);
    file = fsp_files_bst_search(files, pathname);
    if(file != NULL) {
        if(!fsp_files_list_contains(client_info->openedFiles, pathname)) {
            // Il client non ha aperto il file
            file_not_opened = 1;
        } else if(file->locked < 0 || file->locked == client_info->sfd) {
            // È già stata settata la lock sul file
            file->locked = client_info->sfd;
            locked = 1;
        }
    }
    pthread_mutex_unlock(&files_mutex);
    
    if(file == NULL) {
        // File non trovato
        *code = 550;
        strncpy(description, "Requested action not taken. File not found.", descr_max_len);
        description[descr_max_len-1] = '\0';
        return 0;
    }
    if(file_not_opened) {
        *code = 556;
        strncpy(description, "Cannot perform the operation.", descr_max_len);
        description[descr_max_len-1] = '\0';
        return 0;
    }
    if(locked) {
        *code = 200;
        strncpy(description, "The requested action has been successfully completed.", descr_max_len);
        description[descr_max_len-1] = '\0';
        return 0;
    }
    
    // Attende di ottenere la lock su una variabile di condizione
    pthread_mutex_lock(&files_mutex);
    while(file->locked >= 0) {
        pthread_cond_wait(&lock_cmd_isNotLocked, &files_mutex);
    }
    file->locked = client_info->sfd;
    pthread_mutex_unlock(&files_mutex);
    
    *code = 200;
    strncpy(description, "The requested action has been successfully completed.", descr_max_len);
    
    return 0;
}

int open_cmd(CLIENT_INFO client_info, char* pathname, int* code, char* description, const size_t descr_max_len) {
    if(client_info == NULL || pathname == NULL || code == NULL || description == NULL) return -1;
    
    // Cerca il file
    struct fsp_file* file;
    
    pthread_mutex_lock(&files_mutex);
    file = fsp_files_bst_search(files, pathname);
    if(file != NULL) {
        // Se il file sarà da rimuovere, allora non lo apre
        if(file->remove) {
            file = NULL;
        } else if(!fsp_files_list_contains(client_info->openedFiles, pathname)) {
            // Aggiunge un nuovo collegamento al file
            file->links++;
            // Aggiunge il file nella lista dei file aperti dal client
            fsp_files_list_add(&(client_info->openedFiles), file);
        }
        // Se il file era già aperto dal client, allora non fa niente
    }
    pthread_mutex_unlock(&files_mutex);
    
    if(file == NULL) {
        // File non trovato
        *code = 550;
        strncpy(description, "Requested action not taken. File not found.", descr_max_len);
        description[descr_max_len-1] = '\0';
        return 0;
    }
    
    *code = 200;
    strncpy(description, "The requested action has been successfully completed.", descr_max_len);
    description[descr_max_len-1] = '\0';
    
    return 0;
}

int openc_cmd(CLIENT_INFO client_info, char* pathname, int* code, char* description, const size_t descr_max_len) {
    if(client_info == NULL || pathname == NULL || code == NULL || description == NULL) return -1;
    
    // Cerca il file
    int already_exists = 0;
    int remove = 0;
    
    pthread_mutex_lock(&files_mutex);
    struct fsp_file* file = fsp_files_bst_search(files, pathname);
    if(file == NULL) {
        // Il file non esiste
        // Crea il file
        if((file = fsp_file_new(pathname, NULL, 0, 1, -1, 0)) == NULL) {
            pthread_mutex_unlock(&files_mutex);
            return -1;
        }
        // Aggiunge il file all'albero
        fsp_files_bst_insert(&files, file);
        // Aggiunge il file nella lista dei file aperti dal client
        fsp_files_list_add(&(client_info->openedFiles), file);
    } else if(file->remove) {
        // Il file esiste
        // Non permette l'operazione anche se il file ha remove == 1
        remove = 1;
    } else {
        // Il file esiste
        already_exists = 1;
    }
    pthread_mutex_unlock(&files_mutex);
    
    if(already_exists) {
        // File già esistente
        *code = 555;
        strncpy(description, "Requested action not taken. File already exists.", descr_max_len);
        description[descr_max_len-1] = '\0';
        return 0;
    }
    if(remove) {
        // Impossibile eseguire l'operazione perchè il file non è ancora stato chiuso da tutti i client
        // per la cancellazione
        *code = 556;
        strncpy(description, "Cannot perform the operation.", descr_max_len);
        description[descr_max_len-1] = '\0';
        return 0;
    }
    
    *code = 200;
    strncpy(description, "The requested action has been successfully completed.", descr_max_len);
    description[descr_max_len-1] = '\0';
    
    return 0;
}

int opencl_cmd(CLIENT_INFO client_info, char* pathname, int* code, char* description, const size_t descr_max_len) {
    if(client_info == NULL || pathname == NULL || code == NULL || description == NULL) return -1;
    
    // Cerca il file
    int already_exists = 0;
    int remove = 0;
    
    pthread_mutex_lock(&files_mutex);
    struct fsp_file* file = fsp_files_bst_search(files, pathname);
    if(file == NULL) {
        // Il file non esiste
        // Crea il file
        if((file = fsp_file_new(pathname, NULL, 0, 1, client_info->sfd, 0)) == NULL) {
            pthread_mutex_unlock(&files_mutex);
            return -1;
        }
        // Aggiunge il file all'albero
        fsp_files_bst_insert(&files, file);
        // Aggiunge il file nella lista dei file aperti dal client
        fsp_files_list_add(&(client_info->openedFiles), file);
    } else if(file->remove) {
        // Il file esiste
        // Non permette l'operazione anche se il file ha remove == 1
        remove = 1;
    } else {
        // Il file esiste
        already_exists = 1;
    }
    pthread_mutex_unlock(&files_mutex);
    
    if(already_exists) {
        // File già esistente
        *code = 555;
        strncpy(description, "Requested action not taken. File already exists.", descr_max_len);
        description[descr_max_len-1] = '\0';
        return 0;
    }
    if(remove) {
        // Impossibile eseguire l'operazione perchè il file non è ancora stato chiuso da tutti i client
        // per la cancellazione
        *code = 556;
        strncpy(description, "Cannot perform the operation.", descr_max_len);
        description[descr_max_len-1] = '\0';
        return 0;
    }
    
    *code = 200;
    strncpy(description, "The requested action has been successfully completed.", descr_max_len);
    description[descr_max_len-1] = '\0';
    
    return 0;
}

int openl_cmd(CLIENT_INFO client_info, char* pathname, int* code, char* description, const size_t descr_max_len) {
    if(client_info == NULL || pathname == NULL || code == NULL || description == NULL) return -1;
    
    // Cerca il file
    struct fsp_file* file;
    int alreadyLocked = 0;
    
    pthread_mutex_lock(&files_mutex);
    file = fsp_files_bst_search(files, pathname);
    if(file != NULL) {
        if(file->remove) {
            // Se il file sarà da rimuovere, allora non lo apre
            file = NULL;
        } else if(file->locked >= 0 && file->locked != client_info->sfd) {
            alreadyLocked = 1;
        } else if(file->locked < 0) {
            // Se il file era già aperto dal client, esso viene riaperto con lock
            // Setta la lock
            file->locked = client_info->sfd;
            if(!fsp_files_list_contains(client_info->openedFiles, pathname)) {
                // Aggiunge un nuovo collegamento al file
                file->links++;
                // Aggiunge il file nella lista dei file aperti dal client
                fsp_files_list_add(&(client_info->openedFiles), file);
            }
        }
    }
    pthread_mutex_unlock(&files_mutex);
    
    if(file == NULL) {
        // File non trovato
        *code = 550;
        strncpy(description, "Requested action not taken. File not found.", descr_max_len);
        description[descr_max_len-1] = '\0';
        return 0;
    }
    if(alreadyLocked) {
        // Lock già settata sul file da un altro client
        *code = 556;
        strncpy(description, "Cannot perform the operation.", descr_max_len);
        description[descr_max_len-1] = '\0';
        return 0;
    }
    
    *code = 200;
    strncpy(description, "The requested action has been successfully completed.", descr_max_len);
    description[descr_max_len-1] = '\0';
    
    return 0;
}

int read_cmd(CLIENT_INFO client_info, char* pathname, int* code, char* description, const size_t descr_max_len, size_t* data_len, void** data) {
    if(client_info == NULL || pathname == NULL || code == NULL || description == NULL || data_len == NULL || data == NULL) return -1;
    
    struct fsp_file* file;
    int notOpened = 0;
    int locked = 0;
    
    pthread_mutex_lock(&files_mutex);
    // Cerca il file
    file = fsp_files_bst_search(files, pathname);
    if(file != NULL) {
        if(fsp_files_list_contains(client_info->openedFiles, pathname)) {
            // Il file è stato aperto dal client
            if(file->locked < 0 || (file->locked >= 0 && file->locked == client_info->sfd)) {
                // Il file si può leggere
                size_t buf_size = file->size + strlen(pathname) + 32;
                if((*data = malloc(buf_size)) == NULL) {
                    pthread_mutex_unlock(&files_mutex);
                    return -1;
                }
                long int wrote_bytes = 0;
                wrote_bytes = fsp_parser_makeData(data, &buf_size, 0, 1, pathname, file->size, file->data);
                if(wrote_bytes < 0) {
                    free(*data);
                    pthread_mutex_unlock(&files_mutex);
                    return -1;
                }
                *data_len = wrote_bytes;
            } else {
                locked = 1;
            }
        } else {
            notOpened = 1;
        }
    }
    pthread_mutex_unlock(&files_mutex);
    
    if(file == NULL) {
        // File non trovato
        *code = 550;
        strncpy(description, "Requested action not taken. File not found.", descr_max_len);
        description[descr_max_len-1] = '\0';
        return 0;
    }
    if(notOpened) {
        // File non aperto dal client
        *code = 556;
        strncpy(description, "Cannot perform the operation.", descr_max_len);
        description[descr_max_len-1] = '\0';
        return 0;
    }
    if(locked) {
        // Un altro client ha settato la lock sul file
        *code = 554;
        strncpy(description, "Requested action not taken. No access.", descr_max_len);
        description[descr_max_len-1] = '\0';
        return 0;
    }
    
    *code = 200;
    strncpy(description, "The requested action has been successfully completed.", descr_max_len);
    description[descr_max_len-1] = '\0';
    return 0;
}

int readn_cmd(CLIENT_INFO client_info, long int n, int* code, char* description, const size_t descr_max_len, size_t* data_len, void** data) {
    if(client_info == NULL || code == NULL || description == NULL || data_len == NULL || data == NULL) return -1;
    
    pthread_mutex_lock(&files_mutex);
    size_t buf_size;
    // Stima la dimensione del buffer
    if(n <= 0) {
        buf_size = storage_size + files_num*(256 + 32);
    } else {
        buf_size = storage_size/n + n*(256 + 32);
    }
    if((*data = malloc(buf_size)) == NULL) return -1;
    
    long int wrote_bytes_tot = 0;
    
    wrote_bytes_tot = fsp_parser_makeData(data, &buf_size, 0, n > 0 ? (int) n : files_num, files->pathname, files->size, files->data);
    if(wrote_bytes_tot < 0) {
        free(*data);
        return -1;
    }
    
    if(readn_rec(files->left, &n, &wrote_bytes_tot, &buf_size, data) != 0) {
        free(*data);
        return -1;
    }
    if(readn_rec(files->right, &n, &wrote_bytes_tot, &buf_size, data) != 0) {
        free(*data);
        return -1;
    }
    *data_len = wrote_bytes_tot;
    pthread_mutex_unlock(&files_mutex);
    
    *code = 200;
    strncpy(description, "The requested action has been successfully completed.", descr_max_len);
    description[descr_max_len-1] = '\0';
    return 0;
}

int readn_rec(struct fsp_file* root_file, long int* n, long int* wrote_bytes_tot, size_t* data_len, void** data) {
    if(root_file == NULL || *n == 0) return 0;
    long int wrote_bytes = 0;
    wrote_bytes = fsp_parser_makeData(data, data_len, *wrote_bytes_tot, -1, root_file->pathname, root_file->size, root_file->data);
    if(wrote_bytes < 0) {
        return -1;
    }
    (*n)--;
    *wrote_bytes_tot += wrote_bytes;
    
    if(readn_rec(root_file->left, n, wrote_bytes_tot, data_len, data) == -1) return -1;
    if(readn_rec(root_file->right, n, wrote_bytes_tot, data_len, data) == -1) return -1;
    return 0;
}

int remove_cmd(CLIENT_INFO client_info, char* pathname, int* code, char* description, const size_t descr_max_len) {
    if(client_info == NULL || pathname == NULL || code == NULL || description == NULL) return -1;
    
    struct fsp_file* file;
    int notLocked = 0;
    
    pthread_mutex_lock(&files_mutex);
    // Cerca il file
    file = fsp_files_bst_search(files, pathname);
    if(file != NULL) {
        if(file->locked == client_info->sfd) {
            file->remove = 1;
            // Il file viene rimosso dal server solo quando file->links == 0
        } else {
            notLocked = 1;
        }
    }
    pthread_mutex_unlock(&files_mutex);
    
    if(file == NULL) {
        // File non trovato
        *code = 550;
        strncpy(description, "Requested action not taken. File not found.", descr_max_len);
        description[descr_max_len-1] = '\0';
        return 0;
    }
    if(notLocked) {
        // Il client non ha aperto il file o non è l'owner della lock
        *code = 554;
        strncpy(description, "Requested action not taken. No access.", descr_max_len);
        description[descr_max_len-1] = '\0';
        return 0;
    }
    
    *code = 200;
    strncpy(description, "The requested action has been successfully completed.", descr_max_len);
    description[descr_max_len-1] = '\0';
    return 0;
}

int unlock_cmd(CLIENT_INFO client_info, char* pathname, int* code, char* description, const size_t descr_max_len) {
    if(client_info == NULL || pathname == NULL || code == NULL || description == NULL) return -1;
    
    // Cerca il file
    struct fsp_file* file;
    int locked;
    
    pthread_mutex_lock(&files_mutex);
    file = fsp_files_bst_search(files, pathname);
    if(file != NULL) locked = file->locked;
    pthread_mutex_unlock(&files_mutex);
    
    if(file == NULL) {
        // File non trovato
        *code = 550;
        strncpy(description, "Requested action not taken. File not found.", descr_max_len);
        description[descr_max_len-1] = '\0';
        return 0;
    }
    if(!fsp_files_list_contains(client_info->openedFiles, pathname) || locked != client_info->sfd) {
        // Il client non ha aperto il file o non è l'owner della lock
        *code = 556;
        strncpy(description, "Cannot perform the operation.", descr_max_len);
        description[descr_max_len-1] = '\0';
        return 0;
    }
    
    // Rilascia la lock
    pthread_mutex_lock(&files_mutex);
    file->locked = -1;
    pthread_cond_broadcast(&lock_cmd_isNotLocked);
    pthread_mutex_unlock(&files_mutex);
    
    *code = 200;
    strncpy(description, "The requested action has been successfully completed.", descr_max_len);
    description[descr_max_len-1] = '\0';
    return 0;
}

int write_cmd(CLIENT_INFO client_info, char* pathname, struct fsp_data* parsed_data, int* code, char* description, const size_t descr_max_len, size_t* data_len, void** data) {
    if(client_info == NULL || pathname == NULL || code == NULL || description == NULL || data_len == NULL || data == NULL) return -1;
    
    struct fsp_file* file;
    int notOpened = 0;
    int removed = 0;
    int notCreated_or_notLocked = 0;
    int noMemory = 0;
    
    pthread_mutex_lock(&files_mutex);
    // Cerca il file
    file = fsp_files_bst_search(files, pathname);
    if(file != NULL) {
        if(!file->remove) {
            if(fsp_files_list_contains(client_info->openedFiles, pathname)) {
                // Il file è stato aperto dal client
                if(file->data == NULL && file->locked >= 0 && file->locked == client_info->sfd) {
                    // Il file si può scrivere
                    if((file->data = malloc(sizeof(parsed_data->sizes[0]))) == NULL) {
                        pthread_mutex_unlock(&files_mutex);
                        return -1;
                    }
                    memcpy(file->data, parsed_data->data[0], parsed_data->sizes[0]);
                    file->size = parsed_data->sizes[0];
                    storage_size += parsed_data->sizes[0];
                    
                    // Espelle i file dalla memoria se necessario
                    if(storage_size > storage_max_size) {
                        int n = 0;
                        size_t _tot_size = 0;
                        struct fsp_file* _file;
                        struct fsp_file_pathname* _file_pathname = pathnames_queue->head;
                        while(storage_size > storage_max_size) {
                            _file = fsp_files_bst_search(files, _file_pathname->pathname);
                            _file->remove = 1;
                            storage_size -= _file->size;
                            n++;
                            _tot_size += _file->size + strlen(_file->pathname) + 32;
                            _file_pathname = _file_pathname->next;
                        }
                        
                        if((*data = malloc(_tot_size)) == NULL) {
                            pthread_mutex_unlock(&files_mutex);
                            return -1;
                        }
                        
                        long int wrote_bytes_tot = 0;
                        long int wrote_bytes = 0;
                        _file_pathname = fsp_files_queue_dequeue(pathnames_queue);
                        _file = fsp_files_bst_search(files, _file_pathname->pathname);
                        wrote_bytes = fsp_parser_makeData(data, &_tot_size, 0, n, pathname, _file->size, _file->data);
                        if(wrote_bytes < 0) {
                            pthread_mutex_unlock(&files_mutex);
                            return -1;
                        }
                        wrote_bytes_tot += wrote_bytes;
                        for(int i = 0; i < n-1; i++) {
                            _file_pathname = fsp_files_queue_dequeue(pathnames_queue);
                            _file = fsp_files_bst_search(files, _file_pathname->pathname);
                            wrote_bytes = fsp_parser_makeData(data, &_tot_size, wrote_bytes, n, pathname, _file->size, _file->data);
                            if(wrote_bytes < 0) {
                                pthread_mutex_unlock(&files_mutex);
                                return -1;
                            }
                            wrote_bytes_tot += wrote_bytes;
                        }
                        *data_len = wrote_bytes;
                    }
                } else  {
                    notCreated_or_notLocked = 1;
                }
            } else {
                notOpened = 1;
            }
        } else {
            removed = 1;
        }
    }
    pthread_mutex_unlock(&files_mutex);
    
    if(file == NULL || notOpened || removed) {
        // File inesistente o non aperto dal client
        *code = 556;
        strncpy(description, "Cannot perform the operation.", descr_max_len);
        description[descr_max_len-1] = '\0';
        return 0;
    }
    if(notCreated_or_notLocked) {
        // Il file non è stato aperto con il flag CREATE o il client non ha settato la lock su di esso
        *code = 554;
        strncpy(description, "Requested action not taken. No access.", descr_max_len);
        description[descr_max_len-1] = '\0';
        return 0;
    }
    if(noMemory) {
        // È stato superato il limite di numeri di file memorizzabili
        *code = 552;
        strncpy(description, "Requested file action aborted. Exceeded storage allocation.", descr_max_len);
        description[descr_max_len-1] = '\0';
        return 0;
    }
    
    *code = 200;
    strncpy(description, "The requested action has been successfully completed.", descr_max_len);
    description[descr_max_len-1] = '\0';
    return 0;
}
