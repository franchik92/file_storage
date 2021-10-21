#include <fsp_api.h>
#include <fsp_client_request_queue.h>
#include <fsp_opened_files_bst.h>
#include <utils.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <limits.h>
#include <assert.h>

#define FSP_CLIENT_FILE_MAX_LEN 256

// Ogni richiesta Request viene inserita in una coda RequestQueue
typedef struct fsp_client_request Request;
typedef struct fsp_client_request_queue RequestQueue;

// Tiene traccia dei file aperti in un albero binario di ricerca
typedef struct opened_file OpenedFiles;

// Funzioni che eseguono le richieste del client
static void printUsage(void);
// Restituisce -1 se non è stato possibile aggiungere un file appena aperto nell'albero binario di ricerca,
// -2 se non riesce a tornare alla working directory precedente alla sua chiamata,
// 0 altrimenti.
static int write_opt(char* arg, char* dirname, unsigned int time, int p, OpenedFiles** files);
// Restituisce -1 se non è stato possibile aggiungere un file appena aperto nell'albero binario di ricerca,
// 0 altrimenti.
static int write_opt_rec(long int* n, char* dirname, unsigned int time, int p, OpenedFiles** files);
// Restituisce -1 se non è stato possibile aggiungere il file appena aperto nell'albero binario di ricerca,
// 0 altrimenti.
static int Write_opt(char* arg, char* dirname, unsigned int time, int p, OpenedFiles** files);
// Restituisce -1 se non è stato possibile aggiungere il file appena aperto nell'albero binario di ricerca,
// -2 se non è stato possibile allocare memoria,
// 0 altrimenti.
static int read_opt(char* arg, char* dirname, unsigned int time, int p, OpenedFiles** files);
static void Read_opt(char* arg, char* dirname, unsigned int time, int p);
// Restituisce -1 se non è stato possibile aggiungere il file appena aperto nell'albero binario di ricerca,
// 0 altrimenti.
static int lock_opt(char* arg, unsigned int time, int p, OpenedFiles** files);
static void unlock_opt(char* arg, unsigned int time, int p);
// Restituisce -1 se non è stato possibile aggiungere il file appena aperto nell'albero binario di ricerca,
// 0 altrimenti.
static int cancel_opt(char* arg, unsigned int time, int p, OpenedFiles** files);

// Funzioni per l'apertura e la chiusura dei file.
// Viene fatta richiesta automatica di apertura di un file
// - prima di una operazione di scrittura (-w, -W) con flags O_CREATE | O_LOCK,
// - prima di una operazione di lettura (-r) con flag O_DEFAULT se il file non è stato precedentemente aperto,
// - al posto dell'operazione di lock (-l) con flag O_LOCK se il file non è stato precedentemente aperto,
// - prima di una operazione di cancellazione (-c) con flag O_LOCK se il file non è già aperto con flag O_LOCK.
// Viene fatta richiesta automatica di chiusura di un file
// - prima di una operazione di scrittura se il file è aperto,
// - al termine di una operazione di scrittura,
// - prima di una operazione di cancellazione se il file è aperto, ma non ha il flag O_LOCK settato,
// - al termine di una operazione di cancellazione.
// Al termine dell'esecuzione del programma viene fatta richiesta di chiusura di tutti i file rimasti aperti.

// Restituisce 0 in caso di successo,
//             -1 se non riesce ad aprire il file
// (pathname è il path assoluto del file da aprire)
static int open_file(char* pathname, int flags);
static void close_file(const char* pathname);

