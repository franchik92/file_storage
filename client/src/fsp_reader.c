/*
 * Autore: Francesco Gallicchio
 * Matricola: 579131
 */

#include <fsp_reader.h>

#include <stdlib.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>

int fsp_reader_readRequest(int sfd, void** buf, size_t* size, struct fsp_request* req) {
    if(buf == NULL || *buf == NULL || size == NULL || req == NULL || *size > FSP_READER_BUF_MAX_SIZE) {
        return -1;
    }
    
    // Buffer
    char* _buf = (char*) *buf;
    // Numero totale dei byte letti
    ssize_t bytes = 0;
    // Valore di ritorno della funzione read
    ssize_t ret_val;
    // Spazio disponibile del buffer
    size_t len = *size;
    
    while((ret_val = read(sfd, _buf+bytes, len)) > 0) {
        bytes += ret_val;
        len -= ret_val;
        
        // Controlla se il messaggio termina con "\r\n"
        if(bytes >= 2 && _buf[bytes-2] == '\r' && _buf[bytes-1] == '\n') {
            // Controlla se il messaggio contiene la prima riga che termina con "\r\n"
            int r_pos = 0;
            while(_buf[r_pos] != '\r' && _buf[r_pos+1] != '\n') r_pos++;
            if(r_pos != bytes-2) {
                // parsa la stringa
                switch (fsp_parser_parseRequest(_buf, bytes, req)) {
                    case -1:
                        // _buf == NULL || resp == NULL
                        return -1;
                    case -2:
                        // Messaggio incompleto
                        break;
                    case -3:
                        // Il messaggio contiene errori sintattici
                        return -4;
                    default:
                        // Successo
                        return 0;
                }
            }
        }
        // rialloca la memoria se insufficiente
        if(len == 0) {
            if(*size == FSP_READER_BUF_MAX_SIZE) {
                return -5;
            }
            size_t _size = (*size)*2 < FSP_READER_BUF_MAX_SIZE ? (*size)*2 : FSP_READER_BUF_MAX_SIZE;
            char* buf_tmp;
            if((buf_tmp = realloc(_buf, _size)) == NULL) {
                return -5;
            } else {
                _buf = buf_tmp;
            }
            len = (*size);
            (*size) = _size;
        }
    }
    // Se ret_val == 0, allora sfd ha raggiunto EOF,
    // altrimenti c'è stato un errore di lettura (ret_val == -1)
    return ret_val == 0 ? -3 : -2;
}

int fsp_reader_readResponse(int sfd, void** buf, size_t* size, struct fsp_response* resp) {
    if(buf == NULL || *buf == NULL || size == NULL || resp == NULL || *size > FSP_READER_BUF_MAX_SIZE) {
        return -1;
    }
    
    // Buffer
    char* _buf = (char*) *buf;
    // Numero totale dei byte letti
    ssize_t bytes = 0;
    // Valore di ritorno della funzione read
    ssize_t ret_val;
    // Spazio disponibile del buffer
    size_t len = *size;
    
    while((ret_val = read(sfd, _buf+bytes, len)) > 0) {
        bytes += ret_val;
        len -= ret_val;
        
        // Controlla se il messaggio termina con "\r\n"
        if(bytes >= 2 && _buf[bytes-2] == '\r' && _buf[bytes-1] == '\n') {
            // Controlla se il messaggio contiene la prima riga che termina con "\r\n"
            int r_pos = 0;
            while(_buf[r_pos] != '\r' && _buf[r_pos+1] != '\n') r_pos++;
            if(r_pos != bytes-2) {
                // parsa la stringa
                switch (fsp_parser_parseResponse(_buf, bytes, resp)) {
                    case -1:
                        // _buf == NULL || resp == NULL
                        return -1;
                    case -2:
                        // Messaggio incompleto
                        break;
                    case -3:
                        // Il messaggio contiene errori sintattici
                        return -4;
                    default:
                        // Successo
                        return 0;
                }
            }
        }
        // rialloca la memoria se insufficiente
        if(len == 0) {
            if(*size == FSP_READER_BUF_MAX_SIZE) {
                return -5;
            }
            size_t _size = (*size)*2 < FSP_READER_BUF_MAX_SIZE ? (*size)*2 : FSP_READER_BUF_MAX_SIZE;
            
            char* buf_tmp;
            if((buf_tmp = realloc(_buf, _size)) == NULL) {
                return -5;
            } else {
                _buf = buf_tmp;
            }
            len = (*size);
            (*size) = _size;
        }
    }
    // Se ret_val == 0, allora sfd ha raggiunto EOF,
    // altrimenti c'è stato un errore di lettura (ret_val == -1)
    return ret_val == 0 ? -3 : -2;
}
