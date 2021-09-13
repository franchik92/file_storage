#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/time.h>
#include <pthread.h>
#include <string.h>
#include <signal.h>
#include <errno.h>

#include <fsp_file.h>
#include <fsp_files_hash_table.h>
#include <fsp_files_queue.h>
#include <fsp_files_list.h>
#include <fsp_client.h>
#include <fsp_clients_hash_table.h>
#include <fsp_sfd_queue.h>
#include <fsp_parser.h>
#include <fsp_reader.h>
#include <utils.h>

#ifndef UNIX_PATH_MAX
#define UNIX_PATH_MAX 104
#endif

// Nome del file di configurazione
#define CONFIG_FILE "./config.txt"
// Dimensione di defualt dei buffer usati dai client (4MB)
#define FSP_CLIENT_DEF_BUF_SIZE 4194304
// Dimensioni delle tabelle hash
#define FSP_FILES_HASH_TABLE_SIZE 49157
#define FSP_CLIENTS_HASH_TABLE_SIZE 97

// File
typedef struct fsp_file* FSP_FILE;
// Tabella hash contenente tutti i file presenti nel server
typedef struct fsp_files_hash_table* FILES;
// Coda usata per l'espulsione dei file dal server
typedef struct fsp_files_queue* FILES_QUEUE;
// Lista dei file aperti (una per ogni client)
typedef struct fsp_files_list* OPENED_FILES;
// Client
typedef struct fsp_client* CLIENT;
// Tabella hash contenente tutti i client connessi al server
typedef struct fsp_clients_hash_table* CLIENTS;
// Coda in cui vengono inseriti i sfd per la comunicazione dal thread master ai thread worker
typedef struct fsp_sfd_queue* SFD_QUEUE;

// Strutture dati condivise tra i thread
static FILES files = NULL;
static FILES_QUEUE files_queue = NULL;
static CLIENTS clients = NULL;
static SFD_QUEUE sfd_queue = NULL;

// Il file di log
static int log_file;

// Pipe per la comunicazione dei sfd dai thread worker al thread master
static int pfd[2];

// Mutex
// files_mutex viene usato per l'accesso alle strutture dati files e files_queue
// clients_mutex viene usato per l'accesso alle strutture dati clients e sfd_queue
static pthread_mutex_t files_mutex;
static pthread_mutex_t clients_mutex;

// Variabili di condizione
static pthread_cond_t sfd_queue_isNotEmpty;
static pthread_cond_t lock_cmd_isNotLocked;

// Struttura contenente i valori letti dal file di configurazione
// Dopo la lettura del file di configurazione, l'accesso a questa struttura avviene in sola lettura
static struct {
    // Nome del file socket
    char socket_file_name[UNIX_PATH_MAX];
    // Nome del file di log
    char log_file_name[UNIX_PATH_MAX];
    // Numero massimo di file che il server può mantenere in memoria
    unsigned int files_max_num;
    // Capacità massima di memoria del server in byte
    unsigned long int storage_max_size;
    // Numero massimo di connessioni
    unsigned int max_conn;
    // Numero dei thread worker
    unsigned int worker_threads_num;
} config_file = {"/tmp/file_storage.sk", "./log_file.txt", 1000, 64, 16, 4};

// Variabile che indica se il programma deve terminare (quit == 1) o meno (quit == 0)
static volatile sig_atomic_t quit = 0;
// Variabile che indica se il server può accettare nuove connessioni (accept_connections == 1) o meno (accept_connections == 0)
static volatile sig_atomic_t accept_connections = 1;

// Quando i thread worker sono in esecuzione, viene usato files_mutex per l'accesso alle variabili
// files_num, storage_size, files_max_reached_num, storage_max_reached_size e capacity_misses

// Numero dei file presenti
static unsigned int files_num = 0;
// Dimensione della memoria in bytes occupata dai file
static unsigned long int storage_size = 0;

// Numero di file massimo memorizzato nel server
static unsigned int files_max_reached_num = 0;
// Dimensione massima in bytes raggiunta dal file storage
static unsigned long int storage_max_reached_size = 0;
// Numero di volte in cui l'algoritmo di rimpiazzamento della cache è stato eseguito per selezionare uno o più file vittima
static unsigned int capacity_misses = 0;

// Numero dei thread worker attivi (usata con clients_mutex quando i worker thread sono in esecuzione)
static unsigned int active_workers = 0;

/**
 * \brief Libera dalla memoria ogni struttura dati condivisa (files, files_queue, clients, sfd_queue)
 *        assieme ai suoi elementi e chiude tutte le connessioni attive con i client.
 */
static void freeAll(void);

/**
 * \brief Libera file dalla memoria.
 */
static void removeFile(FSP_FILE file);

/**
 * \brief Stampa il nome del file su stdout e libera file dalla memoria.
 */
static void printAndRemoveFile(FSP_FILE file);

/**
 * \brief Chiude la connessione con il socket file descriptor di client e libera client dalla memoria.
 */
static void removeClient(CLIENT client);

/**
 * \brief Chiude la connessione con il socket file descriptor di client e chiude i file aperti da client.
 *        Stampa nel file di log l'avvenuta chiusura della connessione.
 *        Se error_descr != NULL, aggiunge error_descr nel file di log.
 */
static void closeConnection(CLIENT client, char* error_descr);

/**
 * \brief Gestore dei segnali.
 */
static void signalHandler(int signal);

/**
 * \brief Esegue il parse del file di configurazione CONFIG_FILE_PATH e salva i suoi valori in config_file.
 *
 * \return 0 in caso di successo,
 *         -1 se non è stato possibile aprire il file,
 *         -2 se il file contiene errori sintattici.
 */
static int parseConfigFile(void);

/**
 * \brief Stampa nel file di log l'esito del comando req->cmd richiesto dal client client->sfd ed eseguito dal thread thread_id.
 *
 * L'esito del comando viene determinato in base al codice di risposta fsp resp_code.
 * Se il comando di richiesta è APPEND, READ, READN, REMOVE o WRITE e il codice di risposta è 200,
 * allora stampa tra parentesi anche il numero di byte letti/scritti/rimossi.
 * Non stampa nulla se client == NULL || req == NULL.
 */
static void updateLogFile(int thread_id, CLIENT client, struct fsp_request* req, int resp_code, unsigned long int bytes);

/**
 * \brief Funzione eseguita dai thread worker.
 *        Stampa nel file di log i comandi eseguiti.
 */
static void* worker(void* arg);

/**
 * \brief Legge da sfd una request fsp e la salva in req.
 *
 * \return 0 in caso di successo,
 *         -1 se client == NULL || req == NULL || client->buf == NULL ||
 *               client->size == NULL || client->size > FSP_READER_BUF_MAX_SIZE,
 *         -2 in caso di errori durante la lettura (read() setta errno appropriatamente),
 *         -3 se sfd ha raggiunto EOF senza aver letto un messaggio di richiesta,
 *         -4 se il messaggio contiene errori sintattici,
 *         -5 se è stato impossibile riallocare il buffer (memoria insufficiente).
 */
static int receiveFspReq(CLIENT client, struct fsp_request* req);

/**
 * \brief Scrive su sfd un messaggio di risposta fsp con i campi code, description, data_len e data.
 *
 * \return 0 in caso di successo,
 *         -1 altrimenti.
 */
static int sendFspResp(CLIENT client, int code, const char* description, size_t data_len, void* data);

/**
 * \brief Rimuove i file dal server in seguito a capacity miss e li salva nel formato fsp del campo data in *data.
 *        Se data_len != NULL, allora salva in *data_len la lunghezza di *data.
 *
 * \return 0 in caso di successo,
 *         -1 altrimenti.
 */
static int capacityMiss(void** data, size_t* data_len);

/* Funzioni che eseguono i comandi richiesti dai client e restituiscono un messaggio di risposta.
 * Prendono in input le informazioni del client (client), la request req, la response resp
 * in cui salvano i campi del messaggio di risposta fsp e la lunghezza massima del campo descrizione in resp descr_max_len.
 * append_cmd, read_cmd, readn_cmd e write_cmd possono allocare memoria per il campo data in resp (resp->data). In tal caso
 * il valore restituito dalla funzione sarà maggiore di zero. Liberare resp->data dalla memoria con free().
 * Se non viene allocata memoria per il campo data, allora resp->data sarà uguale a NULL.
 * Stampa nel file di log l'avvenuto capacity miss.
 *
 * Le funzioni close_cmd, lock_cmd, open_cmd, openc_cmd, opencl_cmd, openl_cmd e unlock_cmd restituiscono zero in caso di successo.
 * Le funzioni append_cmd, read_cmd, readn_cmd, remove_cmd e write_cmd restituiscono un valore maggiore o uguale a zero che indica
 * il numero di byte letti/scritti/rimossi.
 * Se restituiscono -1 (errore), allora il relativo comando non è stato eseguito e
 * i valori salvati in resp non sono significativi.
 */