int main(int argc, char* argv[]) {
    char* socket_filename = NULL;
    int p = 0;
    RequestQueue queue = {NULL, NULL};
    Request* req = NULL;
    OpenedFiles* files = NULL;
    
    if(argc == 2 && strcmp(argv[1], "-h") == 0) {
        printUsage();
        return 0;
    } else if(argc >= 3 && strcmp(argv[1], "-f") == 0) {
        socket_filename = argv[2];
        optind = 3;
        if(argc >= 4 && strcmp(argv[3], "-p") == 0) {
            p = 1;
            optind = 4;
        }
    } else {
        // Errore: filename non specificato
        fprintf(stderr, "Errore: specificare il nome del socket AF_UNIX a cui connettersi con l'opzione -f\n");
        return -1;
    }
    
    char c;
    while((c = getopt(argc, argv, ":w:W:D:r:R:d:t:l:u:c:")) != -1) {
        switch(c) {
            case 'w':
            case 'W':
            case 'r':
            case 'R':
            case 'l':
            case 'u':
            case 'c':
                if((req = fsp_client_request_queue_newRequest(optopt, optarg)) == NULL) {
                    // Errore: memoria insufficiente
                    fprintf(stderr, "Errore: memoria insufficiente.\n");
                    fsp_client_request_queue_freeAllRequests(&queue);
                    return -1;
                }
                fsp_client_request_queue_enqueue(&queue, req);
                break;
            case 'D':
                // Controlla se l'opzione -D è eseguita congiuntamente a -w o -W
                if(queue.tail == NULL || queue.tail->opt != 'w' || queue.tail->opt != 'W') {
                    fprintf(stderr, "Errore: l'opzione -D può essere usata solo congiuntamente alle opzioni -w o -W.\n");
                    fsp_client_request_queue_freeAllRequests(&queue);
                    return -1;
                }
                queue.tail->dirname = optarg;
                break;
            case 'd':
                // Controlla se l'opzione -d è eseguita congiuntamente a -r o -R
                if(queue.tail == NULL || queue.tail->opt != 'r' || queue.tail->opt != 'R') {
                    fprintf(stderr, "Errore: l'opzione -d può essere usata solo congiuntamente alle opzioni -r o -R.\n");
                    fsp_client_request_queue_freeAllRequests(&queue);
                    return -1;
                }
                queue.tail->dirname = optarg;
                break;
            case 't':
                if(queue.tail == NULL) {
                    fprintf(stderr, "Errore: non è stata specificata la richiesta prima dell'opzione -t.\n");
                    fsp_client_request_queue_freeAllRequests(&queue);
                    return -1;
                }
                queue.tail->time = optarg;
                break;
            case ':':
                if(optopt == 'R') {
                    if((req = fsp_client_request_queue_newRequest(optopt, NULL)) == NULL) {
                        // Errore: memoria insufficiente
                        fprintf(stderr, "Errore: memoria insufficiente.\n");
                        fsp_client_request_queue_freeAllRequests(&queue);
                        return -1;
                    }
                    fsp_client_request_queue_enqueue(&queue, req);
                } else {
                    // Errore: manca l'argomento
                    fprintf(stderr, "Errore: manca l'argomento per l'opzione -%c.\n", optopt);
                    fsp_client_request_queue_freeAllRequests(&queue);
                    return -1;
                }
                break;
            case '?':
                // Errore: opzione non riconosciuta
                if(optopt == 'h' || optopt == 'f' || optopt == 'p') {
                    fprintf(stderr, "Errore: opzione -%c aggiunta in modo errato.\n", optopt);
                } else {
                    fprintf(stderr, "Errore: opzione -%c non riconosciuta.\n", optopt);
                }
                fsp_client_request_queue_freeAllRequests(&queue);
                return -1;
            default:
                break;
        }
    }
    
    // Apre la connessione
    if(p) printf("Apertura della connessione in corso...\n");
    struct timespec abstime;
    abstime.tv_sec = 30;
    abstime.tv_nsec = 0;
    if(openConnection(socket_filename, 3000, abstime) != 0) {
        switch(errno) {
            case EINVAL:
                // Se sockname == NULL || msec < 0 || abstime.tv_sec < 0 || abstime.tv_nsec < 0 || cossessione già aperta
                fprintf(stderr, "Errore openConnection: uno o più argomenti passati alla funzione non sono validi o la connessione è già aperta.\n");
                break;
            case ETIME:
                // Se è scaduto il tempo entro il quale fare richieste di connessione al server
                fprintf(stderr, "Errore openConnection: tempo scaduto.\n");
                break;
            case ECONNREFUSED:
                // Se non è stato possibile connettersi al server o il server non è disponibile
                fprintf(stderr, "Errore openConnection: impossibile connettersi al server.\n");
                break;
            case EBADMSG:
                // Se il messaggio di risposta fsp contiene un codice non riconosciuto/inatteso.
                fprintf(stderr, "Errore openConnection: codice di risposta fsp non riconosciuto/inatteso.\n");
                break;
            case ENAMETOOLONG:
                // Se sockname è troppo lungo (la massima lunghezza è definita in FSP_API_SOCKET_NAME_MAX_LEN)
                fprintf(stderr, "Errore openConnection: sockname troppo lungo.\n");
                break;
            case ENOBUFS:
                // Se è stato impossibile allocare memoria per il buffer
                fprintf(stderr, "Errore openConnection: impossibile allocare memoria per il buffer.\n");
                break;
            default:
                break;
        }
        fsp_client_request_queue_freeAllRequests(&queue);
        if(p) printf("Impossibile connettersi.\n");
        return -1;
    }
    if(p) printf("Connessione aperta.\n\n");
    
    // Esegue le richieste
    while((req = fsp_client_request_queue_dequeue(&queue)) != NULL) {
        long int time = -1;
        if(req->time != NULL && (!isNumber(req->time, &time) || time < 0)) {
            // L'argomento dell'opzione -t non è valido
            fprintf(stderr, "Errore richiesta -%c: l'argomento dell'opzione -t non è valido.\n", req->opt);
            fsp_client_request_queue_freeRequest(req);
            continue;
        } else {
            time = 0;
        }
        
        int res = 0;
        switch(req->opt) {
            case 'w':
                if(p) {
                    printf("-w %s", req->arg);
                    if(req->dirname != NULL) printf(" -D %s", req->dirname);
                    if(time != 0) printf(" -t %lu", time);
                    printf("\n");
                }
                switch (res = write_opt(req->arg, req->dirname, (unsigned int) time, p, &files)) {
                    case -1:
                        fprintf(stderr, "Errore richiesta -w: impossibile aggiornare l'albero di ricerca.\n");
                        break;
                    case -2:
                        fprintf(stderr, "Errore richiesta -w: impossibile tornare alla working directory iniziale.\n");
                        break;
                    default:
                        // Successo
                        break;
                }
                break;
            case 'W':
                if(p) {
                    printf("-W %s", req->arg);
                    if(req->dirname != NULL) printf(" -D %s", req->dirname);
                    if(time != 0) printf(" -t %lu", time);
                    printf("\n");
                }
                res = Write_opt(req->arg, req->dirname, (unsigned int) time, p, &files);
                if(res != 0) fprintf(stderr, "Errore richiesta -W: impossibile aggiornare l'albero di ricerca.\n");
                break;
            case 'r':
                if(p) {
                    printf("-r %s", req->arg);
                    if(req->dirname != NULL) printf(" -d %s", req->dirname);
                    if(time != 0) printf(" -t %lu", time);
                    printf("\n");
                }
                switch (res = read_opt(req->arg, req->dirname, (unsigned int) time, p, &files)) {
                    case -1:
                        fprintf(stderr, "Errore richiesta -r: impossibile aggiornare l'albero di ricerca.\n");
                        break;
                    case -2:
                        fprintf(stderr, "Errore richiesta -r: impossibile allocare memoria.\n");
                        break;
                    default:
                        break;
                }
                break;
            case 'R':
                if(p) {
                    printf("-R");
                    if(req->arg != NULL) printf(" %s", req->arg);
                    if(req->dirname != NULL) printf(" -d %s", req->dirname);
                    if(time != 0) printf(" -t %lu", time);
                    printf("\n");
                }
                Read_opt(req->arg, req->dirname, (unsigned int) time, p);
                break;
            case 'l':
                if(p) printf("-l %s", req->arg);
                if(time != 0) printf(" -t %lu", time);
                printf("\n");
                res = lock_opt(req->arg, (unsigned int) time, p, &files);
                if(res != 0) fprintf(stderr, "Errore richiesta -l: impossibile aggiornare l'albero di ricerca.\n");
                break;
            case 'u':
                if(p) printf("-u %s", req->arg);
                if(time != 0) printf(" -t %lu", time);
                printf("\n");
                unlock_opt(req->arg, (unsigned int) time, p);
                break;
            case 'c':
                if(p) printf("-c %s", req->arg);
                if(time != 0) printf(" -t %lu", time);
                printf("\n");
                res = cancel_opt(req->arg, (unsigned int) time, p, &files);
                if(res != 0) fprintf(stderr, "Errore richiesta -c: impossibile aggiornare l'albero di ricerca.\n");
                break;
            default:
                // Non viene mai eseguito
                fprintf(stderr, "Errore: richiesta -%c non riconosciuta.\n", req->opt);
                res = -1;
                break;
        }
        fsp_client_request_queue_freeRequest(req);
        if(res != 0) {
            fsp_client_request_queue_freeRequest(req);
            fsp_client_request_queue_freeAllRequests(&queue);
            fsp_opened_files_bst_deleteAll(files, close_file);
            closeConnection(socket_filename);
            return -1;
        }
        if(p) printf("\n");
    }
    
    // Chiude tutti i file rimasti aperti
    fsp_opened_files_bst_deleteAll(files, close_file);
    
    // Chiude la connessione
    if(p) printf("Chiusura della connessione in corso...\n");
    if(closeConnection(socket_filename) != 0) {
        switch(errno) {
            case EINVAL:
                // Se sockname == NULL || sockname attuale differente
                fprintf(stderr, "Errore closeConnection: l'argomento passato alla funzione non è valido.\n");
                break;
            case ENOTCONN:
                // Se non è stata aperta la connessione con openConnection()
                fprintf(stderr, "Errore closeConnection: la connessione non è stata precedentemente aperta con openConnection.\n");
                break;
            case ENOBUFS:
                // Se la memoria per il buffer non è sufficiente
                fprintf(stderr, "Errore closeConnection: la memoria per il buffer non è sufficiente.\n");
                break;
            case EIO:
                // Se ci sono stati errori di lettura e scrittura su socket
                fprintf(stderr, "Errore closeConnection: si è verificato un errore di lettura o scrittura su socket.\n");
                break;
            case ECONNABORTED:
                // Se il servizio non è disponibile e la connessione è stata chiusa
                fprintf(stderr, "Errore closeConnection: il servizio non è disponibile e la connessione è stata chiusa.\n");
                break;
            case EBADMSG:
                // Se uno dei messaggi di richiesta o risposta fsp contiene errori sintattici
                fprintf(stderr, "Errore closeConnection: il messaggio di richiesta o risposta fsp contiene errori sintattici.\n");
                break;
            case ECANCELED:
                // Se non è stato possibile eseguire l'operazione
                fprintf(stderr, "Errore closeConnection: non è stato possibile eseguire l'operazione.\n");
                break;
            default:
                break;
        }
        if(p) printf("Impossibile chiudere la connessione.\n");
        return -1;
    }
    if(p) printf("Connessione chiusa.\n");
    
    return 0;
}

