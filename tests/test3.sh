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
echo "FILES_MAX_NUM=100" >> $config_file
echo "STORAGE_MAX_SIZE=32" >> $config_file
echo "MAX_CONN=16" >> $config_file
echo "WORKER_THREADS_NUM=8" >> $config_file

# Avvia in background il processo server
${server_dir}/fsp_server 1> server_out/s.txt 2> server_err_out/s.txt &
server_pid=$!

# Esegue i processi client in background
f_opt="-f /tmp/file_storage.sk"
files_dir_01=files/dir_01
files_dir_02=files/dir_02
files_dir_03=files/dir_03

echo "Attendere trenta secondi..."
end=$(($SECONDS+30))
while [ $SECONDS -lt $end ]; do
    ${client_dir}/fsp $f_opt \
        -w ${files_dir_01},2 -D rejected_files -t 0 \
        -W ${files_dir_02}/meow.jpg -D rejected_files -t 0 \
        -l ${PWD}/${files_dir_02}/Olly.jpg -t 0 \
        -r ${PWD}/${files_dir_02}/Olly.jpg -d downloaded_files -t 0 \
        -u ${PWD}/${files_dir_02}/Olly.jpg -t 0 \
        -r ${PWD}/${files_dir_02}/meow.jpg -d downloaded_files -t 0 \
        -c ${PWD}/${files_dir_02}/Olly.jpg -t 0 \
        -R 2 -d downloaded_files \
        &> /dev/null &
    ${client_dir}/fsp $f_opt \
        -w ${files_dir_03},2 -D rejected_files -t 0 \
        -W ${files_dir_02}/Olly.jpg -D rejected_files -t 0 \
        -l ${PWD}/${files_dir_02}/meow.jpg -t 0 \
        -r ${PWD}/${files_dir_02}/meow.jpg -d downloaded_files -t 0 \
        -u ${PWD}/${files_dir_02}/meow.jpg -t 0 \
        -r ${PWD}/${files_dir_02}/Olly.jpg -d downloaded_files -t 0 \
        -c ${PWD}/${files_dir_02}/meow.jpg -t 0 \
        -R 2 -d downloaded_files \
        &> /dev/null &
    ${client_dir}/fsp $f_opt \
        -w ${files_dir_01},5 -D rejected_files -t 0 \
        -w ${files_dir_02},5 -D rejected_files -t 0 \
        -w ${files_dir_03},5 -D rejected_files -t 0 \
        -W ${files_dir_01}/pm.png,${files_dir_02}/Ivory.jpg,${files_dir_03}/file_03.txt -D rejected_files -t 0 \
        -R 5 -d downloaded_files \
        &> /dev/null &
    ${client_dir}/fsp $f_opt \
        -R 5 -d downloaded_files -t 0 \
        -R 10 -d downloaded_files -t 0 \
        -R 15 -d downloaded_files -t 0 \
        -r ${PWD}/${files_dir_01}/pm.png,${PWD}/${files_dir_02}/Ivory.jpg,${PWD}/${files_dir_03}/file_03.txt -d downloaded_files -t 0 \
        -w ${files_dir_03} -D rejected_files \
        &> /dev/null &
    ${client_dir}/fsp $f_opt \
        -W ${files_dir_01}/pm.png,${files_dir_01}/vi.png -D rejected_files -t 0 \
        -r ${PWD}/${files_dir_01}/pm.png,${PWD}/${files_dir_01}/vi.png -d downloaded_files -t 0 \
        -l ${PWD}/${files_dir_01}/pm.png,${PWD}/${files_dir_01}/vi.png -t 0 \
        -c ${PWD}/${files_dir_01}/pm.png -t 0 \
        -u ${PWD}/${files_dir_01}/vi.png \
        &> /dev/null &
    sleep 0.1
done

# Invia il segnale SIGINT al server
kill -s INT $server_pid