static unsigned long int append_cmd(CLIENT client, struct fsp_request* req, struct fsp_response* resp, const size_t descr_max_len);

static int close_cmd(CLIENT client, struct fsp_request* req, struct fsp_response* resp, const size_t descr_max_len);

static int lock_cmd(CLIENT client, struct fsp_request* req, struct fsp_response* resp, const size_t descr_max_len);

static int open_cmd(CLIENT client, struct fsp_request* req, struct fsp_response* resp, const size_t descr_max_len);

static int openc_cmd(CLIENT client, struct fsp_request* req, struct fsp_response* resp, const size_t descr_max_len);

static int opencl_cmd(CLIENT client, struct fsp_request* req, struct fsp_response* resp, const size_t descr_max_len);

static int openl_cmd(CLIENT client, struct fsp_request* req, struct fsp_response* resp, const size_t descr_max_len);

static unsigned long int read_cmd(CLIENT client, struct fsp_request* req, struct fsp_response* resp, const size_t descr_max_len);

static unsigned long int readn_cmd(CLIENT client, struct fsp_request* req, struct fsp_response* resp, const size_t descr_max_len);

static unsigned long int remove_cmd(CLIENT client, struct fsp_request* req, struct fsp_response* resp, const size_t descr_max_len);

static int unlock_cmd(CLIENT client, struct fsp_request* req, struct fsp_response* resp, const size_t descr_max_len);

static unsigned long int write_cmd(CLIENT client, struct fsp_request* req, struct fsp_response* resp, const size_t descr_max_len);