static void printUsage() {
    printf("fsp -h\n");
    printf("fsp -f filename [-p] request_1 ... request_n\n");
    printf("\trequest_m (1 <= m <= n) := write_req | read_req | lock_req | unlock_req | cancel_req\n");
    printf("\twrite_req := -w dirname[,n_val] [-D dirname] [-t time] | -W file_1[,file_2,...] [-D dirname] [-t time]\n");
    printf("\tread_req := -r file_1[,file_2,...] [-d dirname] [-t time] | -R [n_val] [-d dirname] [-t time]\n");
    printf("\tlock_req := -l file_1[,file_2,...] [-t time]\n");
    printf("\tunlock_req := -u file_1[,file_2,...] [-t time]\n");
    printf("\tcancel_req := -c file_1[,file_2,...] [-t time]\n");
}

static int write_opt(char* arg, char* dirname, unsigned int time, int p, OpenedFiles** files) {
    time = time/1000;
    char* dir_arg = arg;
    char* n_arg = NULL;
    long int n = -1;
    
    while(*arg != ',' || *arg != '\0') arg++;
    if(*arg == ',') {
        *arg = '\0';
        n_arg = arg++;
        if(*n_arg == '\0') {
            fprintf(stderr, "Errore richiesta -w: valore n non specificato dopo la virgola.\n");
            if(p) {
                printf("Non è stato possibile eseguire l'operazione di scrittura dei file contenuti nella cartella %s sul server (non è stato specificato un valore dopo la virgola).\n", dir_arg);
            }
            return 0;
        }
    }
    
    if(n_arg != NULL) {
        if(!isNumber(n_arg, &n) || n < 0 || n > INT_MAX) {
            fprintf(stderr, "Errore richiesta -w: il valore n specificato non è valido.\n");
            if(p) {
                printf("Non è stato possibile eseguire l'operazione di scrittura dei file contenuti nella cartella %s sul server (il valore n specificato non è valido).\n", dir_arg);
            }
            return 0;
        }
    }
    
    // Controlla se dir_arg è una directory
    struct stat info;
    if(stat(dir_arg, &info) == 0) {
        if(!S_ISDIR(info.st_mode)) {
            fprintf(stderr, "Errore richiesta -w: l'argomento %s non è una directory.\n", dir_arg);
            if(p) {
                printf("Non è stato possibile eseguire l'operazione di scrittura dei file contenuti nella cartella %s sul server (%s non è una directory).\n", dir_arg, dir_arg);
            }
            return 0;
        }
    } else {
        // Errore stat
        fprintf(stderr, "Errore richiesta -w: directory %s non trovata.\n", dir_arg);
        if(p) {
            printf("Non è stato possibile eseguire l'operazione di scrittura dei file contenuti nella cartella %s sul server (la directory %s non è stata trovata).\n", dir_arg, dir_arg);
        }
        return 0;
    }
    
    char current_dir[FSP_CLIENT_FILE_MAX_LEN];
    if(getcwd(current_dir, FSP_CLIENT_FILE_MAX_LEN) == NULL) {
        fprintf(stderr, "Errore richiesta -w: è stato impossibile determinare la working directory corrente.\n");
        if(p) {
            printf("Non è stato possibile eseguire l'operazione di scrittura dei file contenuti nella cartella %s sul server (working directory corrente non determinata).\n", dir_arg);
        }
        return 0;
    }
    
    if(chdir(dir_arg) != 0) {
        // Errore: impossibile accedere alla directory
        fprintf(stderr, "Errore richiesta -w: è stato impossibile accedere alla directory %s.\n", dir_arg);
        if(p) {
            printf("Non è stato possibile eseguire l'operazione di scrittura dei file contenuti nella cartella %s sul server (impossibile accedere alla directory %s).\n", dir_arg, dir_arg);
        }
        return 0;
    }
    
    if(write_opt_rec(&n, dirname, time, p, files) != 0) return -1;
    
    if(chdir(current_dir) != 0) {
        // Errore: impossibile tornare alla working directory precedente
        return -2;
    }
    
    return 0;
}

