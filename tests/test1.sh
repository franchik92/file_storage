#!/bin/bash

client_dir=../client
server_dir=../server

# Controlla se sono state create tutte le directory necessarie
if ! [ -d ~/.file_storage ] || ! [ -d clients_out ] || ! [ -d clients_err_out ] || \
   ! [ -d server_out ] || ! [ -d server_err_out ] || ! [ -d downloaded_files ] || ! [ -d rejected_files ]; then
    echo "Usare il comando make all prima di eseguire il test"
    exit 1
fi

# Controlla che il file /tmp/file_storage.sk non sia già in uso
if [ -a /tmp/file_storage.sk ]; then
    echo "Impossibile eseguire il server in quanto il file /tmp/file_storage.sk è già in uso"
    exit 1
fi

# Crea il file di configurazione
config_file=~/.file_storage/config.txt
echo "SOCKET_FILE_NAME=/tmp/file_storage.sk" > $config_file
echo "LOG_FILE_NAME=$HOME/.file_storage/file_storage.log" >> $config_file
echo "FILES_MAX_NUM=10000" >> $config_file
echo "STORAGE_MAX_SIZE=128" >> $config_file
echo "MAX_CONN=16" >> $config_file
echo "WORKER_THREADS_NUM=1" >> $config_file

# Avvia in background il processo server
if command -v valgrind &> /dev/null
then
    valgrind -leak-check=full ${server_dir}/fsp_server 1> server_out/s.txt 2> server_err_out/s.txt &
else
    echo "Valgrind non disponibile: il processo server verrà eseguito senza valgrind"
    ${server_dir}/fsp_server 1> server_out/s.txt 2> server_err_out/s.txt &
fi
server_pid=$!

# Esegue un processo client in background
f_opt="-f /tmp/file_storage.sk"
files_dir_01=files/dir_01
files_dir_02=files/dir_02
files_dir_03=files/dir_03

${client_dir}/fsp $f_opt -p \
    -w ${files_dir_01},5 -t 200 \
    -w ${files_dir_01},0 -D rejected_files -t 200 \
    -w ${files_dir_02} -D rejected_files -t 200 \
    -W ${files_dir_03}/file_01.txt -t 200 \
    -W ${files_dir_03}/file_02.txt,${files_dir_03}/file_03.txt,${files_dir_03}/file_04.txt,${files_dir_03}/file_05.txt -D rejected_files -t 200 \
    -W ${files_dir_01}/us.png,${files_dir_02}/many.jpg,${files_dir_03}/file_05.txt -D rejected_files -t 200 \
    -r ${PWD}/${files_dir_01}/bz.png -t 200 \
    -r ${PWD}/${files_dir_01}/pm.png,${PWD}/${files_dir_02}/meow.jpg,${PWD}/${files_dir_03}/file_02.txt -d downloaded_files -t 200 \
    -R 10 -t 200 \
    -R -d downloaded_files -t 200 \
    -R 0 -d downloaded_files -t 200 \
    -l ${PWD}/${files_dir_02}/play.jpg -t 200 \
    -l ${PWD}/${files_dir_01}/ec.png,${PWD}/${files_dir_02}/purrs.jpg,${PWD}/${files_dir_03}/file_01.txt -t 200 \
    -u ${PWD}/${files_dir_02}/play.jpg -t 200 \
    -u ${PWD}/${files_dir_01}/ec.png,${PWD}/${files_dir_02}/purrs.jpg,${PWD}/${files_dir_03}/file_01.txt,${PWD}/${files_dir_03}/file_02.txt -t 200 \
    -c ${PWD}/${files_dir_01}/yt.png -t 200 \
    -c ${PWD}/${files_dir_01}/yt.png,${PWD}/${files_dir_01}/mp.png,${PWD}/${files_dir_02}/Ivory.jpg,${PWD}/${files_dir_03}/file_03.txt -t 200 \
    1> clients_out/c1_${index}.txt 2> clients_err_out/c1_${index}.txt &

# Attende cinque secondi
echo "Attendere cinque secondi..."
sleep 5

# Invia il segnale SIGHUP al server
kill -s HUP $server_pid
