/*
 * Autore: Francesco Gallicchio
 * Matricola: 579131
 */

#include <fsp_parser.h>
#include <utils.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

int fsp_parser_parseRequest(void* buf, size_t size, struct fsp_request* req) {
    if(buf == NULL || req == NULL) {
        return -1;
    }
    
    char* buf_start = (char*) buf;
    char *start, *end;
    
    char *pos1, *pos2;
    
    // Legge il comando
    start = buf_start;
    end = start;
    while(end - buf_start < size && *end != ' ') end++;
    if(*end == ' ') {
        *end = '\0';
        if(strcmp(start, "APPEND") == 0) {
            req->cmd = APPEND;
        } else if(strcmp(start, "CLOSE") == 0) {
            req->cmd = CLOSE;
        } else if(strcmp(start, "LOCK") == 0) {
            req->cmd = LOCK;
        } else if(strcmp(start, "OPEN") == 0) {
            req->cmd = OPEN;
        } else if(strcmp(start, "OPENC") == 0) {
            req->cmd = OPENC;
        } else if(strcmp(start, "OPENCL") == 0) {
            req->cmd = OPENCL;
        } else if(strcmp(start, "OPENL") == 0) {
            req->cmd = OPENL;
        } else if(strcmp(start, "QUIT") == 0) {
            req->cmd = QUIT;
        } else if(strcmp(start, "READ") == 0) {
            req->cmd = READ;
        } else if(strcmp(start, "READN") == 0) {
            req->cmd = READN;
        } else if(strcmp(start, "REMOVE") == 0) {
            req->cmd = REMOVE;
        } else if(strcmp(start, "UNLOCK") == 0) {
            req->cmd = UNLOCK;
        } else if(strcmp(start, "WRITE") == 0) {
            req->cmd = WRITE;
        } else {
            *end = ' ';
            return -3;
        }
    } else {
        return -2;
    }
    
    // Legge l'argomento
    start = end + 1;
    end = start;
    while(end - buf_start < size && *end != '\r') end++;
    if(*end == '\r') {
        pos1 = end;
        req->arg = start;
    } else {
        return -2;
    }
    if(req->cmd == QUIT && *(req->arg) != '\r') {
        return -3;
    }
    end++;
    if((end - buf_start) == size) {
        return -2;
    } else if(*end != '\n') {
        return -3;
    }
    
    // Legge la lunghezza dei dati
    start = end + 1;
    end = start;
    while(end - buf_start < size && *end != ' ') end++;
    if(*end == ' ') {
        *end = '\0';
        long int data_len;
        if(!isNumber(start, &data_len) || data_len < 0) {
            *end = ' ';
            return -3;
        }
        req->data_len = data_len;
        *end = ' ';
    } else {
        return -2;
    }
    
    // Legge i dati
    start = end + 1;
    end = start + req->data_len;
    if(end - buf_start < size) {
        if(*end == '\r') {
            pos2 = end;
            req->data = (void*) start;
        } else {
            return -3;
        }
    } else {
        return -2;
    }
    end++;
    if((end - buf_start) < size) {
        if(*end != '\n') {
            return -3;
        }
    } else {
        return -2;
    }
    end++;
    if((end - buf_start) != size) {
        return -3;
    }
    
    *pos1 = '\0';
    *pos2 = '\0';
    
    return 0;
}

long int fsp_parser_makeRequest(void** buf, size_t* size, enum fsp_command cmd, const char* arg, size_t data_len, void* data) {
    if(buf == NULL || *buf == NULL || size == NULL || data_len < 0 || (data_len > 0 && data == NULL) || *size > FSP_PARSER_BUF_MAX_SIZE) {
        return -1;
    }
    
    // cmd
    char* _cmd;
    switch(cmd) {
        case APPEND:
            _cmd = "APPEND";
            break;
        case CLOSE:
            _cmd = "CLOSE";
            break;
        case LOCK:
            _cmd = "LOCK";
            break;
        case OPEN:
            _cmd = "OPEN";
            break;
        case OPENC:
            _cmd = "OPENC";
            break;
        case OPENCL:
            _cmd = "OPENCL";
            break;
        case OPENL:
            _cmd = "OPENL";
            break;
        case QUIT:
            _cmd = "QUIT";
            break;
        case READ:
            _cmd = "READ";
            break;
        case READN:
            _cmd = "READN";
            break;
        case REMOVE:
            _cmd = "REMOVE";
            break;
        case UNLOCK:
            _cmd = "UNLOCK";
            break;
        case WRITE:
            _cmd = "WRITE";
            break;
        default:
            return -1;
    }
    
    // data_len
    char data_len_str[12];
    sprintf(data_len_str, "%ld", data_len);
    
    // Determina le lunghezze delle singole stringhe
    size_t cmd_len, arg_len, data_len_str_len, tot_len;
    cmd_len = strlen(_cmd);
    arg_len = arg == NULL ? 0 : strlen(arg);
    data_len_str_len = strlen(data_len_str);
    tot_len = cmd_len + arg_len + data_len_str_len + data_len + 6;
    
    // In C99 sizeof(char) dovrebbe essere sempre pari a 1
    assert(sizeof(char) == 1);
    
    // Rialloca il buffer per contenere l'intero messaggio se necessario
    if(*size < tot_len) {
        void* buf_tmp;
        if(tot_len > FSP_PARSER_BUF_MAX_SIZE || (buf_tmp = realloc(*buf, tot_len)) == NULL) {
            return -2;
        } else {
            *buf = buf_tmp;
            *size = tot_len;
        }
    }
    
    // Scrive nel buffer
    char* _buf = (char*) (*buf);
    memcpy(_buf, _cmd, cmd_len);
    _buf += cmd_len;
    *_buf = ' ';
    _buf++;
    if(arg != NULL) {
        memcpy(_buf, arg, arg_len);
    }
    _buf += arg_len;
    memcpy(_buf, "\r\n", 2);
    _buf += 2;
    memcpy(_buf, data_len_str, data_len_str_len);
    _buf += data_len_str_len;
    *_buf = ' ';
    _buf++;
    if(data != NULL) memcpy(_buf, data, data_len);
    _buf += data_len;
    memcpy(_buf, "\r\n", 2);
    
    return tot_len;
}