static int write_opt_rec(long int* n, char* dirname, unsigned int time, int p, OpenedFiles** files) {
    if(*n == 0) {
        return 0;
    }
    
    DIR* dir = NULL;
    if((dir = opendir(".")) == NULL) {
        // Errore: impossibile aprire la directory corrente
        fprintf(stderr, "Errore richiesta -w: è stato impossibile aprire la directory corrente.\n");
        return 0;
    }
    
    char current_dir[FSP_CLIENT_FILE_MAX_LEN];
    if(getcwd(current_dir, FSP_CLIENT_FILE_MAX_LEN) == NULL) {
        fprintf(stderr, "Errore richiesta -w: è stato impossibile determinare la working directory corrente.\n");
        closedir(dir);
        return 0;
    }
    
    struct dirent* entry = NULL;
    while(((void)(errno = 0), entry = readdir(dir)) != NULL) {
        // Controlla se dirent->d_name è una directory
        struct stat info;
        if(stat(entry->d_name, &info) == 0) {
            if(S_ISDIR(info.st_mode)) {
                // È una directory
                if(strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
                    continue;
                }
                
                if(chdir(entry->d_name) != 0) {
                    // Errore: impossibile accedere alla directory
                    fprintf(stderr, "Errore richiesta -w: è stato impossibile accedere alla directory %s.\n", entry->d_name);
                    continue;
                }
                
                if(write_opt_rec(n, dirname, time, p, files) != 0) return -1;
                
                if(chdir(current_dir) != 0) {
                    // Errore: impossibile tornare alla working directory precedente
                    fprintf(stderr, "Errore richiesta -w: è stato impossibile tornare alla directory %s.\n", current_dir);
                    closedir(dir);
                    return 0;
                }
            } else {
                // Non è una directory
                if(*n == 0) {
                    closedir(dir);
                    return 0;
                }
                
                char filename[FSP_CLIENT_FILE_MAX_LEN];
                int result;
                
                // Determina il path assoluto del file entry->d_name e lo salva in filename
                if(realpath(entry->d_name, filename) == NULL) {
                    fprintf(stderr, "Errore richiesta -w: impossibile determinare il path assoluto del file %s.\n", entry->d_name);
                    continue;
                }
                
                // Controlla se il file è già stato aperto
                struct opened_file* o_file = fsp_opened_files_bst_search(*files, filename);
                if(o_file != NULL) {
                    // Chiude il file
                    close_file(filename);
                    fsp_opened_files_bst_delete(files, filename);
                }
                // Apre il file
                if(open_file(filename, O_CREATE | O_LOCK) != 0) continue;
                // Aggiunge il file all'albero binario di ricerca
                if(fsp_opened_files_bst_insert(files, filename, O_CREATE | O_LOCK) != 0) {
                    close_file(filename);
                    return -1;
                }
                
                if((result = writeFile(filename, dirname)) != 0) {
                    switch(errno) {
                        case EINVAL:
                            // Se pathname == NULL || pathname non è un file regolare || dirname non è una directory
                            fprintf(stderr, "Errore writeFile: uno o più argomenti passati alla funzione non sono validi.\n");
                            break;
                        case ENOTCONN:
                            // Se non è stata aperta la connessione con openConnection()
                            fprintf(stderr, "Errore writeFile: la connessione non è stata precedentemente aperta con openConnection.\n");
                            break;
                        case ENOBUFS:
                            // Se la memoria per il buffer non è sufficiente o non è stato possibile allocare memoria
                            fprintf(stderr, "Errore writeFile: la memoria per il buffer non è sufficiente o non è stato possibile allocare memoria.\n");
                            break;
                        case EIO:
                            // Se ci sono stati errori di lettura e scrittura su socket o errori relativi alla scrittura dei file in dirname
                            fprintf(stderr, "Errore writeFile: si è verificato un errore di I/O.\n");
                            break;
                        case ECONNABORTED:
                            // Se il servizio non è disponibile e la connessione è stata chiusa
                            fprintf(stderr, "Errore writeFile: il servizio non è disponibile e la connessione è stata chiusa.\n");
                            break;
                        case EBADMSG:
                            // Se uno dei messaggi di richiesta o risposta fsp contiene errori sintattici
                            fprintf(stderr, "Errore writeFile: il messaggio di richiesta o risposta fsp contiene errori sintattici.\n");
                            break;
                        case ENOENT:
                            // Se il file pathname non è presente sul server
                            fprintf(stderr, "Errore writeFile: il file %s non è presente sul server.\n", filename);
                            break;
                        case ENOMEM:
                            // Se non c'è sufficiente memoria sul server per eseguire l'operazione
                            fprintf(stderr, "Errore writeFile: memoria sul server insufficiente per scrivere il file %s.\n", filename);
                            break;
                        case EPERM:
                            // Se l'operazione non è consentita
                            fprintf(stderr, "Errore writeFile: operazione non consentita sul file %s.\n", filename);
                            break;
                        case ECANCELED:
                            // Se non è stato possibile eseguire l'operazione
                            fprintf(stderr, "Errore writeFile: non è stato possibile eseguire l'operazione sul file %s.\n", filename);
                            break;
                        default:
                            break;
                    }
                }
                
                // Stampa l'esito sullo standard output
                if(p) {
                    if(result == 0) {
                        // Operazione terminata con successo
                        printf("Il file %s è stato scritto nel server con successo.\n", filename);
                    } else {
                        // Operazione non terminata con successo
                        printf("Non è stato possibile scrivere il file %s nel server.\n", filename);
                    }
                }
                
                // Chiude il file
                close_file(filename);
                fsp_opened_files_bst_delete(files, filename);
                // Aggiorna n
                (*n)--;
                // Attende time secondi prima di eseguire la prossima operazione
                sleep(time);
            }
        } else {
            // Errore stat
            fprintf(stderr, "Errore richiesta -w: file %s sconosciuto.\n", entry->d_name);
            closedir(dir);
            return 0;
        }
    }
    if(errno != 0) {
        // Errore
        fprintf(stderr, "Errore richiesta -w: impossibile determinare tutti i file contenuti nella directory %s.\n", current_dir);
    }
    closedir(dir);
    return 0;
}

