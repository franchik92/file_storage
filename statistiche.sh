#!/bin/bash

logfile=~/.file_storage/file_storage.log

# Controlla se il file di log esiste
if ! [ -f $logfile ]; then
    echo "File di log non trovato"
    exit 1
fi

# Numero di READ/READN e size media delle letture in bytes
lines=$(grep -e ' READ ' -e ' READN ' $logfile)
if [ -z "$lines" ]; then
    n_tot=0
else
    n_tot=$(printf '%s\n' "$lines" | wc -l)
fi
echo "READ/READN (numero totale):" $n_tot
lines_succ=$(printf '%s\n' "$lines" | grep ' SUCCESS ')
if [ -z "$lines_succ" ]; then
    n_succ=0
else
    n_succ=$(printf '%s\n' "$lines_succ" | wc -l)
fi
echo "READ/READN terminati con successo:" $n_succ
echo "READ/READN falliti:" $(($n_tot-$n_succ))
printf '%s' "$lines_succ" | cut -d ' ' -f 7 |
{
    bytes=0
    while read _bytes; do
        _bytes=${_bytes:1:$((${#_bytes}-2))}
        bytes=$(($bytes+$_bytes))
    done
    if [ $n_succ != 0 ]; then
        echo "READ/READN (size media delle letture):" $(($bytes/$n_succ)) bytes
    else
        echo "READ/READN (size media delle letture): 0 bytes"
    fi
}

echo

# Numero di WRITE/APPEND e size media delle scritture in bytes
lines=$(grep -e ' WRITE ' -e ' APPEND ' $logfile)
if [ -z "$lines" ]; then
    n_tot=0
else
    n_tot=$(printf '%s\n' "$lines" | wc -l)
fi
echo "WRITE/APPEND (numero totale):" $n_tot
lines_succ=$(printf '%s\n' "$lines" | grep ' SUCCESS ')
if [ -z "$lines_succ" ]; then
    n_succ=0
else
    n_succ=$(printf '%s\n' "$lines_succ" | wc -l)
fi
echo "WRITE/APPEND terminati con successo:" $n_succ
echo "WRITE/APPEND falliti:" $(($n_tot-$n_succ))
printf '%s' "$lines_succ" | cut -d ' ' -f 7 |
{
    bytes=0
    while read _bytes; do
        _bytes=${_bytes:1:$((${#_bytes}-2))}
        bytes=$(($bytes+$_bytes))
    done
    if [ $n_succ != 0 ]; then
        echo "WRITE/APPEND (size media delle scritture):" $(($bytes/$n_succ)) bytes
    else
        echo "WRITE/APPEND (size media delle scritture): 0 bytes"
    fi
}

echo

# Numero di operazioni di LOCK
lines=$(grep ' LOCK ' $logfile)
if [ -z "$lines" ]; then
    n_tot=0
else
    n_tot=$(printf '%s\n' "$lines" | wc -l)
fi
echo "LOCK (numero totale):" $n_tot
lines_succ=$(printf '%s\n' "$lines" | grep ' SUCCESS')
if [ -z "$lines_succ" ]; then
    n_succ=0
else
    n_succ=$(printf '%s\n' "$lines_succ" | wc -l)
fi
echo "LOCK terminati con successo:" $n_succ
echo "LOCK falliti:" $(($n_tot-$n_succ))

echo

# Numero di operazioni di OPENL/OPENCL
lines=$(grep -e ' OPNEL ' -e ' OPENCL ' $logfile)
if [ -z "$lines" ]; then
    n_tot=0
else
    n_tot=$(printf '%s\n' "$lines" | wc -l)
fi
echo "OPENL/OPENCL (numero totale):" $n_tot
lines_succ=$(printf '%s\n' "$lines" | grep ' SUCCESS')
if [ -z "$lines_succ" ]; then
n_succ=0
else
n_succ=$(printf '%s\n' "$lines_succ" | wc -l)
fi
echo "OPENL/OPENCL  terminati con successo:" $n_succ
echo "OPENL/OPENCL  falliti:" $(($n_tot-$n_succ))

echo

# Numero di operazioni di UNLOCK
lines=$(grep -e ' UNLOCK ' $logfile)
if [ -z "$lines" ]; then
    n_tot=0
else
    n_tot=$(printf '%s\n' "$lines" | wc -l)
fi
echo "UNLOCK (numero totale):" $n_tot
lines_succ=$(printf '%s\n' "$lines" | grep ' SUCCESS')
if [ -z "$lines_succ" ]; then
    n_succ=0
else
    n_succ=$(printf '%s\n' "$lines_succ" | wc -l)
fi
echo "UNLOCK  terminati con successo:" $n_succ
echo "UNLOCK  falliti:" $(($n_tot-$n_succ))

echo

# Numero di operazioni di CLOSE
lines=$(grep -e ' CLOSE ' $logfile)
if [ -z "$lines" ]; then
    n_tot=0
else
    n_tot=$(printf '%s\n' "$lines" | wc -l)
fi
echo "CLOSE (numero totale):" $n_tot
lines_succ=$(printf '%s\n' "$lines" | grep ' SUCCESS')
if [ -z "$lines_succ" ]; then
    n_succ=0
else
    n_succ=$(printf '%s\n' "$lines_succ" | wc -l)
fi
echo "CLOSE  terminati con successo:" $n_succ
echo "CLOSE  falliti:" $(($n_tot-$n_succ))

echo

# Dimensione massima in Mbytes raggiunta dallo storage
grep -e ' APPEND ' -e ' WRITE ' -e ' REMOVE ' -e ' REJECTED_FILE: ' $logfile |
{
    max_bytes=0
    bytes=0
    while IFS= read _bytes; do
        case $_bytes in
            *APPEND*SUCCESS*)
                _bytes=${_bytes##*(}
                _bytes=${_bytes%)}
                bytes=$(($bytes+$_bytes))
                if [ $max_bytes -lt $bytes ]; then
                    max_bytes=$bytes
                fi
                ;;
            *WRITE*SUCCESS*)
                _bytes=${_bytes##*(}
                _bytes=${_bytes%)}
                bytes=$(($bytes+$_bytes))
                if [ $max_bytes -lt $bytes ]; then
                    max_bytes=$bytes
                fi
                ;;
            *REMOVE*SUCCESS*)
                _bytes=${_bytes##*(}
                _bytes=${_bytes%)}
                bytes=$(($bytes-$_bytes))
                ;;
            *REJECTED_FILE:*)
                _bytes=${_bytes##*(}
                _bytes=${_bytes%)}
                bytes=$(($bytes-$_bytes))
                ;;
            *)
                # FAILURE
                ;;
        esac
    done
    echo -n "Dimensione massima raggiunta dallo storage: "
    echo $(echo "scale=2;$max_bytes/1048576" | bc) Mbytes
}

echo

# Dimensione massima in numero di file raggiunta dallo storage
# Il calcolo si basa sul comportamento del programma client:
# per creare un nuovo file prima lo apre con OPENCL e poi lo scrive con WRITE
# (se OPENCL non termina con successo, allora non scrive con WRITE)
grep -e ' OPENCL ' -e ' WRITE ' -e ' REMOVE ' -e ' REJECTED_FILE: ' $logfile |
{
    max_files_num=0
    files_num=0
    while IFS= read line; do
        case $line in
            *OPENCL*SUCCESS*)
                files_num=$(($files_num+1))
                if [ $max_files_num -lt $files_num ]; then
                    max_files_num=$files_num
                fi
                ;;
            *WRITE*FAILURE*)
                files_num=$(($files_num-1))
                ;;
            *REMOVE*SUCCESS*)
                files_num=$(($files_num-1))
                ;;
            *REJECTED_FILE:*)
                files_num=$(($files_num-1))
                ;;
            *)
                # FAILURE
                ;;
        esac
    done
    echo "Dimensione massima in numero di file raggiunta dallo storage: $max_files_num"
}

