/*
 * Autore: Francesco Gallicchio
 * Matricola: 579131
 *
 * API messa a disposizione per i client che intendono connettersi
 * a un file storage server mediante il protocollo FSP (File Storage Protocol).
 *
 * È necessario aprire la connessione con la funzione openConnection prima
 * di invocare una qualsiasi altra funzione.
 * Una volta aperta la connessione con successo, fa uso di un buffer per la generazione e la
 * ricezione dei messaggi fsp.
 * Non può essere aperta più di una connessione contemporaneamente.
 * I nomi dei file sul server sono salvati con il loro path assoluto (iniziano con il carattere '/').
 * Qualsiasi scrittura in memoria secondaria di un file prelevato dal server avviene modificando il suo nome nel modo seguente:
 * il primo carattere '/' viene omesso e qualsiasi altro carattere '/' viene sostituito con '_'.
 */

#ifndef FSP_API_H
#define FSP_API_H

#include <time.h>

#define O_DEFAULT 0
#define O_CREATE 1
#define O_LOCK 2

// Massima lunghezza del nome socket (UNIX_PATH_MAX non definito su Mac OS)
#ifndef UNIX_PATH_MAX
#define UNIX_PATH_MAX 104
#endif

/**
 * \brief Apre una connessione AF_UNIX al socket file sockname.
 *
 * Se il server non accetta immediatamente la richiesta di connessione, la connessione viene ripetuta
 * dopo msec millisecondi e fino allo scadere del tempo assoluto abstime.
 * \return 0 in caso di successo,
 *         -1 in caso di fallimento, errno viene settato opportunamente.
 *
 *         EINVAL: se sockname == NULL || msec < 0 || abstime.tv_sec < 0 || abstime.tv_nsec < 0 || cossessione già aperta.
 *         ETIME: se è scaduto il tempo entro il quale fare richieste di connessione al server.
 *         EBADMSG: se il messaggio di risposta fsp contiene un codice non riconosciuto/inatteso.
 *         ENAMETOOLONG: se sockname è troppo lungo (la massima lunghezza è definita in FSP_API_SOCKET_NAME_MAX_LEN).
 *         ENOBUFS: se è stato impossibile allocare memoria per il buffer.
 *         ECONNREFUSED: se non è stato possibile connettersi al server o il server non è disponibile (codice di risposta 421).
 */
int openConnection(const char* sockname, int msec, const struct timespec abstime);

/**
 * \brief Chiude la connessione AF_UNIX associata al socket file sockname.
 * \return 0 in caso di seccesso,
 *         -1 in caso di fallimento, errno viene settato opportunamente.
 *
 *         EINVAL: se sockname == NULL || sockname attuale differente.
 *         ENOTCONN: se non è stata aperta la connessione con openConnection().
 *         ENOBUFS: se la memoria per il buffer non è sufficiente.
 *         EIO: se ci sono stati errori di lettura e scrittura su socket.
 *         ECONNABORTED: se il servizio non è disponibile (codice di risposta fsp 421) e la connessione è stata chiusa.
 *         EBADMSG: se il messaggio di richiesta fsp contiene errori sintattici (codice di risposta fsp 501), o
 *                  li contiene il messaggio di risposta fsp (anche in caso di codice di risposta fsp non riconosciuto/inatteso).
 *         ECANCELED: se non è stato possibile eseguire l'operazione (codice di risposta fsp 556).
 */
int closeConnection(const char* sockname);