static int Write_opt(char* arg, char* dirname, unsigned int time, int p, OpenedFiles** files) {
    time = time/1000;
    char* file = NULL;
    char filename[FSP_CLIENT_FILE_MAX_LEN];
    int result;
    
    file = strtok(arg, ",");
    
    do {
        // Determina il path assoluto del file e lo salva in filename
        if(realpath(file, filename) == NULL) {
            if(errno == ENOENT) {
                // File non trovato
                fprintf(stderr, "Errore richiesta -W: il file %s non è stato trovato.\n", file);
            } else if(errno == ENOTDIR) {
                // Path non valido
                fprintf(stderr, "Errore richiesta -W: il path %s non è valido.\n", file);
            } else {
                // Impossibile determinare il path assoluto del file
                fprintf(stderr, "Errore richiesta -W: impossibile determinare il path assoluto del file %s.\n", file);
            }
            
            // Stampa l'esito sullo standard output
            if(p) printf("Non è stato possibile scrivere il file %s nel server in quanto inesistente.\n", file);
            
            if((file = strtok(NULL, ",")) != NULL) continue;
            return 0;
        }
        
        // Controlla se il file è già stato aperto
        struct opened_file* o_file = fsp_opened_files_bst_search(*files, filename);
        if(o_file != NULL) {
            // Chiude il file
            close_file(filename);
            fsp_opened_files_bst_delete(files, filename);
        }
        // Apre il file
        if(open_file(filename, O_CREATE | O_LOCK) != 0) {
            if(p) printf("Non è stato possibile scrivere il file %s nel server (apertura con flags O_CREATE | O_LOCK non riuscita).\n", filename);
            if((file = strtok(NULL, ",")) != NULL) continue;
            return 0;
        }
        // Aggiunge il file all'albero binario di ricerca
        if(fsp_opened_files_bst_insert(files, filename, O_CREATE | O_LOCK) != 0) {
            close_file(filename);
            return -1;
        }
        
        if((result = writeFile(filename, dirname)) != 0) {
            switch(errno) {
                case EINVAL:
                    // Se pathname == NULL || pathname non è un file regolare || dirname non è una directory
                    fprintf(stderr, "Errore writeFile: uno o più argomenti passati alla funzione non sono validi.\n");
                    break;
                case ENOTCONN:
                    // Se non è stata aperta la connessione con openConnection()
                    fprintf(stderr, "Errore writeFile: la connessione non è stata precedentemente aperta con openConnection.\n");
                    break;
                case ENOBUFS:
                    // Se la memoria per il buffer non è sufficiente o non è stato possibile allocare memoria
                    fprintf(stderr, "Errore writeFile: la memoria per il buffer non è sufficiente o non è stato possibile allocare memoria.\n");
                    break;
                case EIO:
                    // Se ci sono stati errori di lettura e scrittura su socket o errori relativi alla scrittura dei file in dirname
                    fprintf(stderr, "Errore writeFile: si è verificato un errore di I/O.\n");
                    break;
                case ECONNABORTED:
                    // Se il servizio non è disponibile e la connessione è stata chiusa
                    fprintf(stderr, "Errore writeFile: il servizio non è disponibile e la connessione è stata chiusa.\n");
                    break;
                case EBADMSG:
                    // Se uno dei messaggi di richiesta o risposta fsp contiene errori sintattici
                    fprintf(stderr, "Errore writeFile: il messaggio di richiesta o risposta fsp contiene errori sintattici.\n");
                    break;
                case ENOENT:
                    // Se il file pathname non è presente sul server
                    fprintf(stderr, "Errore writeFile: il file %s non è presente sul server.\n", filename);
                    break;
                case ENOMEM:
                    // Se non c'è sufficiente memoria sul server per eseguire l'operazione
                    fprintf(stderr, "Errore writeFile: memoria sul server insufficiente per scrivere il file %s.\n", filename);
                    break;
                case EPERM:
                    // Se l'operazione non è consentita
                    fprintf(stderr, "Errore writeFile: operazione non consentita sul file %s.\n", filename);
                    break;
                case ECANCELED:
                    // Se non è stato possibile eseguire l'operazione
                    fprintf(stderr, "Errore writeFile: non è stato possibile eseguire l'operazione sul file %s.\n", filename);
                    break;
                default:
                    break;
            }
        }
        
        // Stampa l'esito sullo standard output
        if(p) {
            if(result == 0) {
                // Operazione terminata con successo
                printf("Il file %s è stato scritto nel server con successo.\n", filename);
            } else {
                // Operazione non terminata con successo
                printf("Non è stato possibile scrivere il file %s nel server.\n", filename);
            }
        }
        
        // Chiude il file
        close_file(filename);
        fsp_opened_files_bst_delete(files, filename);
        
        // Attende time secondi prima di eseguire la prossima operazione
        sleep(time);
    } while((file = strtok(NULL, ",")) != NULL);
    
    return 0;
}