int main(int argc, const char* argv[]) {
    
    // Esegue il parse del file di configurazione
    switch(parseConfigFile()) {
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
    printf("File di configurazione %s letto con successo.\n", CONFIG_FILE);
    
    // Apre il file di log in scrittura
    if((log_file = open(config_file.log_file_name, O_WRONLY | O_APPEND | O_CREAT, 0666)) == -1) {
        perror(NULL);
        return -1;
    }
    printf("File di log %s aperto in scrittura.\n", config_file.log_file_name);
    
    // Inizializza le strutture dati
    if(config_file.files_max_num)
    if((files = fsp_files_hash_table_new(FSP_FILES_HASH_TABLE_SIZE)) == NULL ||
       (files_queue = fsp_files_queue_new()) == NULL ||
       (clients = fsp_clients_hash_table_new(FSP_CLIENTS_HASH_TABLE_SIZE)) == NULL ||
       (sfd_queue = fsp_sfd_queue_new(config_file.max_conn)) == NULL) {
        fprintf(stderr, "Errore: memoria insufficiente.\n");
        freeAll();
        close(log_file);
        return -1;
    }
    // Mutex
    pthread_mutexattr_t mutex_attr;
    pthread_mutexattr_init(&mutex_attr);
    pthread_mutexattr_settype(&mutex_attr, PTHREAD_MUTEX_RECURSIVE);
    if(pthread_mutex_init(&files_mutex, &mutex_attr) != 0 ||
       pthread_mutex_init(&clients_mutex, NULL) != 0) {
        fprintf(stderr, "Errore: mutex non creato.\n");
        freeAll();
        close(log_file);
        return -1;
    }
    // Condition variables
    if(pthread_cond_init(&sfd_queue_isNotEmpty, NULL) != 0 ||
       pthread_cond_init(&lock_cmd_isNotLocked, NULL) != 0) {
        fprintf(stderr, "Errore: variabile di condizione non creata.\n");
        freeAll();
        close(log_file);
        return -1;
    }
    // Pipe senza nome per la comunicazione tra i thread worker e il thread master
    if(pipe(pfd) != 0) {
        fprintf(stderr, "Errore: pipe non creata.\n");
        freeAll();
        close(log_file);
        return -1;
    }
    printf("Strutture dati inizializzate.\n");
    
    // Imposta la gestione dei segnali
    struct sigaction sa;
    sigset_t mask;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signalHandler;
    sigfillset(&mask);
    sa.sa_mask = mask;
    if(sigaction(SIGINT, &sa, NULL) != 0 ||
       sigaction(SIGQUIT, &sa, NULL) != 0 ||
       sigaction(SIGHUP, &sa, NULL) != 0) {
        perror(NULL);
        freeAll();
        close(pfd[0]);
        close(pfd[1]);
        close(log_file);
        return -1;
    }
    printf("Gestione dei segnali impostata.\n");
    
    // socket
    int sfd;
    if((sfd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
        perror(NULL);
        freeAll();
        close(pfd[0]);
        close(pfd[1]);
        close(log_file);
        return -1;
    }
    printf("Socket di tipo SOCK_STREAM creato.\n");
    // bind
    struct sockaddr_un sockaddr;
    sockaddr.sun_family = AF_UNIX;
    strncpy(sockaddr.sun_path, config_file.socket_file_name, UNIX_PATH_MAX);
    if(bind(sfd, (struct sockaddr*) &sockaddr, sizeof(sockaddr)) == -1) {
        perror(NULL);
        freeAll();
        close(sfd);
        close(pfd[0]);
        close(pfd[1]);
        close(log_file);
        return -1;
    }
    printf("Nome %s assegnato al socket.\n", config_file.socket_file_name);
    // listen
    if(listen(sfd, SOMAXCONN) == -1) {
        perror(NULL);
        freeAll();
        close(sfd);
        close(pfd[0]);
        close(pfd[1]);
        close(log_file);
        return -1;
    }
    printf("Socket in ascolto.\n");
    
    // Crea i thread
    active_workers = config_file.worker_threads_num;
    pthread_t* threads = NULL;
    if((threads = malloc(sizeof(pthread_t)*config_file.worker_threads_num)) == NULL) {
        fprintf(stderr, "Errore: memoria insufficiente.\n");
        freeAll();
        close(sfd);
        close(pfd[0]);
        close(pfd[1]);
        close(log_file);
        return -1;
    }
    for(int i = 0; i < config_file.worker_threads_num; i++) {
        if(pthread_create(&(threads[i]), NULL, worker, (void*) (unsigned long int)(i+1)) != 0) {
            fprintf(stderr, "Errore: impossibile creare un nuovo thread.\n");
            for(int j = 0; j <= i; j++) {
                pthread_detach(threads[j]);
            }
            freeAll();
            close(sfd);
            close(pfd[0]);
            close(pfd[1]);
            free(threads);
            close(log_file);
        }
    }
    printf("Avvio dell'esecuzione dei thread worker.\n");
    
    // select
    int ready_descriptors_num;
    struct timeval timeout;
    memset(&timeout, 0, sizeof(timeout));
    timeout.tv_sec = 5;
    fd_set set, rdset;
    FD_ZERO(&set);
    FD_SET(sfd, &set);
    FD_SET(pfd[0], &set);
    int fd_max = 0;
    if(pfd[0] > fd_max) fd_max = pfd[0];
    if(sfd > fd_max) fd_max = sfd;
    int break_loop = 0;
    while(break_loop) {
        rdset = set;
        if((ready_descriptors_num = select(fd_max+1, &rdset, NULL, NULL, &timeout)) == -1) {
            if(errno == EINTR) {
                // Segnale ricevuto
                memset(&timeout, 0, sizeof(timeout));
                pthread_mutex_lock(&clients_mutex);
                if(quit || (!accept_connections && clients->clients_num == 0)) {
                    pthread_cond_signal(&sfd_queue_isNotEmpty);
                } else {
                    timeout.tv_sec = 5;
                }
                pthread_mutex_unlock(&clients_mutex);
                continue;
            } else {
                // Termina l'esecuzione
                perror(NULL);
                for(int i = 0; i < config_file.worker_threads_num; i++) {
                    pthread_detach(threads[i]);
                }
                freeAll();
                close(sfd);
                close(pfd[0]);
                close(pfd[1]);
                free(threads);
                close(log_file);
                return -1;
            }
        } else {
            if(ready_descriptors_num == 0) {
                // Timeout
                memset(&timeout, 0, sizeof(timeout));
                pthread_mutex_lock(&clients_mutex);
                if(quit || (!accept_connections && clients->clients_num == 0)) {
                    pthread_cond_signal(&sfd_queue_isNotEmpty);
                } else {
                    timeout.tv_sec = 5;
                }
                pthread_mutex_unlock(&clients_mutex);
                continue;
            }
            for(int fd = 0; fd < fd_max; fd++) {
                if(FD_ISSET(fd, &rdset)) {
                    if(fd == sfd) {
                        if(quit || !accept_connections) {
                            // Il server non accetta più nuove connessioni
                            FD_CLR(fd, &set);
                            if(fd == fd_max) {
                                // Determina il nuovo fd_max
                                int _fd = fd;
                                // È sempre presente almeno il file descriptor della pipe
                                while(!FD_ISSET(_fd, &set)) _fd--;
                                fd_max = _fd;
                            }
                            continue;
                        }
                        
                        // Accetta una nuova connessione
                        int fd_c;
                        if((fd_c = accept(sfd, NULL, 0)) != -1) {
                            perror(NULL);
                            continue;
                        }
                        
                        // Scrive nel file di log
                        char msg[128] = {0};
                        sprintf(msg, "CONNECTION_OPENED: %d\n", fd_c);
                        write(log_file, msg, strlen(msg));
                        
                        // Crea un nuovo client
                        CLIENT client = NULL;
                        if((client = fsp_client_new(fd_c, FSP_CLIENT_DEF_BUF_SIZE)) == NULL) {
                            // Memoria insufficiente
                            close(fd_c);
                            
                            // Scrive nel file di log
                            sprintf(msg, "CONNECTION_CLOSED: %d (internal error)\n", fd_c);
                            write(log_file, msg, strlen(msg));
                            
                            continue;
                        }
                        
                        int serviceNotAvailable = 0;
                        pthread_mutex_lock(&clients_mutex);
                        if(clients->clients_num == config_file.max_conn) {
                            serviceNotAvailable = 1;
                        } else {
                            // Aggiunge il client alla tabella hash
                            fsp_clients_hash_table_insert(clients, client);
                        }
                        pthread_mutex_unlock(&clients_mutex);
                        
                        if(serviceNotAvailable) {
                            // Invia il messaggio di risposta fsp con codice 421
                            sendFspResp(client, 421, "Service not available, closing connection.", 0, NULL);
                            close(fd_c);
                            
                            // Scrive nel file di log
                            sprintf(msg, "CONNECTION_CLOSED: %d (service not available)\n", fd_c);
                            write(log_file, msg, strlen(msg));
                        } else {
                            // Invia il messaggio di risposta fsp con codice 220
                            if(sendFspResp(client, 220, "Service ready.", 0, NULL) != 0) {
                                close(fd_c);
                                
                                // Scrive nel file di log
                                sprintf(msg, "CONNECTION_CLOSED: %d (internal error)\n", fd_c);
                                write(log_file, msg, strlen(msg));
                                
                                continue;
                            }
                            
                            FD_SET(fd_c, &set);
                            if(fd_c > fd_max) fd_max = fd_c;
                        }
                    } else if(fd == pfd[0]) {
                        // Descrittore da aggiungere nuovamente (comunicato da un thread worker)
                        int fd_c;
                        if(read(pfd[0], &fd_c, sizeof(int)) == 0) {
                            // Il descrittore della pipe per la scrittura è stato chiuso
                            // Termina l'esecuzione
                            close(pfd[0]);
                            close(sfd);
                            break_loop = 1;
                            break;
                        } else {
                            FD_SET(fd_c, &set);
                            if(fd_c > fd_max) fd_max = fd_c;
                        }
                    } else {
                        // lettura request fsp
                        pthread_mutex_lock(&clients_mutex);
                        fsp_sfd_queue_enqueue(sfd_queue, fd);
                        pthread_cond_signal(&sfd_queue_isNotEmpty);
                        pthread_mutex_unlock(&clients_mutex);
                        
                        FD_CLR(fd, &set);
                        if(fd == fd_max) {
                            // Determina il nuovo fd_max
                            int _fd = fd;
                            // È sempre presente almeno il file descriptor della pipe
                            while(!FD_ISSET(_fd, &set)) _fd--;
                            fd_max = _fd;
                        }
                    }
                }
            }
        }
    }
    
    // Join sui thread worker
    for(int i = 0; i < config_file.worker_threads_num; i++) {
        pthread_join(threads[i], NULL);
    }
    free(threads);
    printf("Esecuzione dei thread worker terminata.\n");
    
    // Stampa il sunto delle operazioni
    printf("--------------------------------\n");
    printf("Numero di file massimo memorizzato nel server: %d\n", files_max_reached_num);
    printf("Dimensione massima raggiunta dal file storage: %.2f MB\n", (float) storage_max_reached_size/1048576.0);
    printf("Numero di volte in cui la cache è stata rimpiazzata: %d\n", capacity_misses);
    printf("File contenuti nello storage al momento della chiusura del server:\n");
    fsp_files_hash_table_deleteAll(files, printAndRemoveFile);
    fsp_files_hash_table_free(files);
    files = NULL;
    
    freeAll();
    close(log_file);
    
    return 0;
}

static void freeAll() {
    if(files_queue != NULL) fsp_files_queue_free(files_queue);
    if(files != NULL) {
        fsp_files_hash_table_deleteAll(files, removeFile);
        fsp_files_hash_table_free(files);
    }
    if(clients != NULL) {
        fsp_clients_hash_table_deleteAll(clients, removeClient);
        fsp_clients_hash_table_free(clients);
    }
    if(sfd_queue != NULL) fsp_sfd_queue_free(sfd_queue);
}

static void removeFile(FSP_FILE file) {
    if(file != NULL) {
        fsp_file_free(file);
    }
}

static void printAndRemoveFile(FSP_FILE file) {
    if(file != NULL) {
        printf("%s\n", file->pathname);
        fsp_file_free(file);
    }
}

static void removeClient(CLIENT client) {
    if(client != NULL) {
        close(client->sfd);
        fsp_client_free(client);
    }
}

static void closeConnection(CLIENT client, char* error_descr) {
    if(client == NULL) return;
    
    pthread_mutex_lock(&clients_mutex);
    fsp_clients_hash_table_delete(clients, client->sfd);
    close(client->sfd);
    pthread_mutex_unlock(&clients_mutex);
    
    // Chiude i file aperti dal client
    pthread_mutex_lock(&files_mutex);
    while(client->openedFiles != NULL) {
        FSP_FILE opened_file = (client->openedFiles)->file;
        fsp_files_list_remove(&(client->openedFiles), opened_file->pathname);
        if(opened_file->locked == client->sfd) {
            opened_file->locked = -1;
            pthread_cond_broadcast(&lock_cmd_isNotLocked);
        }
        opened_file->links--;
        if(opened_file->links == 0) {
            if(opened_file->data == NULL) {
                fsp_files_queue_remove(files_queue, opened_file->pathname);
                files_num--;
            }
            if(opened_file->remove || opened_file->data == NULL) {
                fsp_files_hash_table_delete(files, opened_file->pathname);
                fsp_file_free(opened_file);
            }
        }
    }
    pthread_mutex_unlock(&files_mutex);
    
    // Scrive nel file di log
    const size_t msg_size = 256;
    char msg[msg_size] = {0};
    if(error_descr != NULL) {
        snprintf(msg, msg_size, "CONNECTION_CLOSED: %d (%s)\n", client->sfd, error_descr);
        write(log_file, msg, strlen(msg));
    } else {
        sprintf(msg, "CONNECTION_CLOSED: %d\n", client->sfd);
        write(log_file, msg, strlen(msg));
    }
    
    fsp_client_free(client);
}

static void signalHandler(int signal) {
    if(signal == SIGINT || signal == SIGQUIT) {
        quit = 1;
    } else if(signal == SIGHUP) {
        accept_connections = 0;
    }
}

static int parseConfigFile() {
    FILE* file = NULL;
    if((file = fopen(CONFIG_FILE, "r")) == NULL) {
        // File non aperto
        return -1;
    }
    
    const size_t buf_size = 256;
    char buf[buf_size];
    long int val;
    
    while(fgets(buf, buf_size, file) != NULL) {
        char* param_start = buf;
        char* end = param_start;
        while(*end != '\0' || *end != '=') end++;
        if(*end == '\0') {
            // Errore di sintassi
            fclose(file);
            return -2;
        }
        *end = '\0';
        end++;
        
        char* val_start = end;
        
        while(*end != '\0' || *end != '\n') end++;
        if(*end == '\0') {
            // Errore di sintassi
            fclose(file);
            return -2;
        }
        *end = '\0';
        
        if(strcmp("SOCKET_FILE_NAME", param_start) == 0) {
            strncpy(config_file.socket_file_name, val_start, UNIX_PATH_MAX);
        } else if(strcmp("LOG_FILE_NAME", param_start) == 0) {
            strncpy(config_file.log_file_name, val_start, UNIX_PATH_MAX);
        } else if(strcmp("FILES_MAX_NUM", param_start) == 0) {
            if(!isNumber(val_start, &val) || val < 0) {
                // Errore di sintassi
                fclose(file);
                return -2;
            }
            config_file.files_max_num = (unsigned int) val;
        } else if(strcmp("STORAGE_MAX_SIZE", param_start) == 0) {
            if(!isNumber(val_start, &val) || val < 0) {
                // Errore di sintassi
                fclose(file);
                return -2;
            }
            config_file.storage_max_size = (unsigned long int) val;
        } else if(strcmp("MAX_CONN", param_start) == 0) {
            if(!isNumber(val_start, &val) || val < 0) {
                // Errore di sintassi
                fclose(file);
                return -2;
            }
            config_file.max_conn = (unsigned int) val;
        } else if(strcmp("WORKER_THREADS_NUM", param_start) == 0) {
            if(!isNumber(val_start, &val) || val < 0) {
                // Errore di sintassi
                fclose(file);
                return -2;
            }
            config_file.worker_threads_num = (unsigned int) val;
        } else {
            // Parametro non riconosciuto
            fclose(file);
            return -2;
        }
    }
    
    fclose(file);
    return 0;
}

static void updateLogFile(int thread_id, CLIENT client, struct fsp_request* req, int resp_code, unsigned long int bytes) {
    if(client == NULL || req == NULL) return;
    
    const size_t msg_size = 512;
    const size_t arg_max_len = 256;
    char msg[msg_size] = {0};
    
    sprintf(msg, "%d: %d", thread_id, client->sfd);
    
    switch(req->cmd) {
        case APPEND:
            strcat(msg, " APPEND ");
            break;
        case CLOSE:
            strcat(msg, " CLOSE ");
            break;
        case LOCK:
            strcat(msg, " LOCK ");
            break;
        case OPEN:
            strcat(msg, " OPEN ");
            break;
        case OPENC:
            strcat(msg, " OPENC ");
            break;
        case OPENCL:
            strcat(msg, " OPENCL ");
            break;
        case OPENL:
            strcat(msg, " OPENL ");
            break;
        case QUIT:
            strcat(msg, " QUIT");
            break;
        case READ:
            strcat(msg, " READ ");
            break;
        case READN:
            strcat(msg, " READN ");
            break;
        case REMOVE:
            strcat(msg, " REMOVE ");
            break;
        case UNLOCK:
            strcat(msg, " UNLOCK ");
            break;
        case WRITE:
            strcat(msg, " WRITE ");
            break;
        default:
            // Mai eseguito
            break;
    }
    
    if(req->cmd != QUIT) {
        strncat(msg, req->arg, arg_max_len);
    }
    
    switch(resp_code) {
        case 200:
        case 220:
        case 221:
            // Success
            if(req->cmd == APPEND || req->cmd == READ || req->cmd == READN || req->cmd == REMOVE || req->cmd == WRITE) {
                sprintf(msg + strlen(msg), " SUCCESS (%lu)\n", bytes);
            } else {
                strcat(msg, " SUCCESS\n");
            }
            break;
        case 421:
            strcat(msg, " FAILURE (service not available)\n");
            break;
        case 501:
            strcat(msg, " FAILURE (syntax error)\n");
            break;
        case 550:
            strcat(msg, " FAILURE (file not found)\n");
            break;
        case 552:
            strcat(msg, " FAILURE (exceeded storage allocation)\n");
            break;
        case 554:
            strcat(msg, " FAILURE (no access)\n");
            break;
        case 555:
            strcat(msg, " FAILURE (file already exists)\n");
            break;
        case 556:
            strcat(msg, " FAILURE (cannot perform the operation)\n");
            break;
        default:
            break;
    }
    
    // Scrive nel file di log
    write(log_file, msg, strlen(msg));
}

static void* worker(void* arg) {
    // thread ID
    int thread_id = (int) *((unsigned long int*) arg);
    // Maschera i segnali
    sigset_t mask;
    sigfillset(&mask);
    if(pthread_sigmask(SIG_SETMASK, &mask, NULL) != 0) {
        fprintf(stderr, "Errore: signal mask del thread %d non modificata.\n", thread_id);
        return 0;
    }
    
    while(1) {
        
        int sfd;
        CLIENT client = NULL;
        
        // Determina il client
        pthread_mutex_lock(&clients_mutex);
        while(fsp_sfd_queue_isEmpty(sfd_queue) && !quit && (accept_connections || clients->clients_num != 0)) {
            pthread_cond_wait(&sfd_queue_isNotEmpty, &clients_mutex);
        }
        if(quit || (!accept_connections && clients->clients_num == 0)) {
            if(active_workers == 1) close(pfd[1]);
            active_workers--;
            pthread_cond_signal(&sfd_queue_isNotEmpty);
            pthread_mutex_unlock(&clients_mutex);
            return 0;
        }
        sfd = fsp_sfd_queue_dequeue(sfd_queue);
        client = fsp_clients_hash_table_search(clients, sfd);
        pthread_mutex_unlock(&clients_mutex);
        
        if(client == NULL) {
            // Client non trovato
            close(sfd);
            
            // Stampa nel file di log
            char msg[64] = {0};
            sprintf(msg, "CONNECTION_CLOSED: %d (client not found)\n", sfd);
            write(log_file, msg, strlen(msg));
            
            continue;
        }
        
        // Il messaggio di risposta
        const size_t descr_max_len = 128;
        char description[descr_max_len];
        struct fsp_response resp = {200, description, 0, NULL};
        
        // Legge il messaggio di richiesta
        struct fsp_request req;
        switch(receiveFspReq(client, &req)) {
            case -1:
                // client->buf == NULL || client->size == NULL
                // client->size > FSP_READER_BUF_MAX_SIZE
            case -2:
                // Errori durante la lettura
                
                // Chiude immediatamente la connessione
                closeConnection(client, "internal error");
                continue;
            case -3:
                // sfd ha raggiunto EOF senza aver letto un messaggio di richiesta
                
                // Chiude immediatamente la connessione
                closeConnection(client, "reached EOF");
                continue;
            case -4:
                // Il messaggio di richiesta contiene errori sintattici
                resp.code = 501;
                strncpy(resp.description, "Syntax error, message unrecognised.", descr_max_len);
                description[descr_max_len-1] = '\0';
                break;
            case -5:
                // Impossibile riallocare il buffer (memoria insufficiente)
                resp.code = 421;
                strncpy(resp.description, "Service not available, closing connection.", descr_max_len);
                description[descr_max_len-1] = '\0';
                break;
            default:
                break;
        }
        
        // Esegue il comando
        // Valore di ritorno delle funzioni che eseguono i comandi
        unsigned long int ret_val = 0;
        if(resp.code != 421 && resp.code != 501) {
            switch(req.cmd) {
                case APPEND:
                    ret_val = append_cmd(client, &req, &resp, descr_max_len);
                    break;
                case CLOSE:
                    ret_val = close_cmd(client, &req, &resp, descr_max_len);
                    break;
                case LOCK:
                    ret_val = lock_cmd(client, &req, &resp, descr_max_len);
                    break;
                case OPEN:
                    ret_val = open_cmd(client, &req, &resp, descr_max_len);
                    break;
                case OPENC:
                    ret_val = openc_cmd(client, &req, &resp, descr_max_len);
                    break;
                case OPENCL:
                    ret_val = opencl_cmd(client, &req, &resp, descr_max_len);
                    break;
                case OPENL:
                    ret_val = openl_cmd(client, &req, &resp, descr_max_len);
                    break;
                case QUIT:
                    resp.code = 221;
                    strncpy(resp.description, "Service closing connection.", descr_max_len);
                    resp.description[descr_max_len-1] = '\0';
                    break;
                case READ:
                    ret_val = read_cmd(client, &req, &resp, descr_max_len);
                    break;
                case READN:
                    ret_val = readn_cmd(client, &req, &resp, descr_max_len);
                    break;
                case REMOVE:
                    ret_val = remove_cmd(client, &req, &resp, descr_max_len);
                    break;
                case UNLOCK:
                    ret_val = unlock_cmd(client, &req, &resp, descr_max_len);
                    break;
                case WRITE:
                    ret_val = write_cmd(client, &req, &resp, descr_max_len);
                    break;
                default:
                    // Mai eseguito
                    break;
            }
            if(ret_val != 0) {
                // Chiude immediatamente la connessione
                closeConnection(client, "internal error");
                continue;
            }
        }
        
        // Scrive nel file di log
        updateLogFile(thread_id, client, &req, resp.code, ret_val);
        
        // Invia il messaggio di risposta
        if(sendFspResp(client, resp.code, resp.description, resp.data_len, resp.data) != 0) {
            // Chiude immediatamente la connessione
            if(resp.data != NULL) free(resp.data);
            closeConnection(client, "internal error");
            continue;
        }
        // Libera il campo data dalla memoria se necessario
        if(resp.data != NULL) {
            free(resp.data);
        }
        
        if(resp.code == 221 || resp.code == 421 || quit) {
            // Chiude la connessione
            closeConnection(client, NULL);
            continue;
        }
        
        // Comunica al master thread il valore sfd (attraverso una pipe senza nome)
        write(pfd[1], &(client->sfd), sizeof(int));
    }
    
    return 0;
}

static int receiveFspReq(CLIENT client, struct fsp_request* req) {
    if(client == NULL || req == NULL) return -1;
    
    int ret_val;
    struct fsp_request _req;
    if ((ret_val = fsp_reader_readRequest(client->sfd, &(client->buf), &(client->size), &_req)) != 0) {
        return ret_val;
    }
    
    *req = _req;
    
    return 0;
}

static int sendFspResp(CLIENT client, int code, const char* description, size_t data_len, void* data) {
    if(client == NULL) return -1;
    
    // Genera il messaggio di risposta
    long int bytes;
    switch(bytes = fsp_parser_makeResponse(&(client->buf), &(client->size), code, description, data_len, data)) {
        case -1:
            // buf == NULL || *buf == NULL || size == NULL || data_len < 0 ||
            // (data_len > 0 && data == NULL) || client->size > FSP_PARSER_BUF_MAX_SIZE
        case -2:
            // Non è stato possibile riallocare la memoria per client->buf
            return -1;
        default:
            // Successo
            break;
    }
    
    // Invia il messaggio di risposta
    char* _buf = (char*) client->buf;
    ssize_t w_bytes;
    while(bytes > 0) {
        if((w_bytes = write(client->sfd, _buf, bytes)) == -1) {
            // Errore durante la scrittura
            return -1;
        } else {
            bytes -= w_bytes;
            _buf += w_bytes;
        }
    }
    
    return 0;
}

static int capacityMiss(void** data, size_t* data_len) {
    if(data == NULL) return -1;
    
    pthread_mutex_lock(&files_mutex);
    
    if(files_num <= config_file.files_max_num && storage_size <= config_file.storage_max_size) {
        pthread_mutex_unlock(&files_mutex);
        return 0;
    }
    
    unsigned long int prev_storage_size = storage_size;
    int n = 0;
    size_t tot_size = 0;
    
    FSP_FILE file = files_queue->head;
    
    // Determina il numero di file da espellere e stima tot_size
    while(files_num > config_file.files_max_num || storage_size > config_file.storage_max_size) {
        if(file == NULL) {
            files_num += n;
            storage_size = prev_storage_size;
            pthread_mutex_unlock(&files_mutex);
            return -1;
        }
        files_num--;
        storage_size -= file->size;
        n++;
        tot_size += file->size + strlen(file->pathname) + 32;
        file = file->queue_next;
    }
    
    // Alloca la memoria per *data
    if((*data = malloc(tot_size)) == NULL) {
        files_num += n;
        storage_size = prev_storage_size;
        pthread_mutex_unlock(&files_mutex);
        return -1;
    }
    
    // Scrive in *data
    long int wrote_bytes_tot = 0;
    long int wrote_bytes = 0;
    file = files_queue->head;
    wrote_bytes = fsp_parser_makeData(*data, &tot_size, 0, n, file->pathname, file->size, file->data);
    if(wrote_bytes < 0) {
        files_num += n;
        storage_size = prev_storage_size;
        free(*data);
        *data = NULL;
        pthread_mutex_unlock(&files_mutex);
        return -1;
    }
    wrote_bytes_tot += wrote_bytes;
    for(int i = 1; i < n; i++) {
        file = file->queue_next;
        wrote_bytes = fsp_parser_makeData(data, &tot_size, wrote_bytes, 0, file->pathname, file->size, file->data);
        if(wrote_bytes < 0) {
            files_num += n;
            storage_size = prev_storage_size;
            free(*data);
            *data = NULL;
            pthread_mutex_unlock(&files_mutex);
            return -1;
        }
        wrote_bytes_tot += wrote_bytes;
    }
    if(data_len != NULL) *data_len = wrote_bytes;
    
    // Messaggio per il file di log
    size_t msg_size = 64 + n*512;
    // VLA
    char msg[msg_size];
    sprintf(msg, "CAPACITY_MISS: %d (%lu)\n", n, prev_storage_size - storage_size);
    // Rimuove i file
    for(int i = 0; i < n; i++) {
        file = fsp_files_queue_dequeue(files_queue);
        
        // Messaggio per il file di log
        snprintf(msg + strlen(msg), 512, "REMOVED_FILE: %s (%lu)\n", file->pathname, file->size);
        
        if(file->links == 0) {
            // Rimuove il file
            fsp_files_hash_table_delete(files, file->pathname);
            fsp_file_free(file);
        } else {
            // File da rimuovere quando verrà chiuso da tutti i client
            file->remove = 1;
            pthread_cond_broadcast(&lock_cmd_isNotLocked);
        }
    }
    
    // Aggiorna la statistica
    capacity_misses++;
    pthread_mutex_unlock(&files_mutex);
    
    // Scrive nel file di log
    write(log_file, msg, strlen(msg));
    
    return 0;
}

static unsigned long int append_cmd(CLIENT client, struct fsp_request* req, struct fsp_response* resp, const size_t descr_max_len) {
    if(client == NULL || req == NULL || resp == NULL) return -1;
    
    unsigned long int bytes = 0;
    resp->data_len = 0;
    resp->data = NULL;
    
    // Dati contenuti nel campo data
    struct fsp_data parsed_data;
    
    // Legge i dati
    switch (fsp_parser_parseData(req->data_len, req->data, &parsed_data)) {
        case -1:
            // data_len <= 0 || data == NULL || parsed_data == NULL
        case -2:
            // Il campo data del messaggio è incompleto
        case -3:
            // Il campo data del messaggio contiene errori sintattici
            resp->code = 501;
            strncpy(resp->description, "Syntax error, message unrecognised.", descr_max_len);
            resp->description[descr_max_len-1] = '\0';
            return 0;
        case -4:
            // Impossibile allocare memoria nello heap
            return -1;
        default:
            // Successo
            break;
    }
    if(parsed_data.n != 1) {
        fsp_parser_freeData(&parsed_data);
        resp->code = 501;
        strncpy(resp->description, "Syntax error, message unrecognised.", descr_max_len);
        resp->description[descr_max_len-1] = '\0';
        return 0;
    }
    bytes = parsed_data.sizes[0];
    
    FSP_FILE file;
    int notOpened = 0;
    int notLocked = 0;
    int noMemory = 0;
    
    pthread_mutex_lock(&files_mutex);
    // Cerca il file
    file = fsp_files_hash_table_search(files, req->arg);
    // Se il file esiste ma deve essere rimosso, allora non esegue il comando
    if(file != NULL && file->remove) file = NULL;
    if(file != NULL) {
        if(file->size + parsed_data.sizes[0] <= config_file.storage_max_size) {
            if(fsp_files_list_contains(client->openedFiles, req->arg) && file->data != NULL) {
                // Il file è stato aperto dal client senza flag O_CREATE
                if(file->locked >= 0 && file->locked == client->sfd) {
                    // Il client detiene la lock sul file
                    void* _data = file->data;
                    if((file->data = realloc(file->data, file->size + parsed_data.sizes[0])) == NULL) {
                        file->data = _data;
                        fsp_parser_freeData(&parsed_data);
                        pthread_mutex_unlock(&files_mutex);
                        return -1;
                    }
                    memcpy((file->data) + file->size, parsed_data.data[0], parsed_data.sizes[0]);
                    file->size += parsed_data.sizes[0];
                    unsigned long int prev_storage_size = storage_size;
                    storage_size += parsed_data.sizes[0];
                    
                    // Espelle i file dalla memoria se necessario
                    if(storage_size > config_file.storage_max_size) {
                        if(capacityMiss(&(resp->data), &(resp->data_len)) != 0) {
                            _data = file->data;
                            if((file->data = realloc(file->data, file->size - parsed_data.sizes[0])) == NULL) {
                                file->data = _data;
                            }
                            file->size -= parsed_data.sizes[0];
                            fsp_parser_freeData(&parsed_data);
                            storage_size = prev_storage_size;
                            pthread_mutex_unlock(&files_mutex);
                            return -1;
                        }
                    }
                    
                    // Aggiorna la statistica
                    if(storage_size > storage_max_reached_size) storage_max_reached_size = storage_size;
                } else  {
                    notLocked = 1;
                }
            } else {
                notOpened = 1;
            }
        } else {
            noMemory = 1;
        }
    }
    pthread_mutex_unlock(&files_mutex);
    
    fsp_parser_freeData(&parsed_data);
    
    if(file == NULL) {
        // File inesistente o file da rimuovere
        resp->code = 550;
        strncpy(resp->description, "Requested action not taken. File not found.", descr_max_len);
        resp->description[descr_max_len-1] = '\0';
        return 0;
    }
    if(noMemory) {
        fsp_parser_freeData(&parsed_data);
        resp->code = 552;
        strncpy(resp->description, "Requested file action aborted. Exceeded storage allocation.", descr_max_len);
        resp->description[descr_max_len-1] = '\0';
        return 0;
    }
    if(notOpened) {
        // Il file non è stato aperto oppure non è stato aperto in modo corretto (con flag O_CREATE)
        resp->code = 556;
        strncpy(resp->description, "Cannot perform the operation.", descr_max_len);
        resp->description[descr_max_len-1] = '\0';
        return 0;
    }
    if(notLocked) {
        // Il client non detiene la lock sul file
        resp->code = 554;
        strncpy(resp->description, "Requested action not taken. No access.", descr_max_len);
        resp->description[descr_max_len-1] = '\0';
        return 0;
    }
    
    resp->code = 200;
    strncpy(resp->description, "The requested action has been successfully completed.", descr_max_len);
    resp->description[descr_max_len-1] = '\0';
    return bytes;
}

static int close_cmd(CLIENT client, struct fsp_request* req, struct fsp_response* resp, const size_t descr_max_len) {
    if(client == NULL || req == NULL || resp == NULL) return -1;
    
    resp->data_len = 0;
    resp->data = NULL;
    
    struct fsp_file* file;
    int notOpened = 0;
    
    pthread_mutex_lock(&files_mutex);
    // Cerca il file
    file = fsp_files_hash_table_search(files, req->arg);
    if(file != NULL) {
        if(fsp_files_list_contains(client->openedFiles, req->arg)) {
            // Rimuove il file dalla lista dei file aperti del client
            fsp_files_list_remove(&(client->openedFiles), req->arg);
            // Rimuove la lock se la detiene
            if(file->locked == client->sfd) {
                file->locked = -1;
                pthread_cond_broadcast(&lock_cmd_isNotLocked);
            }
            // Rimuove il link dal file
            file->links--;
            // Rimuove il file se links == 0
            if(file->links == 0) {
                if(file->data == NULL) {
                    fsp_files_queue_remove(files_queue, req->arg);
                    files_num--;
                }
                if(file->remove || file->data == NULL) {
                    fsp_files_hash_table_delete(files, req->arg);
                    fsp_file_free(file);
                }
            }
        } else {
            notOpened = 1;
        }
    }
    pthread_mutex_unlock(&files_mutex);
    
    if(file == NULL) {
        // File non trovato
        resp->code = 550;
        strncpy(resp->description, "Requested action not taken. File not found.", descr_max_len);
        resp->description[descr_max_len-1] = '\0';
        return 0;
    }
    if(notOpened) {
        // File non aperto dal client
        resp->code = 556;
        strncpy(resp->description, "Cannot perform the operation.", descr_max_len);
        resp->description[descr_max_len-1] = '\0';
        return 0;
    }
    
    resp->code = 200;
    strncpy(resp->description, "The requested action has been successfully completed.", descr_max_len);
    resp->description[descr_max_len-1] = '\0';
    return 0;
}

static int lock_cmd(CLIENT client, struct fsp_request* req, struct fsp_response* resp, const size_t descr_max_len) {
    if(client == NULL || req == NULL || resp == NULL) return -1;
    
    resp->data_len = 0;
    resp->data = NULL;
    
    FSP_FILE file;
    int notOpened = 0;
    
    pthread_mutex_lock(&files_mutex);
    // Cerca il file
    file = fsp_files_hash_table_search(files, req->arg);
    // Se il file esiste ma deve essere rimosso, allora non esegue il comando
    if(file != NULL && file->remove) file = NULL;
    if(file != NULL) {
        // File trovato
        if(!fsp_files_list_contains(client->openedFiles, req->arg)) {
            // Il client non ha aperto il file
            notOpened = 1;
        } else if(file->locked < 0 || file->locked == client->sfd) {
            // Setta la lock
            file->locked = client->sfd;
        } else {
            // Attende di ottenere la lock su una variabile di condizione
            while(file->locked >= 0 && !file->remove) {
                pthread_cond_wait(&lock_cmd_isNotLocked, &files_mutex);
            }
            if(file->remove) {
                file = NULL;
            } else {
                file->locked = client->sfd;
            }
        }
    }
    pthread_mutex_unlock(&files_mutex);
    
    if(file == NULL) {
        // File non trovato
        resp->code = 550;
        strncpy(resp->description, "Requested action not taken. File not found.", descr_max_len);
        resp->description[descr_max_len-1] = '\0';
        return 0;
    }
    if(notOpened) {
        // File non aperto dal client
        resp->code = 556;
        strncpy(resp->description, "Cannot perform the operation.", descr_max_len);
        resp->description[descr_max_len-1] = '\0';
        return 0;
    }
    
    resp->code = 200;
    strncpy(resp->description, "The requested action has been successfully completed.", descr_max_len);
    resp->description[descr_max_len-1] = '\0';
    
    return 0;
}

static int open_cmd(CLIENT client, struct fsp_request* req, struct fsp_response* resp, const size_t descr_max_len) {
    if(client == NULL || req == NULL || resp == NULL) return -1;
    
    resp->data_len = 0;
    resp->data = NULL;
    
    FSP_FILE file;
    
    pthread_mutex_lock(&files_mutex);
    // Cerca il file
    file = fsp_files_hash_table_search(files, req->arg);
    // Se il file esiste ma deve essere rimosso, allora non esegue il comando
    if(file != NULL && file->remove) file = NULL;
    if(file != NULL) {
        if(!fsp_files_list_contains(client->openedFiles, req->arg)) {
            // File non ancora aperto dal client
            // Aggiunge un nuovo collegamento al file
            file->links++;
            // Aggiunge il file nella lista dei file aperti dal client
            fsp_files_list_add(&(client->openedFiles), file);
        }
        // Se il file era già aperto dal client, allora non fa niente
    }
    pthread_mutex_unlock(&files_mutex);
    
    if(file == NULL) {
        // File non trovato
        resp->code = 550;
        strncpy(resp->description, "Requested action not taken. File not found.", descr_max_len);
        resp->description[descr_max_len-1] = '\0';
        return 0;
    }
    
    resp->code = 200;
    strncpy(resp->description, "The requested action has been successfully completed.", descr_max_len);
    resp->description[descr_max_len-1] = '\0';
    return 0;
}

static int openc_cmd(CLIENT client, struct fsp_request* req, struct fsp_response* resp, const size_t descr_max_len) {
    if(client == NULL || req == NULL || resp == NULL) return -1;
    
    resp->data_len = 0;
    resp->data = NULL;
    
    if(config_file.files_max_num == 0) {
        resp->code = 552;
        strncpy(resp->description, "Requested file action aborted. Exceeded storage allocation.", descr_max_len);
        resp->description[descr_max_len-1] = '\0';
        return 0;
    }
    
    int alreadyExists = 0;
    int notRemoved = 0;
    
    FSP_FILE file;
    
    pthread_mutex_lock(&files_mutex);
    // Controlla se il file esiste
    file = fsp_files_hash_table_search(files, req->arg);
    if(file == NULL) {
        // Il file non esiste
        // Crea il file
        if((file = fsp_file_new(req->arg, NULL, 0, 1, 0, 0)) == NULL) {
            pthread_mutex_unlock(&files_mutex);
            return -1;
        }
        files_num++;
        
        // Espelle un file dalla memoria se necessario
        if(files_num > config_file.files_max_num) {
            if(capacityMiss(&(resp->data), &(resp->data_len)) != 0) {
                fsp_file_free(file);
                files_num--;
                pthread_mutex_unlock(&files_mutex);
                return -1;
            }
        }
        
        // Aggiunge il file alla tabella hash, alla coda e alla lista dei file aperti dal client
        fsp_files_hash_table_insert(files, file);
        fsp_files_queue_enqueue(files_queue, file);
        fsp_files_list_add(&(client->openedFiles), file);
        
        // Aggiorna la statistica
        if(files_num > files_max_reached_num) files_max_reached_num = files_num;
    } else {
        if(file->remove) {
            notRemoved = 1;
        } else {
            alreadyExists = 1;
        }
    }
    pthread_mutex_unlock(&files_mutex);
    
    if(alreadyExists) {
        // File già esistente
        resp->code = 555;
        strncpy(resp->description, "Requested action not taken. File already exists.", descr_max_len);
        resp->description[descr_max_len-1] = '\0';
        return 0;
    }
    if(notRemoved) {
        // File non ancora rimosso (attende di essere chiuso da tutti i client)
        resp->code = 556;
        strncpy(resp->description, "Cannot perform the operation.", descr_max_len);
        resp->description[descr_max_len-1] = '\0';
        return 0;
    }
    
    resp->code = 200;
    strncpy(resp->description, "The requested action has been successfully completed.", descr_max_len);
    resp->description[descr_max_len-1] = '\0';
    return 0;
}

static int opencl_cmd(CLIENT client, struct fsp_request* req, struct fsp_response* resp, const size_t descr_max_len) {
    if(client == NULL || req == NULL || resp == NULL) return -1;
    
    resp->data_len = 0;
    resp->data = NULL;
    
    if(config_file.files_max_num == 0) {
        resp->code = 552;
        strncpy(resp->description, "Requested file action aborted. Exceeded storage allocation.", descr_max_len);
        resp->description[descr_max_len-1] = '\0';
        return 0;
    }
    
    int alreadyExists = 0;
    int notRemoved = 0;
    
    FSP_FILE file;
    
    pthread_mutex_lock(&files_mutex);
    // Controlla se il file esiste
    file = fsp_files_hash_table_search(files, req->arg);
    if(file == NULL) {
        // Il file non esiste
        // Crea il file
        if((file = fsp_file_new(req->arg, NULL, 0, 1, client->sfd, 0)) == NULL) {
            pthread_mutex_unlock(&files_mutex);
            return -1;
        }
        files_num++;
        
        // Espelle un file dalla memoria se necessario
        if(files_num > config_file.files_max_num) {
            if(capacityMiss(&(resp->data), &(resp->data_len)) != 0) {
                fsp_file_free(file);
                files_num--;
                pthread_mutex_unlock(&files_mutex);
                return -1;
            }
        }
        
        // Aggiunge il file alla tabella hash, alla coda e alla lista dei file aperti dal client
        fsp_files_hash_table_insert(files, file);
        fsp_files_queue_enqueue(files_queue, file);
        fsp_files_list_add(&(client->openedFiles), file);
        
        // Aggiorna la statistica
        if(files_num > files_max_reached_num) files_max_reached_num = files_num;
    } else {
        if(file->remove) {
            notRemoved = 1;
        } else {
            alreadyExists = 1;
        }
    }
    pthread_mutex_unlock(&files_mutex);
    
    if(alreadyExists) {
        // File già esistente
        resp->code = 555;
        strncpy(resp->description, "Requested action not taken. File already exists.", descr_max_len);
        resp->description[descr_max_len-1] = '\0';
        return 0;
    }
    if(notRemoved) {
        // File non ancora rimosso (attende di essere chiuso da tutti i client)
        resp->code = 556;
        strncpy(resp->description, "Cannot perform the operation.", descr_max_len);
        resp->description[descr_max_len-1] = '\0';
        return 0;
    }
    
    resp->code = 200;
    strncpy(resp->description, "The requested action has been successfully completed.", descr_max_len);
    resp->description[descr_max_len-1] = '\0';
    return 0;
}

static int openl_cmd(CLIENT client, struct fsp_request* req, struct fsp_response* resp, const size_t descr_max_len) {
    if(client == NULL || req == NULL || resp == NULL) return -1;
    
    resp->data_len = 0;
    resp->data = NULL;
    
    int locked = 0;
    
    FSP_FILE file;
    
    pthread_mutex_lock(&files_mutex);
    // Cerca il file
    file = fsp_files_hash_table_search(files, req->arg);
    // Se il file esiste ma deve essere rimosso, allora non esegue il comando
    if(file != NULL && file->remove) file = NULL;
    if(file != NULL) {
        if(file->locked < 0) {
            // Nessuno detiene la lock sul file
            if(!fsp_files_list_contains(client->openedFiles, req->arg)) {
                // File non ancora aperto dal client
                // Aggiunge un nuovo collegamento al file
                file->links++;
                // Setta la lock al file
                file->locked = client->sfd;
                // Aggiunge il file nella lista dei file aperti dal client
                fsp_files_list_add(&(client->openedFiles), file);
            } else {
                // File già aperto dal client
                // Setta la lock al file
                file->locked = client->sfd;
            }
        } else if(file->locked == client->sfd) {
            // Il file è già aperto dal client e il client detiene già la lock sul file
        } else {
            locked = 1;
        }
    }
    pthread_mutex_unlock(&files_mutex);
    
    if(file == NULL) {
        // File non trovato
        resp->code = 550;
        strncpy(resp->description, "Requested action not taken. File not found.", descr_max_len);
        resp->description[descr_max_len-1] = '\0';
        return 0;
    }
    if(locked) {
        // Un altro client ha settato la lock sul file
        resp->code = 554;
        strncpy(resp->description, "Requested action not taken. No access.", descr_max_len);
        resp->description[descr_max_len-1] = '\0';
        return 0;
    }
    
    resp->code = 200;
    strncpy(resp->description, "The requested action has been successfully completed.", descr_max_len);
    resp->description[descr_max_len-1] = '\0';
    return 0;
}

static unsigned long int read_cmd(CLIENT client, struct fsp_request* req, struct fsp_response* resp, const size_t descr_max_len) {
    if(client == NULL || req == NULL || resp == NULL) return -1;
    
    unsigned long int bytes = 0;
    resp->data_len = 0;
    resp->data = NULL;
    
    FSP_FILE file;
    int notOpened = 0;
    int locked = 0;
    
    pthread_mutex_lock(&files_mutex);
    // Cerca il file
    file = fsp_files_hash_table_search(files, req->arg);
    // Se il file esiste ma deve essere rimosso, allora non esegue il comando
    if(file != NULL && file->remove) file = NULL;
    if(file != NULL) {
        if(fsp_files_list_contains(client->openedFiles, req->arg)) {
            // Il file è stato aperto dal client
            if(file->locked < 0 || (file->locked >= 0 && file->locked == client->sfd)) {
                // Il file si può leggere
                size_t buf_size = file->size + strlen(file->pathname) + 32;
                if((resp->data = malloc(buf_size)) == NULL) {
                    pthread_mutex_unlock(&files_mutex);
                    return -1;
                }
                long int wrote_bytes = 0;
                wrote_bytes = fsp_parser_makeData(resp->data, &buf_size, 0, 1, file->pathname, file->size, file->data);
                if(wrote_bytes < 0) {
                    free(resp->data);
                    pthread_mutex_unlock(&files_mutex);
                    return -1;
                }
                resp->data_len = wrote_bytes;
                bytes = file->size;
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
        resp->code = 550;
        strncpy(resp->description, "Requested action not taken. File not found.", descr_max_len);
        resp->description[descr_max_len-1] = '\0';
        return 0;
    }
    if(notOpened) {
        // File non aperto dal client
        resp->code = 556;
        strncpy(resp->description, "Cannot perform the operation.", descr_max_len);
        resp->description[descr_max_len-1] = '\0';
        return 0;
    }
    if(locked) {
        // Un altro client ha settato la lock sul file
        resp->code = 554;
        strncpy(resp->description, "Requested action not taken. No access.", descr_max_len);
        resp->description[descr_max_len-1] = '\0';
        return 0;
    }
    
    resp->code = 200;
    strncpy(resp->description, "The requested action has been successfully completed.", descr_max_len);
    resp->description[descr_max_len-1] = '\0';
    return bytes;
}

static unsigned long int readn_cmd(CLIENT client, struct fsp_request* req, struct fsp_response* resp, const size_t descr_max_len) {
    if(client == NULL || req == NULL || resp == NULL) return -1;
    
    unsigned long int bytes = 0;
    resp->data_len = 0;
    resp->data = NULL;
    
    long int n;
    if(!isNumber(req->arg, &n)) {
        resp->code = 501;
        strncpy(resp->description, "Syntax error, message unrecognised.", descr_max_len);
        resp->description[descr_max_len-1] = '\0';
        return 0;
    }
    
    pthread_mutex_lock(&files_mutex);
    unsigned int availableFiles = 0;
    size_t buf_size = 0;
    
    // Calcola il numero di file disponibili
    struct fsp_files_hash_table_iterator* iterator = fsp_files_hash_table_getIterator(files);
    FSP_FILE file = fsp_files_hash_table_getNext(iterator);
    
    while((file = fsp_files_hash_table_getNext(iterator)) != NULL || n > 0) {
        // Non legge i file da rimuovere e i file in stato locked di cui il client non detiene la lock
        if(file->remove || (file->locked >=0 && file->locked != client->sfd)) {
            continue;
        }
        availableFiles++;
        buf_size += file->size;
        if(n > 0) n--;
    }
    free(iterator);
    bytes = buf_size;
    
    if(availableFiles > 0) {
        
        // Stima la dimensione del buffer e alloca la memoria
        buf_size += availableFiles*(256 + 32);
        if((resp->data = malloc(buf_size)) == NULL) {
            pthread_mutex_unlock(&files_mutex);
            return -1;
        }
        
        // Prepara l'iteratore
        iterator = fsp_files_hash_table_getIterator(files);
        FSP_FILE file = fsp_files_hash_table_getNext(iterator);
        
        // Non legge un file da rimuovere o un file in stato locked di cui il client non detiene la lock
        while(file->remove || (file->locked >=0 && file->locked != client->sfd)) {
            if((file = fsp_files_hash_table_getNext(iterator)) == NULL) break;
        }
        
        // Legge i file
        long int wrote_bytes = 0;
        long int wrote_bytes_tot = 0;
        
        wrote_bytes_tot = fsp_parser_makeData(resp->data, &buf_size, 0, availableFiles, file->pathname, file->size, file->data);
        if(wrote_bytes_tot < 0) {
            free(resp->data);
            resp->data = NULL;
            free(iterator);
            pthread_mutex_unlock(&files_mutex);
            return -1;
        }
        
        while((file = fsp_files_hash_table_getNext(iterator)) != NULL || n > 0) {
            // Non legge i file da rimuovere e i file in stato locked di cui il client non detiene la lock
            if(file->remove || (file->locked >=0 && file->locked != client->sfd)) continue;
            
            wrote_bytes = fsp_parser_makeData(resp->data, &buf_size, wrote_bytes_tot, 0, file->pathname, file->size, file->data);
            if(wrote_bytes < 0) {
                free(resp->data);
                resp->data = NULL;
                free(iterator);
                pthread_mutex_unlock(&files_mutex);
                return -1;
            }
            wrote_bytes_tot += wrote_bytes;
            if(n > 0) n--;
        }
        
        resp->data_len = wrote_bytes_tot;
        free(iterator);
    }
    pthread_mutex_unlock(&files_mutex);
    
    resp->code = 200;
    strncpy(resp->description, "The requested action has been successfully completed.", descr_max_len);
    resp->description[descr_max_len-1] = '\0';
    return bytes;
}

static unsigned long int remove_cmd(CLIENT client, struct fsp_request* req, struct fsp_response* resp, const size_t descr_max_len) {
    if(client == NULL || req == NULL || resp == NULL) return -1;
    
    unsigned long int bytes = 0;
    resp->data_len = 0;
    resp->data = NULL;
    
    struct fsp_file* file;
    int notOpened = 0;
    int notLocked = 0;
    
    pthread_mutex_lock(&files_mutex);
    // Cerca il file
    file = fsp_files_hash_table_search(files, req->arg);
    if(file != NULL && file->remove) file = NULL;
    if(file != NULL) {
        if(fsp_files_list_contains(client->openedFiles, req->arg)) {
            if(file->locked == client->sfd) {
                // Il file viene rimosso dal server solo quando file->links == 0
                file->remove = 1;
                fsp_files_queue_remove(files_queue, req->arg);
                files_num--;
                storage_size -= file->size;
                bytes = file->size;
                pthread_cond_broadcast(&lock_cmd_isNotLocked);
            } else {
                notLocked = 1;
            }
        } else {
            notOpened = 1;
        }
    }
    pthread_mutex_unlock(&files_mutex);
    
    if(file == NULL) {
        // File non trovato o già da rimuovere
        resp->code = 550;
        strncpy(resp->description, "Requested action not taken. File not found.", descr_max_len);
        resp->description[descr_max_len-1] = '\0';
        return 0;
    }
    if(notOpened) {
        // Il client non ha aperto il file
        resp->code = 556;
        strncpy(resp->description, "Cannot perform the operation.", descr_max_len);
        resp->description[descr_max_len-1] = '\0';
        return 0;
    }
    if(notLocked) {
        // Il client non detiene la lock sul file
        resp->code = 554;
        strncpy(resp->description, "Requested action not taken. No access.", descr_max_len);
        resp->description[descr_max_len-1] = '\0';
        return 0;
    }
    
    resp->code = 200;
    strncpy(resp->description, "The requested action has been successfully completed.", descr_max_len);
    resp->description[descr_max_len-1] = '\0';
    return bytes;
}

static int unlock_cmd(CLIENT client, struct fsp_request* req, struct fsp_response* resp, const size_t descr_max_len) {
    if(client == NULL || req == NULL || resp == NULL) return -1;
    
    resp->data_len = 0;
    resp->data = NULL;
    
    FSP_FILE file;
    int notOpened = 0;
    int notLocked = 0;
    
    pthread_mutex_lock(&files_mutex);
    // Cerca il file
    file = fsp_files_hash_table_search(files, req->arg);
    // Se il file esiste ma deve essere rimosso, allora non esegue il comando
    if(file != NULL && file->remove) file = NULL;
    if(file != NULL) {
        if(!fsp_files_list_contains(client->openedFiles, req->arg)) {
            // Il file non è stato aperto dal client
            notOpened = 1;
        } else if(file->locked >= 0 && file->locked != client->sfd) {
            // Il file ha la lock settata da un altro client
            notLocked = 1;
        } else if(file->locked >= 0){
            // Rilascia la lock
            file->locked = -1;
            pthread_cond_broadcast(&lock_cmd_isNotLocked);
        }
    }
    pthread_mutex_unlock(&files_mutex);
    
    if(file == NULL) {
        // File non trovato o file da rimuovere
        resp->code = 550;
        strncpy(resp->description, "Requested action not taken. File not found.", descr_max_len);
        resp->description[descr_max_len-1] = '\0';
        return 0;
    }
    if(notOpened) {
        // Il client non ha aperto il file
        resp->code = 556;
        strncpy(resp->description, "Cannot perform the operation.", descr_max_len);
        resp->description[descr_max_len-1] = '\0';
        return 0;
    }
    if(notLocked) {
        // Il client non detiene la lock sul file
        resp->code = 554;
        strncpy(resp->description, "Requested action not taken. No access.", descr_max_len);
        resp->description[descr_max_len-1] = '\0';
        return 0;
    }
    
    resp->code = 200;
    strncpy(resp->description, "The requested action has been successfully completed.", descr_max_len);
    resp->description[descr_max_len-1] = '\0';
    return 0;
}

static unsigned long int write_cmd(CLIENT client, struct fsp_request* req, struct fsp_response* resp, const size_t descr_max_len) {
    if(client == NULL || req == NULL || resp == NULL) return -1;
    
    unsigned long int bytes = 0;
    resp->data_len = 0;
    resp->data = NULL;
    
    // Dati contenuti nel campo data
    struct fsp_data parsed_data;
    
    // Legge i dati
    switch (fsp_parser_parseData(req->data_len, req->data, &parsed_data)) {
        case -1:
            // data_len <= 0 || data == NULL || parsed_data == NULL
        case -2:
            // Il campo data del messaggio è incompleto
        case -3:
            // Il campo data del messaggio contiene errori sintattici
            resp->code = 501;
            strncpy(resp->description, "Syntax error, message unrecognised.", descr_max_len);
            resp->description[descr_max_len-1] = '\0';
            return 0;
        case -4:
            // Impossibile allocare memoria nello heap
            return -1;
        default:
            // Successo
            break;
    }
    if(parsed_data.n != 1) {
        fsp_parser_freeData(&parsed_data);
        resp->code = 501;
        strncpy(resp->description, "Syntax error, message unrecognised.", descr_max_len);
        resp->description[descr_max_len-1] = '\0';
        return 0;
    }
    if(parsed_data.sizes[0] > config_file.storage_max_size) {
        fsp_parser_freeData(&parsed_data);
        resp->code = 552;
        strncpy(resp->description, "Requested file action aborted. Exceeded storage allocation.", descr_max_len);
        resp->description[descr_max_len-1] = '\0';
        return 0;
    }
    bytes = parsed_data.sizes[0];
    
    FSP_FILE file;
    int notOpened = 0;
    int notLocked = 0;
    
    pthread_mutex_lock(&files_mutex);
    // Cerca il file
    file = fsp_files_hash_table_search(files, req->arg);
    // Se il file esiste ma deve essere rimosso, allora non esegue il comando
    if(file != NULL && file->remove) file = NULL;
    if(file != NULL) {
        if(fsp_files_list_contains(client->openedFiles, req->arg) && file->data == NULL) {
            // Il file è stato aperto dal client con flag O_CREATE
            if(file->locked >= 0 && file->locked == client->sfd) {
                // Il client detiene la lock sul file
                if((file->data = malloc(parsed_data.sizes[0])) == NULL) {
                    fsp_parser_freeData(&parsed_data);
                    pthread_mutex_unlock(&files_mutex);
                    return -1;
                }
                memcpy(file->data, parsed_data.data[0], parsed_data.sizes[0]);
                file->size = parsed_data.sizes[0];
                unsigned long int prev_storage_size = storage_size;
                storage_size += parsed_data.sizes[0];
                
                // Espelle i file dalla memoria se necessario
                if(storage_size > config_file.storage_max_size) {
                    if(capacityMiss(&(resp->data), &(resp->data_len)) != 0) {
                        fsp_parser_freeData(&parsed_data);
                        free(file->data);
                        file->data = NULL;
                        file->size = 0;
                        storage_size = prev_storage_size;
                        pthread_mutex_unlock(&files_mutex);
                        return -1;
                    }
                }
                
                // Aggiorna la statistica
                if(storage_size > storage_max_reached_size) storage_max_reached_size = storage_size;
            } else  {
                notLocked = 1;
            }
        } else {
            notOpened = 1;
        }
    }
    pthread_mutex_unlock(&files_mutex);
    
    fsp_parser_freeData(&parsed_data);
    
    if(file == NULL) {
        // File inesistente o file da rimuovere
        resp->code = 550;
        strncpy(resp->description, "Requested action not taken. File not found.", descr_max_len);
        resp->description[descr_max_len-1] = '\0';
        return 0;
    }
    if(notOpened) {
        // Il file non è stato aperto oppure non è stato aperto in modo corretto (senza flag O_CREATE)
        resp->code = 556;
        strncpy(resp->description, "Cannot perform the operation.", descr_max_len);
        resp->description[descr_max_len-1] = '\0';
        return 0;
    }
    if(notLocked) {
        // Il client non detiene la lock sul file
        resp->code = 554;
        strncpy(resp->description, "Requested action not taken. No access.", descr_max_len);
        resp->description[descr_max_len-1] = '\0';
        return 0;
    }
    
    resp->code = 200;
    strncpy(resp->description, "The requested action has been successfully completed.", descr_max_len);
    resp->description[descr_max_len-1] = '\0';
    return bytes;
}