/**
 * \brief Richiesta di apertura o di creazione del file pathname.
 *
 * La semantica dipende dai flags passati come secondo argomento che possono essere O_CREATE ed
 * O_LOCK. Se viene passato il flag O_CREATE ed il file esiste già memorizzato nel server, oppure il
 * file non esiste ed il flag O_CREATE non è stato specificato, viene ritornato un errore. In caso di
 * successo, il file viene sempre aperto in lettura e scrittura, ed in particolare le scritture
 * possono avvenire solo in append. Se viene passato il flag O_LOCK (eventualmente in OR con O_CREATE)
 * il file viene aperto e/o creato in modalità locked, che vuol dire che l’unico che può leggere o
 * scrivere il file pathname è il processo che lo ha aperto.
 * Specificare 0 (O_DEFAULT) come flags per il comportamento di default: apertura di un file esistente in
 * lettura e scrittura senza la modalità locked.
 * \return 0 in caso di successo,
 *         -1 in caso di fallimento, errno viene settato opportunamente.
 *
 *         EINVAL: se pathname == NULL || (flags != O_DEFAULT && flags != O_CREATE && flags != O_LOCK && flags != O_CREATE | O_LOCK).
 *         ENOTCONN: se non è stata aperta la connessione con openConnection().
 *         ENOBUFS: se la memoria per il buffer non è sufficiente.
 *         EIO: se ci sono stati errori di lettura e scrittura su socket.
 *         ECONNABORTED: se il servizio non è disponibile (codice di risposta fsp 421) e la connessione è stata chiusa.
 *         EBADMSG: se il messaggio di richiesta fsp contiene errori sintattici (codice di risposta fsp 501), o
 *                  li contiene il messaggio di risposta fsp (anche in caso di codice di risposta fsp non riconosciuto/inatteso).
 *         ENOENT: se non viene passato il flag O_CREATE e il file pathname non è presente sul server (codice di risposta fsp 550).
 *         EEXIST: se viene passato il flag O_CREATE ed il file pathname esiste già memorizzato nel server (codice di risposta fsp 555).
 *         ECANCELED: se non è stato possibile eseguire l'operazione (codice di risposta fsp 556).
 */
int openFile(const char* pathname, int flags);

/**
 * \brief Legge tutto il contenuto del file pathname dal server.
 *
 * Se il file pathname esiste, allora ritorna un puntatore ad un'area allocata sullo heap nel
 * parametro buf, mentre size conterrà la dimensione del buffer dati (ossia la dimensione in bytes del
 * file letto). In caso di errore, buf e size non sono validi.
 * \return 0 in caso di successo,
 *         -1 in caso di fallimento, errno viene settato opportunamente.
 *
 *         EINVAL: se pathname == NULL || buf == NULL || size == NULL.
 *         ENOTCONN: se non è stata aperta la connessione con openConnection().
 *         ENOBUFS: se la memoria per il buffer non è sufficiente o
 *                  non è stato possibile allocare memoria.
 *         EIO: se ci sono stati errori di lettura e scrittura su socket.
 *         ECONNABORTED: se il servizio non è disponibile (codice di risposta fsp 421) e la connessione è stata chiusa.
 *         EBADMSG: se il messaggio di richiesta fsp contiene errori sintattici (codice di risposta fsp 501), o
 *                  li contiene il messaggio di risposta fsp (anche in caso di codice di risposta fsp non riconosciuto/inatteso).
 *         ENOENT: se il file pathname non è presente sul server (codice di risposta fsp 550).
 *         EPERM: se l'operazione non è consentita (codice di risposta fsp 554).
 *         ECANCELED: se non è stato possibile eseguire l'operazione (codice di risposta fsp 556).
 */
int readFile(const char* pathname, void** buf, size_t* size);

/**
 * \brief Richiede al server la lettura di N files qualsiasi da memorizzare nella directory dirname
 *        lato client.
 *
 * Se il server ha meno di ‘N’ file disponibili, li invia tutti. Se N<=0 la richiesta al server è
 * quella di leggere tutti i file memorizzati al suo interno.
 * \return un valore maggiore o uguale a 0 in caso di successo (cioè ritorna il numero di file
 *         effettivamente letti),
 *         -1 in caso di fallimento, errno viene settato opportunamente.
 *
 *         EINVAL: se dirname == NULL || dirname non è una directory.
 *         ENOTCONN: se non è stata aperta la connessione con openConnection().
 *         ENOBUFS: se la memoria per il buffer non è sufficiente o
 *                  non è stato possibile allocare memoria.
 *         EIO: se ci sono stati errori di lettura e scrittura su socket o
 *              errori relativi alla scrittura dei file in dirname.
 *         ECONNABORTED: se il servizio non è disponibile (codice di risposta fsp 421) e la connessione è stata chiusa.
 *         EBADMSG: se il messaggio di richiesta fsp contiene errori sintattici (codice di risposta fsp 501), o
 *                  li contiene il messaggio di risposta fsp (anche in caso di codice di risposta fsp non riconosciuto/inatteso).
 *         ECANCELED: se non è stato possibile eseguire l'operazione (codice di risposta fsp 556).
 */
int readNFiles(int N, const char* dirname);