static int read_opt(char* arg, char* dirname, unsigned int time, int p, OpenedFiles** files) {
    time = time/1000;
    char* file = NULL;
    char* buf = NULL;
    size_t buf_size;
    int result;
    
    file = strtok(arg, ",");
    
    do {
        // Controlla se il file è già stato aperto
        struct opened_file* o_file = fsp_opened_files_bst_search(*files, file);
        if(o_file == NULL) {
            // Il file non è ancora stato aperto
            // Apre il file
            if(open_file(file, O_DEFAULT) != 0) {
                if(p) printf("Non è stato possibile leggere il file %s dal server (apertura del file non riuscita).\n", file);
                if((file = strtok(NULL, ",")) != NULL) continue;
                return 0;
            }
            // Aggiunge il file all'albero binario di ricerca
            if(fsp_opened_files_bst_insert(files, file, O_DEFAULT) != 0) {
                close_file(file);
                return -1;
            }
        }
        
        // Legge il file
        if((result = readFile(file, (void**) &buf, &buf_size)) != 0) {
            switch(errno) {
                case EINVAL:
                    // Se pathname == NULL || buf == NULL || size == NULL
                    fprintf(stderr, "Errore readFile: uno o più argomenti passati alla funzione non sono validi.\n");
                    break;
                case ENOTCONN:
                    // Se non è stata aperta la connessione con openConnection()
                    fprintf(stderr, "Errore readFile: la connessione non è stata precedentemente aperta con openConnection.\n");
                    break;
                case ENOBUFS:
                    // Se la memoria per il buffer non è sufficiente o non è stato possibile allocare memoria
                    fprintf(stderr, "Errore readFile: la memoria per il buffer non è sufficiente o non è stato possibile allocare memoria.\n");
                    break;
                case EIO:
                    // Se ci sono stati errori di lettura e scrittura su socket
                    fprintf(stderr, "Errore readFile: si è verificato un errore di lettura o scrittura su socket.\n");
                    break;
                case ECONNABORTED:
                    // Se il servizio non è disponibile e la connessione è stata chiusa
                    fprintf(stderr, "Errore readFile: il servizio non è disponibile e la connessione è stata chiusa.\n");
                    break;
                case EBADMSG:
                    // Se uno dei messaggi di richiesta o risposta fsp contiene errori sintattici
                    fprintf(stderr, "Errore readFile: il messaggio di richiesta o risposta fsp contiene errori sintattici.\n");
                    break;
                case ENOENT:
                    // Se il file pathname non è presente sul server
                    fprintf(stderr, "Errore readFile: il file %s non è presente sul server.\n", file);
                    break;
                case EPERM:
                    // Se l'operazione non è consentita
                    fprintf(stderr, "Errore readFile: operazione non consentita sul file %s.\n", file);
                    break;
                case ECANCELED:
                    // Se non è stato possibile eseguire l'operazione
                    fprintf(stderr, "Errore readFile: non è stato possibile eseguire l'operazione sul file %s.\n", file);
                    break;
                default:
                    break;
            }
        }
        
        // Stampa l'esito sullo standard output
        if(p) {
            if(result == 0) {
                // Operazione terminata con successo
                printf("Il file %s è stato letto dal server con successo (%lu bytes letti).\n", file, buf_size);
            } else {
                // Operazione non terminata con successo
                printf("Non è stato possibile leggere il file %s dal server.\n", file);
            }
        }
        
        // Salva nella cartella il file se dirname != NULL
        if(dirname != NULL && result == 0) {
            unsigned long dirname_len = strlen(dirname);
            unsigned long file_len = strlen(file);
            
            // Copia il nome del file in file_cpy
            char* file_cpy;
            if((file_cpy = calloc(file_len+1, sizeof(char))) == NULL) {
                // Memoria non allocata
                free(buf);
                return -2;
            }
            memcpy(file_cpy, file, file_len);
            file_cpy[file_len] = '\0';
            
            // Modifica il nome del file per la scrittura nella directory dirname
            // Il primo carattere deve essere '/' in quanto il file è identificato
            // univocamente dal suo path assoluto
            assert(*file_cpy == '/');
            char* f = file_cpy+1;
            while(*f != '\0') {
                if(*f == '/') {
                    *f = '_';
                }
                f++;
            }
            
            // Se il nome della directory termina con '/', allora non lo considera
            if(dirname[dirname_len-1] == '/') dirname_len--;
            // Concatena il path della directory con il nuovo nome del file (file_cpy)
            char* filename;
            if((filename = calloc(dirname_len+file_len+1, sizeof(char))) == NULL) {
                // Memoria non allocata
                free(file_cpy);
                free(buf);
                return -2;
            }
            memcpy(filename, dirname, dirname_len);
            memcpy(filename+dirname_len, file_cpy, file_len);
            filename[dirname_len+file_len] = '\0';
            
            // Apre _file in scrittura e scrive
            FILE* _file;
            if((_file = fopen(filename, "wb")) != NULL) {
                size_t wrote_bytes = fwrite(buf, 1, buf_size, _file);
                if(wrote_bytes < buf_size) {
                    // Non sono stati scritti tutti i byte (errore)
                    fprintf(stderr, "Errore richiesta -r: non è stato possibile scrivere il file %s nella cartella %s.\n", file, dirname);
                    if(p) {
                        printf("Non è stato possibile scrivere il file %s nella cartella %s.\n", file, dirname);
                    }
                } else {
                    if(p) {
                        printf("Il file %s è stato scritto nella cartella %s con successo sotto il nome di %s.\n", file, dirname, file_cpy+1);
                    }
                }
                // Chiude _file
                fclose(_file);
            } else {
                // Errore durante l'apertura di _file
                fprintf(stderr, "Errore richiesta -r: non è stato possibile aprire il file %s in modalità scrittura nella cartella %s.\n", file, dirname);
                if(p) {
                    printf("Non è stato possibile scrivere il file %s nella cartella %s.\n", file, dirname);
                }
            }

            // Libera dalla memoria file_cpy e filename
            free(file_cpy);
            free(filename);
        }
        
        // Libera dalla memoria buf e attende time secondi prima di eseguire la prossima operazione
        if(result == 0) {
            free(buf);
            buf = NULL;
        }
        sleep(time);
    } while((file = strtok(NULL, ",")) != NULL);
    
    return 0;
}

static void Read_opt(char* arg, char* dirname, unsigned int time, int p) {
    time = time/1000;
    long int n = 0;
    int result;
    
    if(arg != NULL) {
        if(!isNumber(arg, &n) || n < 0 || n > INT_MAX) {
            fprintf(stderr, "Errore richiesta -R: il valore n specificato non è valido.\n");
            // Stampa l'esito sullo standard output
            if(p) {
                printf("Non è stato possibile scrivere nessun file nella cartella %s in quanto il valore n dell'argomento non è valido.\n", dirname);
            }
            return;
        }
    }
    
    if((result = readNFiles((int) n, dirname)) != 0) {
        switch(errno) {
            case EINVAL:
                // Se dirname == NULL || dirname non è una directory
                fprintf(stderr, "Errore readNFiles: uno o più argomenti passati alla funzione non sono validi.\n");
                break;
            case ENOTCONN:
                // Se non è stata aperta la connessione con openConnection()
                fprintf(stderr, "Errore readNFiles: la connessione non è stata precedentemente aperta con openConnection.\n");
                break;
            case ENOBUFS:
                // Se la memoria per il buffer non è sufficiente o non è stato possibile allocare memoria
                fprintf(stderr, "Errore readNFiles: la memoria per il buffer non è sufficiente o non è stato possibile allocare memoria.\n");
                break;
            case EIO:
                // se ci sono stati errori di lettura e scrittura su socket o errori relativi alla scrittura dei file in dirname
                fprintf(stderr, "Errore readNFiles: si è verificato un errore di I/O.\n");
                break;
            case ECONNABORTED:
                // Se il servizio non è disponibile e la connessione è stata chiusa
                fprintf(stderr, "Errore readNFiles: il servizio non è disponibile e la connessione è stata chiusa.\n");
                break;
            case EBADMSG:
                // Se uno dei messaggi di richiesta o risposta fsp contiene errori sintattici
                fprintf(stderr, "Errore readNFiles: il messaggio di richiesta o risposta fsp contiene errori sintattici.\n");
                break;
            default:
                break;
        }
    }
    
    // Stampa l'esito sullo standard output
    if(p) {
        if(result >= 0) {
            // Operazione terminata con successo
            printf("Sono stati scritti con successo %d file nella cartella %s.\n", result, dirname);
        } else {
            // Operazione non terminata con successo
            printf("Non è stato possibile scrivere nessun file nella cartella %s.\n", dirname);
        }
    }
    
    // Attende time secondi prima di eseguire la prossima operazione
    sleep(time);
}

