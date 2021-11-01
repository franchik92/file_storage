/*
 * Autore: Francesco Gallicchio
 * Matricola: 579131
 */

#include <fsp_api.h>
#include <fsp_parser.h>
#include <fsp_reader.h>
#include <utils.h>

#include <stdio.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

// Dimensione del buffer di default (4MB)
#define FSP_API_BUF_DEF_SIZE 4194304

// Nome socket
static char sname[UNIX_PATH_MAX];
// Socket file descriptor
static int sfd = -1;
// Buffer
static void* fsp_buf = NULL;
static size_t fsp_buf_size = 0;

/**
 * \brief Invia un messaggio di richiesta fsp con comando cmd, argomento pathname e campo dato di lunghezza data_len.
 */
static int sendFspReq(enum fsp_command cmd, const char* pathname, size_t data_len, void* data);

/**
 * \brief Riceve un messaggio di risposta fsp e lo salva in resp.
 */
static int receiveFspResp(struct fsp_response* resp);

/**
 * \brief Salva nella directory dirname i dati contenuti in data (campo DATA dei messaggi fsp)
 *        di lunghezza totale data_len.
 *        Restituisce il numero dei file salvati in dirname.
 */
static int saveData(const char* dirname, size_t data_len, void* data);

int openConnection(const char* sockname, int msec, const struct timespec abstime) {
    if(sockname == NULL || msec < 0 || abstime.tv_sec < 0 || abstime.tv_nsec < 0 || fsp_buf != NULL) {
        errno = EINVAL;
        return -1;
    }
    
    if(strlen(sockname) >= UNIX_PATH_MAX) {
        errno = ENAMETOOLONG;
        return -1;
    }
    strncpy(sname, sockname, UNIX_PATH_MAX);
    
    if((sfd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
        errno = ECONNREFUSED;
        return -1;
    }
    
    struct timespec ts;
    ts.tv_sec = msec/1000;
    ts.tv_nsec = (msec%1000)*1000000;
    
    struct sockaddr_un sa;
    sa.sun_family = AF_UNIX;
    strncpy(sa.sun_path, sockname, UNIX_PATH_MAX);
    sa.sun_path[UNIX_PATH_MAX-1] = '\0';
    
    struct timespec start, end;
    clock_gettime(CLOCK_REALTIME, &start);
    while(connect(sfd, (struct sockaddr*) &sa, sizeof(sa)) == -1) {
        if(errno == ENOENT) {
            clock_gettime(CLOCK_REALTIME, &end);
            if(end.tv_sec - start.tv_sec < abstime.tv_sec) {
                nanosleep(&ts, NULL);
            } else if(end.tv_sec - start.tv_sec == abstime.tv_sec) {
                if(end.tv_nsec - start.tv_nsec < 0) {
                    if(start.tv_nsec - end.tv_nsec > abstime.tv_nsec) {
                        errno = ETIME;
                        return -1;
                    }
                } else {
                    if(end.tv_nsec - start.tv_nsec > abstime.tv_nsec) {
                        errno = ETIME;
                        return -1;
                    }
                }
                nanosleep(&ts, NULL);
            } else {
                errno = ETIME;
                return -1;
            }
        } else {
            errno = ECONNREFUSED;
            return -1;
        }
    }
    
    // Alloca memoria per il buffer
    fsp_buf_size = FSP_API_BUF_DEF_SIZE;
    if((fsp_buf = malloc(fsp_buf_size)) == NULL) {
        errno = ENOBUFS;
        return -1;
    }
    
    struct fsp_response resp;
    if(receiveFspResp(&resp) != 0 || resp.code == 421) {
        free(fsp_buf);
        fsp_buf = NULL;
        errno = ECONNREFUSED;
        return -1;
    }
    if(resp.code != 220) {
        free(fsp_buf);
        fsp_buf = NULL;
        errno = EBADMSG;
        return -1;
    }

    return 0;
}

int closeConnection(const char* sockname) {
    if(sockname == NULL || strcmp(sockname, sname) != 0) {
        errno = EINVAL;
        return -1;
    }
    
    if(sendFspReq(QUIT, NULL, 0, NULL) != 0) {
        return -1;
    }
    
    struct fsp_response resp;
    if(receiveFspResp(&resp) != 0) {
        if(errno == ENOENT || errno == ENOMEM || errno == EPERM || errno == EEXIST) errno = EBADMSG;
        return -1;
    }
    if(resp.code != 221) {
        errno = EBADMSG;
        return -1;
    }
    
    close(sfd);
    sfd = -1;
    sname[0] = '\0';
    free(fsp_buf);
    fsp_buf = NULL;
    fsp_buf_size = 0;
    
    return 0;
}

int openFile(const char* pathname, int flags, const char* dirname) {
    if(pathname == NULL) {
        errno = EINVAL;
        return -1;
    }
    if(dirname != NULL) {
        // Controlla se dirname è una directory
        struct stat info;
        if(stat(dirname, &info) == 0) {
            if(!S_ISDIR(info.st_mode)) {
                errno = EINVAL;
                return -1;
            }
        } else {
            // Errore stat
            errno = EINVAL;
            return -1;
        }
    }
    
    enum fsp_command cmd;
    switch(flags) {
        case O_DEFAULT:
            cmd = OPEN;
            break;
        case O_CREATE:
            cmd = OPENC;
            break;
        case O_LOCK:
            cmd = OPENL;
            break;
        case O_CREATE | O_LOCK:
            cmd = OPENCL;
            break;
        default:
            errno = EINVAL;
            return -1;
    }
    
    if(sendFspReq(cmd, pathname, 0, NULL) != 0) {
        return -1;
    }
    
    struct fsp_response resp;
    if(receiveFspResp(&resp) != 0) {
        if(errno == EPERM) errno = EBADMSG;
        return -1;
    }
    if(resp.code != 200) {
        errno = EBADMSG;
        return -1;
    }
    
    if(resp.data_len > 0 && dirname != NULL) {
        return saveData(dirname, resp.data_len, resp.data) >= 0 ? 0 : -1;
    }
    
    return 0;
}

int readFile(const char* pathname, void** buf, size_t* size) {
    if(pathname == NULL || buf == NULL || size == NULL) {
        errno = EINVAL;
        return -1;
    }
    
    if(sendFspReq(READ, pathname, 0, NULL) != 0) {
        return -1;
    }
    struct fsp_response resp;
    if(receiveFspResp(&resp) != 0) {
        if(errno == ENOMEM || errno == EEXIST) errno = EBADMSG;
        return -1;
    }
    if(resp.code != 200) {
        errno = EBADMSG;
        return -1;
    }
    
    // Legge i dati
    struct fsp_data* parsed_data = NULL;
    switch (fsp_parser_parseData(resp.data_len, resp.data, &parsed_data)) {
        case -1:
            // data_len <= 0 || data == NULL || parsed_data == NULL
        case -2:
            // Il campo data del messaggio è incompleto
        case -3:
            // Il campo data del messaggio contiene errori sintattici
            errno = EBADMSG;
            return -1;
        case -4:
            // Impossibile allocare memoria nello heap
            errno = ENOBUFS;
            return -1;
        default:
            // Successo
            break;
    }
    if(parsed_data->next != NULL) {
        fsp_parser_freeData(parsed_data);
        errno = EBADMSG;
        return -1;
    }
    
    if((*buf = malloc(parsed_data->size)) == NULL) {
        fsp_parser_freeData(parsed_data);
        errno = ENOBUFS;
        return -1;
    }
    memcpy(*buf, parsed_data->data, parsed_data->size);
    *size = parsed_data->size;
    
    fsp_parser_freeData(parsed_data);
    
    return 0;
}

int readNFiles(int N, const char* dirname) {
    if(dirname == NULL) {
        errno = EINVAL;
        return -1;
    }
    // Controlla se dirname è una directory
    struct stat info;
    if(stat(dirname, &info) == 0) {
        if(!S_ISDIR(info.st_mode)) {
            errno = EINVAL;
            return -1;
        }
    } else {
        // Errore stat
        errno = EINVAL;
        return -1;
    }
    
    // N_str
    char N_str[12];
    snprintf(N_str, 12, "%d", N);
    
    if(sendFspReq(READN, N_str, 0, NULL) != 0) {
        return -1;
    }
    struct fsp_response resp;
    if(receiveFspResp(&resp) != 0) {
        if(errno == ENOENT || errno == ENOMEM || errno == EPERM || errno == EEXIST || errno == ECANCELED) errno = EBADMSG;
        return -1;
    }
    
    if(resp.code != 200) {
        errno = EBADMSG;
        return -1;
    }
    
    // Salva i dati ricevuti
    if(resp.data_len > 0) {
        return saveData(dirname, resp.data_len, resp.data);
    }
    
    return 0;
}

int writeFile(const char* pathname, const char* dirname) {
    if(pathname == NULL) {
        errno = EINVAL;
        return -1;
    }
    if(dirname != NULL) {
        // Controlla se dirname è una directory
        struct stat info;
        if(stat(dirname, &info) == 0) {
            if(!S_ISDIR(info.st_mode)) {
                errno = EINVAL;
                return -1;
            }
        } else {
            // Errore stat
            errno = EINVAL;
            return -1;
        }
    }
    
    FILE* file;
    void* buf;
    size_t buf_size;
    
    struct stat info;
    if(stat(pathname, &info) == 0) {
        if(!S_ISREG(info.st_mode)) {
            errno = EINVAL;
            return -1;
        }
        buf_size = info.st_size;
    } else {
        // Errore stat
        errno = EINVAL;
        return -1;
    }
    
    // Alloca il buffer
    if((buf = malloc(buf_size)) == NULL) {
        errno = ENOBUFS;
        return -1;
    }
    
    // Copia nel buffer il contenuto del file
    if((file = fopen(pathname, "rb")) != NULL) {
        size_t bytes;
        bytes = fread(buf, 1, buf_size, file);
        if(bytes != buf_size) {
            errno = EIO;
            return -1;
        }
    } else {
        errno = EIO;
        return -1;
    }
    fclose(file);
    
    // Campo data
    void* data_buf;
    size_t data_buf_size = strlen(pathname) + buf_size + 64;
    if((data_buf = malloc(data_buf_size)) == NULL) {
        errno = ENOBUFS;
        return -1;
    }
    long int bytes;
    switch(bytes = fsp_parser_makeData(&data_buf, &data_buf_size, 0, pathname, buf_size, buf)) {
        case -1:
            // data_buf_size > FSP_PARSER_BUF_MAX_SIZE
        case -2:
            // Non è stato possibile riallocare la memoria per *buf
            free(buf);
            free(data_buf);
            errno = ENOBUFS;
            return -1;
        default:
            // Successo
            break;
    }
    
    if(sendFspReq(WRITE, pathname, bytes, data_buf) != 0) {
        int err = errno;
        free(buf);
        free(data_buf);
        errno = err;
        return -1;
    }
    free(buf);
    free(data_buf);
    
    struct fsp_response resp;
    if(receiveFspResp(&resp) != 0) {
        if(errno == EEXIST) errno = EBADMSG;
        return -1;
    }
    if(resp.code != 200) {
        errno = EBADMSG;
        return -1;
    }
    
    if(resp.data_len > 0 && dirname != NULL) {
        return saveData(dirname, resp.data_len, resp.data) >= 0 ? 0 : -1;
    }
    
    return 0;
}

int appendToFile(const char* pathname, void* buf, size_t size, const char* dirname) {
    if(pathname == NULL || buf == NULL || size < 0) {
        errno = EINVAL;
        return -1;
    }
    if(dirname != NULL) {
        // Controlla se dirname è una directory
        struct stat info;
        if(stat(dirname, &info) == 0) {
            if(!S_ISDIR(info.st_mode)) {
                errno = EINVAL;
                return -1;
            }
        } else {
            // Errore stat
            errno = EINVAL;
            return -1;
        }
    }
    
    // Campo data
    void* data_buf;
    size_t data_buf_size = strlen(pathname) + size + 64;
    if((data_buf = malloc(data_buf_size)) == NULL) {
        errno = ENOBUFS;
        return -1;
    }
    long int bytes;
    switch(bytes = fsp_parser_makeData(&data_buf, &data_buf_size, 0, pathname, size, buf)) {
        case -1:
            // data_buf_size > FSP_PARSER_BUF_MAX_SIZE
        case -2:
            // Non è stato possibile riallocare la memoria per *buf
            free(data_buf);
            errno = ENOBUFS;
            return -1;
        default:
            // Successo
            break;
    }
    
    if(sendFspReq(APPEND, pathname, bytes, data_buf) != 0) {
        return -1;
    }
    free(data_buf);
    
    struct fsp_response resp;
    if(receiveFspResp(&resp) != 0) {
        if(errno == EEXIST) errno = EBADMSG;
        return -1;
    }
    if(resp.code != 200) {
        errno = EBADMSG;
        return -1;
    }
    
    // Controlla se ci sono dati espulsi dal server
    if(resp.data_len > 0 && dirname != NULL) {
        return saveData(dirname, resp.data_len, resp.data) >= 0 ? 0 : -1;
    }
    
    return 0;
}

int lockFile(const char* pathname) {
    if(pathname == NULL) {
        errno = EINVAL;
        return -1;
    }
    
    if(sendFspReq(LOCK, pathname, 0, NULL) != 0) {
        return -1;
    }
    
    struct fsp_response resp;
    if(receiveFspResp(&resp) != 0) {
        if(errno == ENOMEM || errno == EPERM || errno == EEXIST) errno = EBADMSG;
        return -1;
    }
    if(resp.code != 200) {
        errno = EBADMSG;
        return -1;
    }
    
    return 0;
}

int unlockFile(const char* pathname) {
    if(pathname == NULL) {
        errno = EINVAL;
        return -1;
    }
    
    if(sendFspReq(UNLOCK, pathname, 0, NULL) != 0) {
        return -1;
    }
    
    struct fsp_response resp;
    if(receiveFspResp(&resp) != 0) {
        if(errno == ENOMEM || errno == EEXIST) errno = EBADMSG;
        return -1;
    }
    if(resp.code != 200) {
        errno = EBADMSG;
        return -1;
    }
    
    return 0;
}

int closeFile(const char* pathname) {
    if(pathname == NULL) {
        errno = EINVAL;
        return -1;
    }
    
    if(sendFspReq(CLOSE, pathname, 0, NULL) != 0) {
        return -1;
    }
    
    struct fsp_response resp;
    if(receiveFspResp(&resp) != 0) {
        if(errno == ENOMEM || errno == EEXIST || errno == EPERM) errno = EBADMSG;
        return -1;
    }
    if(resp.code != 200) {
        errno = EBADMSG;
        return -1;
    }
    
    return 0;
}

int removeFile(const char* pathname) {
    if(pathname == NULL) {
        errno = EINVAL;
        return -1;
    }
    
    if(sendFspReq(REMOVE, pathname, 0, NULL) != 0) {
        return -1;
    }
    
    struct fsp_response resp;
    if(receiveFspResp(&resp) != 0) {
        if(errno == ENOMEM || errno == EEXIST) errno = EBADMSG;
        return -1;
    }
    if(resp.code != 200) {
        errno = EBADMSG;
        return -1;
    }
    
    return 0;
}

static int sendFspReq(enum fsp_command cmd, const char* pathname, size_t data_len, void* data) {
    // Genera il messaggio di richiesta
    long int bytes;
    switch(bytes = fsp_parser_makeRequest(&fsp_buf, &fsp_buf_size, cmd, pathname, data_len, data)) {
        case -1:
            // fsp_buf == NULL || *fsp_buf_size > FSP_PARSER_BUF_MAX_SIZE
            // Se fsp_buf == NULL, allora openConnection non è stata invocata
            errno = fsp_buf == NULL ? ENOTCONN : ENOBUFS;
            return -1;
        case -2:
            // Non è stato possibile riallocare la memoria per *fsp_buf
            errno = ENOBUFS;
            return -1;
        default:
            // Successo
            break;
    }
    
    // Invia il messaggio di richiesta
    char* _buf = (char*) fsp_buf;
    ssize_t w_bytes;
    while(bytes > 0) {
        if((w_bytes = write(sfd, _buf, bytes)) == -1) {
            // Errore durante la scrittura
            errno = EIO;
            return -1;
        } else {
            bytes -= w_bytes;
            _buf += w_bytes;
        }
    }
    
    return 0;
}

static int receiveFspResp(struct fsp_response* resp) {
    struct fsp_response _resp;
    
    switch (fsp_reader_readResponse(sfd, &fsp_buf, &fsp_buf_size, &_resp)) {
        case -1:
            // fsp_buf == NULL || *size > FSP_READER_BUF_MAX_SIZE
            errno = fsp_buf == NULL ? ENOTCONN : ENOBUFS;
            return -1;
        case -2:
            // Errori durante la lettura
            errno = EIO;
            return -1;
        case -3:
            // sfd ha raggiunto EOF senza aver letto un messaggio di risposta
            close(sfd);
            sfd = -1;
            sname[0] = '\0';
            free(fsp_buf);
            fsp_buf = NULL;
            fsp_buf_size = 0;
            errno = ECONNABORTED;
            return -1;
        case -4:
            // Il messaggio di risposta contiene errori sintattici
            errno = EBADMSG;
            return -1;
        case -5:
            // Impossibile riallocare il buffer (memoria insufficiente)
            errno = ENOBUFS;
            return -1;
        default:
            // Successo
            break;
    }
    
    switch (_resp.code) {
        case 200:
        case 220:
        case 221:
            // Successo
            break;
        case 421:
            // Servizio non disponibile, chiusura della connessione
            close(sfd);
            sfd = -1;
            sname[0] = '\0';
            free(fsp_buf);
            fsp_buf = NULL;
            fsp_buf_size = 0;
            errno = ECONNABORTED;
            return -1;
        case 501:
            // Errore di sintassi nel messaggio di richiesta
            errno = EBADMSG;
            return -1;
        case 550:
            // Il file non è presente sul server
            errno = ENOENT;
            return -1;
        case 552:
            // Non c'è sufficiente memoria sul server per eseguire l'operazione
            errno = ENOMEM;
            return -1;
        case 554:
            // Operazione non consentita
            errno = EPERM;
            return -1;
        case 555:
            // File già memorizzato nel server
            errno = EEXIST;
            return -1;
        case 556:
            // Impossibile eseguire l'operazione
            errno = ECANCELED;
            return -1;
        default:
            errno = EBADMSG;
            return -1;
    }
    
    if(resp != NULL) {
        *resp = _resp;
    }
    
    return 0;
}

static int saveData(const char* dirname, size_t data_len, void* data) {
    // Legge i dati
    struct fsp_data* parsed_data = NULL;
    switch (fsp_parser_parseData(data_len, data, &parsed_data)) {
        case -1:
            // data_len <= 0 || data == NULL || parsed_data == NULL
        case -2:
            // Il campo data del messaggio è incompleto
        case -3:
            // Il campo data del messaggio contiene errori sintattici
            errno = EBADMSG;
            return -1;
        case -4:
            // Impossibile allocare memoria nello heap
            errno = ENOBUFS;
            return -1;
        default:
            // Successo
            break;
    }
    
    // Salva in dirname i file
    struct fsp_data* _parsed_data = parsed_data;
    int data_num = 0;
    
    while(_parsed_data != NULL) {
        FILE* file;
        char* pathname = _parsed_data->pathname;
        unsigned int dirname_len = (unsigned int) strlen(dirname);
        unsigned int pathname_len = (unsigned int) strlen(pathname);
        
        // Modifica il nome del file per la scrittura nella directory dirname
        // Il primo carattere deve essere '/' in quanto il file è identificato
        // univocamente dal suo path assoluto
        assert(*pathname == '/');
        char* p = pathname+1;
        while(*p != '\0') {
            if(*p == '/') {
                *p = '_';
            }
            p++;
        }
        
        // Se il nome della directory termina con '/', allora non lo considera
        if(dirname[dirname_len-1] == '/') dirname_len--;
        
        // Concatena il path della directory con il nuovo nome del file
        char* filename;
        if((filename = calloc(dirname_len+pathname_len+1, sizeof(char))) == NULL) {
            fsp_parser_freeData(parsed_data);
            errno = ENOBUFS;
            return -1;
        }
        memcpy(filename, dirname, dirname_len);
        memcpy(filename+dirname_len, pathname, pathname_len);
        filename[dirname_len+pathname_len] = '\0';
        
        if((file = fopen(filename, "wb")) != NULL) {
            size_t bytes;
            bytes = fwrite(_parsed_data->data, 1, _parsed_data->size, file);
            if(bytes != _parsed_data->size) {
                fsp_parser_freeData(parsed_data);
                fclose(file);
                free(filename);
                errno = EIO;
                return -1;
            }
        } else {
            fsp_parser_freeData(parsed_data);
            free(filename);
            errno = EIO;
            return -1;
        }
        if(fclose(file) == EOF) {
            fsp_parser_freeData(parsed_data);
            free(filename);
            errno = EIO;
            return -1;
        }
        free(filename);
        
        data_num++;
        _parsed_data = _parsed_data->next;
    }
    fsp_parser_freeData(parsed_data);
    
    return data_num;
}