int fsp_parser_parseResponse(void* buf, size_t size, struct fsp_response* resp) {
    if(buf == NULL || resp == NULL) {
        return -1;
    }
    
    char* buf_start = (char*) buf;
    char *start, *end;
    
    char* pos1, *pos2;
    
    // Legge il codice
    start = buf_start;
    end = start;
    while(end - buf_start < size && *end != ' ') end++;
    if(*end == ' ') {
        *end = '\0';
        long int code;
        if(!isNumber(start, &code)) {
            *end = ' ';
            return -3;
        } else {
            resp->code = (int) code;
        }
        *end = ' ';
    } else {
        return -2;
    }
    
    // Legge la descrizione
    start = end + 1;
    end = start;
    while(end - buf_start < size && *end != '\r') end++;
    if(*end == '\r') {
        pos1 = end;
        resp->description = start;
    } else {
        return -2;
    }
    end++;
    if((end - buf_start) == size) {
        return -2;
    } else if(*end != '\n') {
        return -3;
    }
    
    // Legge la lunghezza dei dati
    start = end + 1;
    end = start;
    while(end - buf_start < size && *end != ' ') end++;
    if(*end == ' ') {
        *end = '\0';
        long int data_len;
        if(!isNumber(start, &data_len) || data_len < 0) {
            *end = ' ';
            return -3;
        }
        resp->data_len = data_len;
        *end = ' ';
    } else {
        return -2;
    }
    
    // Legge i dati
    start = end + 1;
    end = start + resp->data_len;
    if(end - buf_start < size) {
        if(*end == '\r') {
            pos2 = end;
            resp->data = (void*) start;
        } else {
            return -3;
        }
    } else {
        return -2;
    }
    end++;
    if((end - buf_start) < size) {
        if(*end != '\n') {
            return -3;
        }
    } else {
        return -2;
    }
    end++;
    if((end - buf_start) != size) {
        return -3;
    }
    
    *pos1 = '\0';
    *pos2 = '\0';
    
    return 0;
}

long int fsp_parser_makeResponse(void** buf, size_t* size, int code, const char* description, size_t data_len, void* data) {
    if(buf == NULL || *buf == NULL || size == NULL || data_len < 0 || (data_len > 0 && data == NULL) || *size > FSP_PARSER_BUF_MAX_SIZE) {
        return -1;
    }
    
    // code
    char code_str[12];
    sprintf(code_str, "%d", code);
    
    // data_len
    char data_len_str[12];
    sprintf(data_len_str, "%ld", data_len);
    
    // Determina le lunghezze delle singole stringhe
    size_t code_len, descr_len, data_len_str_len, tot_len;
    code_len = strlen(code_str);
    descr_len = strlen(description);
    data_len_str_len = strlen(data_len_str);
    tot_len = code_len + descr_len + data_len_str_len + data_len + 6;
    
    // In C99 sizeof(char) dovrebbe essere sempre pari a 1
    assert(sizeof(char) == 1);
    
    // Rialloca il buffer per contenere l'intero messaggio se necessario
    if(*size < tot_len) {
        void* buf_tmp;
        if(tot_len > FSP_PARSER_BUF_MAX_SIZE || (buf_tmp = realloc(*buf, tot_len)) == NULL) {
            return -2;
        } else {
            *buf = buf_tmp;
            *size = tot_len;
        }
    }
    
    // Scrive nel buffer
    char* _buf = (char*) (*buf);
    memcpy(_buf, code_str, code_len);
    _buf += code_len;
    *_buf = ' ';
    _buf++;
    memcpy(_buf, description, descr_len);
    _buf += descr_len;
    memcpy(_buf, "\r\n", 2);
    _buf += 2;
    memcpy(_buf, data_len_str, data_len_str_len);
    _buf += data_len_str_len;
    *_buf = ' ';
    _buf++;
    if(data != NULL) memcpy(_buf, data, data_len);
    _buf += data_len;
    memcpy(_buf, "\r\n", 2);
    
    return tot_len;
}

