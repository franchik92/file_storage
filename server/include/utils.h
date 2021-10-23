/*
 * Autore: Francesco Gallicchio
 * Matricola: 579131
 */

#ifndef UTILS_H
#define UTILS_H

#include <stdlib.h>
#include <errno.h>

/**
 * \brief Controlla se la stringa str è un numero intero e lo salva in *n in caso lo fosse.
 * \return 1 in caso di successo,
 *         0 se str == NULL || n == NULL (setta errno a EINVAL), se str non è
 *         un numero intero, o in caso di overflow/underflow (setta errno a ERANGE).
 */
static inline int isNumber(const char* str, long int* n) {
    if (str == NULL || n == NULL) {
        errno = EINVAL;
        return 0;
    }
    char* endptr = NULL;
    errno=0;
    long int val = strtol(str, &endptr, 10);
    if (errno == ERANGE) return 0;
    if (*str != '\0' && *endptr == '\0') {
        *n = val;
        return 1;
    }
    return 0;
}

#endif