/**
 * \brief Scrive tutto il file puntato da pathname nel file server.
 *
 * Ritorna successo solo se la precedente operazione, terminata con successo, è stata
 * openFile(pathname, O_CREATE| O_LOCK). Se dirname è diverso da NULL, il file eventualmente spedito
 * dal server perchè espulso dalla cache per far posto al file pathname dovrà essere scritto in
 * dirname.
 * \return 0 in caso di successo,
 *         -1 in caso di fallimento, errno viene settato opportunamente.
 *
 *         EINVAL: se pathname == NULL || pathname non è un file regolare || dirname non è una directory (se dirname != NULL).
 *         ENOTCONN: se non è stata aperta la connessione con openConnection().
 *         ENOBUFS: se la memoria per il buffer non è sufficiente o
 *                  non è stato possibile allocare memoria.
 *         EIO: se ci sono stati errori di lettura e scrittura su socket o
 *              errori relativi alla scrittura dei file in dirname.
 *         ECONNABORTED: se il servizio non è disponibile (codice di risposta fsp 421) e la connessione è stata chiusa.
 *         EBADMSG: se il messaggio di richiesta fsp contiene errori sintattici (codice di risposta fsp 501), o
 *                  li contiene il messaggio di risposta fsp (anche in caso di codice di risposta fsp non riconosciuto/inatteso).
 *         ENOMEM: se non c'è sufficiente memoria sul server per eseguire l'operazione (codice di risposta fsp 552).
 *         EPERM: se l'operazione non è consentita (codice di risposta fsp 554).
 *         ECANCELED: se non è stato possibile eseguire l'operazione (codice di risposta fsp 556).
 */
int writeFile(const char* pathname, const char* dirname);

/**
 * \brief Richiesta di scrivere in append al file pathname i size bytes contenuti nel buffer buf.
 *
 * L’operazione di append nel file è garantita essere atomica dal file server. Se dirname è diverso
 * da NULL, il file eventualmente spedito dal server perchè espulso dalla cache per far posto ai
 * nuovi dati di pathname dovrà essere scritto in dirname.
 * \return 0 in caso di successo,
 *         -1 in caso di fallimento, errno viene settato opportunamente.
 *
 *         EINVAL: se pathname == NULL || buf == NULL || size < 0 || dirname non è una directory (se dirname != NULL).
 *         ENOTCONN: se non è stata aperta la connessione con openConnection().
 *         ENOBUFS: se la memoria per il buffer non è sufficiente o
 *                  non è stato possibile allocare memoria.
 *         EIO: se ci sono stati errori di lettura e scrittura su socket o
 *              errori relativi alla scrittura dei file in dirname.
 *         ECONNABORTED: se il servizio non è disponibile (codice di risposta fsp 421) e la connessione è stata chiusa.
 *         EBADMSG: se il messaggio di richiesta fsp contiene errori sintattici (codice di risposta fsp 501), o
 *                  li contiene il messaggio di risposta fsp (anche in caso di codice di risposta fsp non riconosciuto/inatteso).
 *         ENOENT: se il file pathname non è presente sul server (codice di risposta fsp 550).
 *         ENOMEM: se non c'è sufficiente memoria sul server per eseguire l'operazione (codice di risposta fsp 552).
 *         EPERM: se l'operazione non è consentita (codice di risposta fsp 554).
 *         ECANCELED: se non è stato possibile eseguire l'operazione (codice di risposta fsp 556).
 */
int appendToFile(const char* pathname, void* buf, size_t size, const char* dirname);

/**
 * \brief In caso di successo setta il flag O_LOCK al file.
 *
 * Se il file era stato aperto/creato con il flag O_LOCK e la richiesta proviene dallo stesso
 * processo, oppure se il file non ha il flag O_LOCK settato, l’operazione termina immediatamente
 * con successo, altrimenti l’operazione non viene completata fino a quando il flag O_LOCK non viene
 * resettato dal detentore della lock. L’ordine di acquisizione della lock sul file non è
 * specificato.
 * \return 0 in caso di successo,
 *         -1 in caso di fallimento, errno viene settato opportunamente.
 *
 *         EINVAL: se pathname == NULL.
 *         ENOTCONN: se non è stata aperta la connessione con openConnection().
 *         ENOBUFS: se la memoria per il buffer non è sufficiente.
 *         EIO: se ci sono stati errori di lettura e scrittura su socket.
 *         ECONNABORTED: se il servizio non è disponibile (codice di risposta fsp 421) e la connessione è stata chiusa.
 *         EBADMSG: se il messaggio di richiesta fsp contiene errori sintattici (codice di risposta fsp 501), o
 *                  li contiene il messaggio di risposta fsp (anche in caso di codice di risposta fsp non riconosciuto/inatteso).
 *         ENOENT: se il file pathname non è presente sul server (codice di risposta fsp 550).
 *         ECANCELED: se non è stato possibile eseguire l'operazione (codice di risposta fsp 556).
 */