int fsp_parser_parseData(size_t data_len, void* data, struct fsp_data** parsed_data) {
    if(data_len <= 0 || data == NULL || parsed_data == NULL) {
        return -1;
    }
    
    *parsed_data = NULL;
    
    char* buf_start = (char*) data;
    char *start, *end;
    
    start = buf_start;
    end = start;
    
    // Crea il primo nodo della lista
    if((*parsed_data = malloc(sizeof(struct fsp_data))) == NULL) {
        return -4;
    }
    struct fsp_data* _parsed_data = *parsed_data;
    _parsed_data->next = NULL;
    
    while(end - buf_start != data_len) {
        // Legge il pathname
        while(end - buf_start < data_len && *end != ' ') end++;
        if(*end == ' ') {
            *end = '\0';
            _parsed_data->pathname = start;
        } else {
            fsp_parser_freeData(*parsed_data);
            *parsed_data = NULL;
            return -2;
        }
        
        // Legge la dimensione
        start = end + 1;
        end = start;
        while(end - buf_start < data_len && *end != ' ') end++;
        if(*end == ' ') {
            *end = '\0';
            long int size;
            if(!isNumber(start, &size) || size <= 0) {
                fsp_parser_freeData(*parsed_data);
                *parsed_data = NULL;
                return -3;
            }
            _parsed_data->size = (size_t) size;
        } else {
            fsp_parser_freeData(*parsed_data);
            *parsed_data = NULL;
            return -2;
        }
        
        // Legge il dato
        start = end + 1;
        end = start + _parsed_data->size;
        if(end - buf_start < data_len) {
            if(*end == ' ') {
                *end = '\0';
                _parsed_data->data = (void*) start;
            } else {
                fsp_parser_freeData(*parsed_data);
                *parsed_data = NULL;
                return -3;
            }
        } else {
            fsp_parser_freeData(*parsed_data);
            *parsed_data = NULL;
            return -2;
        }
        
        start = end + 1;
        end = start;
        if((end - buf_start) == data_len) {
            break;
        } else if((end - buf_start) > data_len) {
            fsp_parser_freeData(*parsed_data);
            *parsed_data = NULL;
            return -3;
        } else {
            // Crea il prossimo nodo
            struct fsp_data* newData = NULL;
            if((newData = malloc(sizeof(struct fsp_data))) == NULL) {
                fsp_parser_freeData(*parsed_data);
                *parsed_data = NULL;
                return -4;
            }
            newData->next = NULL;
            _parsed_data->next = newData;
            _parsed_data = newData;
        }
    }
    
    return 0;
}

long int fsp_parser_makeData(void** buf, size_t* size, unsigned long int offset, const char* pathname, size_t data_size, void* data) {
    if(buf == NULL || *buf == NULL || size == NULL || offset < 0 || pathname == NULL || data_size < 0 || (data_size > 0 && data == NULL) || *size > FSP_PARSER_BUF_MAX_SIZE || data == NULL) {
        return -1;
    }
    
    // data_size
    char data_size_str[12];
    sprintf(data_size_str, "%ld", data_size);
    
    long int pathname_len, data_size_str_len, tot_len;
    pathname_len = strlen(pathname);
    data_size_str_len = strlen(data_size_str);
    tot_len = pathname_len + data_size_str_len + data_size + 3;
    
    // In C99 sizeof(char) dovrebbe essere sempre pari a 1
    assert(sizeof(char) == 1);
    
    // Rialloca il buffer per contenere l'intero messaggio se necessario
    unsigned long int remaining = *size - offset;
    if(remaining < tot_len) {
        void* buf_tmp;
        size_t buf_tmp_size = *size + tot_len - remaining;
        if(buf_tmp_size > FSP_PARSER_BUF_MAX_SIZE || (buf_tmp = realloc(*buf, buf_tmp_size)) == NULL) {
            return -2;
        } else {
            *buf = buf_tmp;
            *size = buf_tmp_size;
        }
    }
    
    // Scrive nel buffer
    char* _buf = (char*) (*buf);
    _buf += offset;
    
    memcpy(_buf, pathname, pathname_len);
    _buf += pathname_len;
    *_buf = ' ';
    _buf++;
    memcpy(_buf, data_size_str, data_size_str_len);
    _buf += data_size_str_len;
    *_buf = ' ';
    _buf++;
    memcpy(_buf, data, data_size);
    _buf += data_size;
    *_buf = ' ';
    
    return tot_len;
}

void fsp_parser_freeData(struct fsp_data* parsed_data) {
    struct fsp_data* _parsed_data;
    while(parsed_data != NULL) {
        _parsed_data = parsed_data->next;
        free(parsed_data);
        parsed_data = _parsed_data;
    }
}
