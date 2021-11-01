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
echo "FILES_MAX_NUM=10" >> $config_file
echo "STORAGE_MAX_SIZE=1" >> $config_file
echo "MAX_CONN=16" >> $config_file
echo "WORKER_THREADS_NUM=4" >> $config_file

# Avvia in background il processo server
${server_dir}/fsp_server 1> server_out/s.txt 2> server_err_out/s.txt &
server_pid=$!

# Esegue 10 processi client in background
f_opt="-f /tmp/file_storage.sk"
files_dir_01=files/dir_01
files_dir_02=files/dir_02
files_dir_03=files/dir_03

for index in {1..5}; do
    ${client_dir}/fsp $f_opt -p \
        -W ${files_dir_01}/gb-wls.png,${files_dir_01}/us.png -t 400 \
        -w ${files_dir_03} -D rejected_files -t 400 \
        -r ${PWD}/${files_dir_02}/Kittens.jpg,${PWD}/${files_dir_02}/Olly.jpg,${PWD}/${files_dir_02}/random.jpg,${PWD}/${files_dir_02}/Ivory.jpg -d downloaded_files \
        1> clients_out/c1_${index}.txt 2> clients_err_out/c1_${index}.txt &
    ${client_dir}/fsp $f_opt -p \
        -W ${files_dir_02}/Kittens.jpg,${files_dir_02}/Olly.jpg,${files_dir_02}/random.jpg,${files_dir_02}/Ivory.jpg -D rejected_files -t 200 \
        -R 4 -d downloaded_files \
        1> clients_out/c2_${index}.txt 2> clients_err_out/c2_${index}.txt &
done

# Attende cinque secondi
echo "Attendere cinque secondi..."
sleep 5

# Invia il segnale SIGHUP al server
kill -s HUP $server_pid