static int lock_opt(char* arg, unsigned int time, int p, OpenedFiles** files) {
    time = time/1000;
    char* file = NULL;
    int result;
    
    file = strtok(arg, ",");
    
    do {
        // Controlla se il file è già stato aperto
        struct opened_file* o_file = fsp_opened_files_bst_search(*files, file);
        if(o_file == NULL) {
            // Il file non è ancora stato aperto
            // Apre il file
            if(open_file(file, O_LOCK) != 0) {
                if(p) printf("Non è stato possibile settare il flag O_LOCK al file %s durante la sua apertura.\n", file);
                if((file = strtok(NULL, ",")) != NULL) continue;
                return 0;
            }
            // Aggiunge il file all'albero binario di ricerca
            if(fsp_opened_files_bst_insert(files, file, O_LOCK) != 0) {
                close_file(file);
                return -1;
            }
            if((file = strtok(NULL, ",")) != NULL) continue;
            return 0;
        }
        
        // Setta la lock
        if((result = lockFile(file)) != 0) {
            switch(errno) {
                case EINVAL:
                    // Se pathname == NULL
                    fprintf(stderr, "Errore lockFile: l'argomento passato alla funzione non è valido.\n");
                    break;
                case ENOTCONN:
                    // Se non è stata aperta la connessione con openConnection()
                    fprintf(stderr, "Errore lockFile: la connessione non è stata precedentemente aperta con openConnection.\n");
                    break;
                case ENOBUFS:
                    // Se la memoria per il buffer non è sufficiente
                    fprintf(stderr, "Errore lockFile: la memoria per il buffer non è sufficiente.\n");
                    break;
                case EIO:
                    // Se ci sono stati errori di lettura e scrittura su socket
                    fprintf(stderr, "Errore lockFile: si è verificato un errore di lettura o scrittura su socket.\n");
                    break;
                case ECONNABORTED:
                    // Se il servizio non è disponibile e la connessione è stata chiusa
                    fprintf(stderr, "Errore lockFile: il servizio non è disponibile e la connessione è stata chiusa.\n");
                    break;
                case EBADMSG:
                    // Se uno dei messaggi di richiesta o risposta fsp contiene errori sintattici
                    fprintf(stderr, "Errore lockFile: il messaggio di richiesta o risposta fsp contiene errori sintattici.\n");
                    break;
                case ENOENT:
                    // Se il file pathname non è presente sul server
                    fprintf(stderr, "Errore lockFile: il file %s non è presente sul server.\n", file);
                    break;
                case ECANCELED:
                    // Se non è stato possibile eseguire l'operazione
                    fprintf(stderr, "Errore lockFile: non è stato possibile eseguire l'operazione sul file %s.\n", file);
                    break;
                default:
                    break;
            }
        }
        
        // Stampa l'esito sullo standard output
        if(p) {
            if(result == 0) {
                // Operazione terminata con successo
                printf("Il flag O_LOCK è stato settato al file %s con successo.\n", file);
            } else {
                // Operazione non terminata con successo
                printf("Non è stato possibile settare il flag O_LOCK al file %s.\n", file);
            }
        }
        
        // Attende time secondi prima di eseguire la prossima operazione
        sleep(time);
    } while((file = strtok(NULL, ",")) != NULL);
    
    return 0;
}

static void unlock_opt(char* arg, unsigned int time, int p) {
    time = time/1000;
    char* file = NULL;
    int result;
    
    file = strtok(arg, ",");
    
    do {
        if((result = unlockFile(file)) != 0) {
            switch(errno) {
                case EINVAL:
                    // Se pathname == NULL
                    fprintf(stderr, "Errore unlockFile: l'argomento passato alla funzione non è valido.\n");
                    break;
                case ENOTCONN:
                    // Se non è stata aperta la connessione con openConnection()
                    fprintf(stderr, "Errore unlockFile: la connessione non è stata precedentemente aperta con openConnection.\n");
                    break;
                case ENOBUFS:
                    // Se la memoria per il buffer non è sufficiente
                    fprintf(stderr, "Errore unlockFile: la memoria per il buffer non è sufficiente.\n");
                    break;
                case EIO:
                    // Se ci sono stati errori di lettura e scrittura su socket
                    fprintf(stderr, "Errore unlockFile: si è verificato un errore di lettura o scrittura su socket.\n");
                    break;
                case ECONNABORTED:
                    // Se il servizio non è disponibile e la connessione è stata chiusa
                    fprintf(stderr, "Errore unlockFile: il servizio non è disponibile e la connessione è stata chiusa.\n");
                    break;
                case EBADMSG:
                    // Se uno dei messaggi di richiesta o risposta fsp contiene errori sintattici
                    fprintf(stderr, "Errore unlockFile: il messaggio di richiesta o risposta fsp contiene errori sintattici.\n");
                    break;
                case ENOENT:
                    // Se il file pathname non è presente sul server
                    fprintf(stderr, "Errore unlockFile: il file %s non è presente sul server.\n", file);
                    break;
                case EPERM:
                    // Se l'operazione non è consentita
                    fprintf(stderr, "Errore unlockFile: operazione non consentita sul file %s.\n", file);
                    break;
                case ECANCELED:
                    // Se non è stato possibile eseguire l'operazione
                    fprintf(stderr, "Errore unlockFile: non è stato possibile eseguire l'operazione sul file %s.\n", file);
                    break;
                default:
                    break;
            }
        }
        
        // Stampa l'esito sullo standard output
        if(p) {
            if(result == 0) {
                // Operazione terminata con successo
                printf("Il flag O_LOCK è stato resettato sul file %s con successo.\n", file);
            } else {
                // Operazione non terminata con successo
                printf("Non è stato possibile resettare il flag O_LOCK sul file %s.\n", file);
            }
        }
        
        // Attende time secondi prima di eseguire la prossima operazione
        sleep(time);
    } while((file = strtok(NULL, ",")) != NULL);
}