int lockFile(const char* pathname);

/**
 * \brief Resetta il flag O_LOCK sul file pathname.
 *
 * L’operazione ha successo solo se l’owner della lock è il processo che ha richiesto l’operazione,
 * altrimenti l’operazione termina con errore.
 * \return 0 in caso di successo,
 *         -1 in caso di fallimento, errno viene settato opportunamente.
 *
 *         EINVAL: se pathname == NULL.
 *         ENOTCONN: se non è stata aperta la connessione con openConnection().
 *         ENOBUFS: se la memoria per il buffer non è sufficiente.
 *         EIO: se ci sono stati errori di lettura e scrittura su socket.
 *         ECONNABORTED: se il servizio non è disponibile (codice di risposta fsp 421) e la connessione è stata chiusa.
 *         EBADMSG: se il messaggio di richiesta fsp contiene errori sintattici (codice di risposta fsp 501), o
 *                  li contiene il messaggio di risposta fsp (anche in caso di codice di risposta fsp non riconosciuto/inatteso).
 *         ENOENT: se il file pathname non è presente sul server (codice di risposta fsp 550).
 *         ECANCELED: se non è stato possibile eseguire l'operazione (codice di risposta fsp 556).
 */
int unlockFile(const char* pathname);

/**
 * \brief Richiesta di chiusura del file puntato da pathname.
 *
 * Eventuali operazioni sul file dopo la closeFile falliscono.
 * \return 0 in caso di successo,
 *         -1 in caso di fallimento, errno viene settato opportunamente.
 *
 *         EINVAL: se pathname == NULL.
 *         ENOTCONN: se non è stata aperta la connessione con openConnection().
 *         ENOBUFS: se la memoria per il buffer non è sufficiente.
 *         EIO: se ci sono stati errori di lettura e scrittura su socket.
 *         ECONNABORTED: se il servizio non è disponibile (codice di risposta fsp 421) e la connessione è stata chiusa.
 *         EBADMSG: se il messaggio di richiesta fsp contiene errori sintattici (codice di risposta fsp 501), o
 *                  li contiene il messaggio di risposta fsp (anche in caso di codice di risposta fsp non riconosciuto/inatteso).
 *         ENOENT: se il file pathname non è presente sul server (codice di risposta fsp 550).
 *         ECANCELED: se non è stato possibile eseguire l'operazione (codice di risposta fsp 556).
 */
int closeFile(const char* pathname);

/**
 * \brief Rimuove il file cancellandolo dal file storage server.
 *
 * L’operazione fallisce se il file non è in stato locked, o è in stato locked da parte di un processo
 * client diverso da chi effettua la removeFile.
 * \return 0 in caso di successo,
 *         -1 in caso di fallimento, errno viene settato opportunamente.
 *
 *         EINVAL: se pathname == NULL.
 *         ENOTCONN: se non è stata aperta la connessione con openConnection().
 *         ENOBUFS: se la memoria per il buffer non è sufficiente.
 *         EIO: se ci sono stati errori di lettura e scrittura su socket.
 *         ECONNABORTED: se il servizio non è disponibile (codice di risposta fsp 421) e la connessione è stata chiusa.
 *         EBADMSG: se il messaggio di richiesta fsp contiene errori sintattici (codice di risposta fsp 501), o
 *                  li contiene il messaggio di risposta fsp (anche in caso di codice di risposta fsp non riconosciuto/inatteso).
 *         ENOENT: se il file pathname non è presente sul server (codice di risposta fsp 550).
 *         EPERM: se l'operazione non è consentita (codice di risposta fsp 554).
 *         ECANCELED: se non è stato possibile eseguire l'operazione (codice di risposta fsp 556).
 */
int removeFile(const char* pathname);

#endif