echo

# Numero di volte in cui l'algoritmo di rimpiazzamento della cache è stato eseguito per selezionare uno o più file vittima
echo "CAPACITY_MISS (numero totale):" $(grep ' CAPACITY_MISS' $logfile | wc -l)

echo

# Numero di richieste servite da ogni worker thread
grep -e ' SUCCESS' -e ' FAILURE' $logfile | cut -d ' ' -f 2 |
{
    max_thread_id=0
    while read thread_id; do
        thread_id=${thread_id%:}
        if [ -z ${threads[thread_id]} ]; then
            threads[thread_id]=1
            if [ $max_thread_id -lt $thread_id ]; then
                max_thread_id=$thread_id
            fi
        else
            threads[$thread_id]=$((${threads[thread_id]}+1))
        fi
    done

    if [ $max_thread_id -gt 0 ]; then
        for ((t_id=1; t_id<=$max_thread_id; t_id++)); do
            if [ -z ${threads[t_id]} ]; then
                echo "Numero di richieste servite dal worker thread $t_id: 0" > /dev/null
            else
                echo "Numero di richieste servite dal worker thread $t_id: ${threads[t_id]}"
            fi
        done
    else
        echo "Nessun worker thread ha servito le richieste"
    fi
}

echo

# Massimo numero di connessioni contemporanee
grep -e ' CONNECTION_OPENED: ' -e ' CONNECTION_CLOSED: ' $logfile |
{
    max_conn=0
    conn=0
    while IFS= read line; do
        case $line in
            *CONNECTION_OPENED:*)
                conn=$(($conn+1))
                if [ $max_conn -lt $conn ]; then
                    max_conn=$conn
                fi
                ;;
            *CONNECTION_CLOSED:*)
                conn=$(($conn-1))
                ;;
            *)
                # Errore
                ;;
        esac
    done
    echo "Massimo numero di connessioni contemporanee: $max_conn"
}