static int cancel_opt(char* arg, unsigned int time, int p, OpenedFiles** files) {
    time = time/1000;
    char* file = NULL;
    int result;
    
    file = strtok(arg, ",");
    
    do {
        // Controlla se il file è già stato aperto
        struct opened_file* o_file = fsp_opened_files_bst_search(*files, file);
        if(o_file != NULL && o_file->flags != O_LOCK && o_file->flags != (O_CREATE | O_LOCK)) {
            // Il file è già stato aperto e non ha il flag O_LOCK settato
            // Chiude il file
            close_file(file);
            fsp_opened_files_bst_delete(files, file);
            o_file = NULL;
        }
        // Controlla se il file è ancora aperto o è stato appena chiuso
        if(o_file == NULL) {
            // Apre il file
            if(open_file(file, O_LOCK) != 0) {
                if(p) printf("Non è stato possibile rimuovere dal server il file %s (apertura con flag O_LOCK non riuscita).\n", file);
                if((file = strtok(NULL, ",")) != NULL) continue;
                return 0;
            }
            // Aggiunge il file all'albero binario di ricerca
            if(fsp_opened_files_bst_insert(files, file, O_LOCK) != 0) {
                close_file(file);
                return -1;
            }
        }
        
        // Rimuove il file
        if((result = removeFile(file)) != 0) {
            switch(errno) {
                case EINVAL:
                    // Se pathname == NULL
                    fprintf(stderr, "Errore removeFile: l'argomento passato alla funzione non è valido.\n");
                    break;
                case ENOTCONN:
                    // Se non è stata aperta la connessione con openConnection()
                    fprintf(stderr, "Errore removeFile: la connessione non è stata precedentemente aperta con openConnection.\n");
                    break;
                case ENOBUFS:
                    // Se la memoria per il buffer non è sufficiente
                    fprintf(stderr, "Errore removeFile: la memoria per il buffer non è sufficiente.\n");
                    break;
                case EIO:
                    // Se ci sono stati errori di lettura e scrittura su socket
                    fprintf(stderr, "Errore removeFile: si è verificato un errore di lettura o scrittura su socket.\n");
                    break;
                case ECONNABORTED:
                    // Se il servizio non è disponibile e la connessione è stata chiusa
                    fprintf(stderr, "Errore removeFile: il servizio non è disponibile e la connessione è stata chiusa.\n");
                    break;
                case EBADMSG:
                    // Se uno dei messaggi di richiesta o risposta fsp contiene errori sintattici
                    fprintf(stderr, "Errore removeFile: il messaggio di richiesta o risposta fsp contiene errori sintattici.\n");
                    break;
                case ENOENT:
                    // Se il file pathname non è presente sul server
                    fprintf(stderr, "Errore removeFile: il file %s non è presente sul server.\n", file);
                    break;
                case EPERM:
                    // Se l'operazione non è consentita
                    fprintf(stderr, "Errore removeFile: operazione non consentita sul file %s.\n", file);
                    break;
                case ECANCELED:
                    // Se non è stato possibile eseguire l'operazione
                    fprintf(stderr, "Errore removeFile: non è stato possibile eseguire l'operazione sul file %s.\n", file);
                    break;
                default:
                    break;
            }
        }
        
        // Stampa l'esito sullo standard output
        if(p) {
            if(result == 0) {
                // Operazione terminata con successo
                printf("Il file %s è stato rimosso dal server con successo.\n", file);
            } else {
                // Operazione non terminata con successo
                printf("Non è stato possibile rimuovere dal server il file %s.\n", file);
            }
        }
        
        // Chiude il file
        close_file(file);
        fsp_opened_files_bst_delete(files, file);
        
        // Attende time secondi prima di eseguire la prossima operazione
        sleep(time);
    } while((file = strtok(NULL, ",")) != NULL);
    
    return 0;
}

static int open_file(char* pathname, int flags) {
    if(openFile(pathname, flags) != 0) {
        switch(errno) {
            case EINVAL:
                // Se pathname == NULL || (flags != O_DEFAULT && flags != O_CREATE && flags != O_LOCK && flags != O_CREATE | O_LOCK)
                fprintf(stderr, "Errore openFile: uno o più argomenti passati alla funzione non sono validi.\n");
                break;
            case ENOTCONN:
                // Se non è stata aperta la connessione con openConnection()
                fprintf(stderr, "Errore openFile: la connessione non è stata precedentemente aperta con openConnection.\n");
                break;
            case ENOBUFS:
                // Se la memoria per il buffer non è sufficiente
                fprintf(stderr, "Errore openFile: la memoria per il buffer non è sufficiente.\n");
                break;
            case EIO:
                // Se ci sono stati errori di lettura e scrittura su socket
                fprintf(stderr, "Errore openFile: si è verificato un errore di lettura o scrittura su socket.\n");
                break;
            case ECONNABORTED:
                // Se il servizio non è disponibile e la connessione è stata chiusa
                fprintf(stderr, "Errore openFile: il servizio non è disponibile e la connessione è stata chiusa.\n");
                break;
            case EBADMSG:
                // Se uno dei messaggi di richiesta o risposta fsp contiene errori sintattici
                fprintf(stderr, "Errore openFile: il messaggio di richiesta o risposta fsp contiene errori sintattici.\n");
                break;
            case ENOENT:
                // Se non viene passato il flag O_CREATE e il file pathname non è presente sul server
                fprintf(stderr, "Errore openFile: non è stato passato il flag O_CREATE, ma il file %s non è presente sul server.\n", pathname);
                break;
            case ENOMEM:
                // Se non c'è sufficiente memoria sul server per eseguire l'operazione
                fprintf(stderr, "Errore openFile: memoria sul server insufficiente per creare il file %s.\n", pathname);
                break;
            case EPERM:
                // Se l'operazione non è consentita
                fprintf(stderr, "Errore openFile: operazione non consentita sul file %s.\n", pathname);
                break;
            case EEXIST:
                // Se viene passato il flag O_CREATE ed il file pathname esiste già memorizzato nel server
                fprintf(stderr, "Errore openFile: è stato passato il flag O_CREATE, ma il file %s è già presente sul server.\n", pathname);
                break;
            case ECANCELED:
                // Se non è stato possibile eseguire l'operazione
                fprintf(stderr, "Errore openFile: non è stato possibile eseguire l'operazione sul file %s.\n", pathname);
                break;
            default:
                break;
        }
        return -1;
    }
    
    return 0;
}

static void close_file(const char* pathname) {
    if(closeFile(pathname) != 0) {
        switch(errno) {
            case EINVAL:
                // Se pathname == NULL
                fprintf(stderr, "Errore closeFile: l'argomento passato alla funzione non è valido.\n");
                break;
            case ENOTCONN:
                // Se non è stata aperta la connessione con openConnection()
                fprintf(stderr, "Errore closeFile: la connessione non è stata precedentemente aperta con openConnection.\n");
                break;
            case ENOBUFS:
                // Se la memoria per il buffer non è sufficiente
                fprintf(stderr, "Errore closeFile: la memoria per il buffer non è sufficiente.\n");
                break;
            case EIO:
                // Se ci sono stati errori di lettura e scrittura su socket
                fprintf(stderr, "Errore closeFile: si è verificato un errore di lettura o scrittura su socket.\n");
                break;
            case ECONNABORTED:
                // Se il servizio non è disponibile e la connessione è stata chiusa
                fprintf(stderr, "Errore closeFile: il servizio non è disponibile e la connessione è stata chiusa.\n");
                break;
            case EBADMSG:
                // Se uno dei messaggi di richiesta o risposta fsp contiene errori sintattici
                fprintf(stderr, "Errore closeFile: il messaggio di richiesta o risposta fsp contiene errori sintattici.\n");
                break;
            case ENOENT:
                // Se il file pathname non è presente sul server
                fprintf(stderr, "Errore closeFile: il file %s non è presente sul server.\n", pathname);
                break;
            case ECANCELED:
                // Se non è stato possibile eseguire l'operazione
                fprintf(stderr, "Errore closeFile: non è stato possibile eseguire l'operazione sul file %s.\n", pathname);
                break;
            default:
                break;
        }
    }
}
