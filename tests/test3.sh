# Crea il file di configurazione
CONFIG_FILE=./server/config.txt
echo "SOCKET_FILE_NAME=/tmp/file_storage.sk" > $CONFIG_FILE
echo "LOG_FILE_NAME=log_file.txt" >> $CONFIG_FILE
echo "FILES_MAX_NUM=100" >> $CONFIG_FILE
echo "STORAGE_MAX_SIZE=32" >> $CONFIG_FILE
echo "MAX_CONN=16" >> $CONFIG_FILE
echo "WORKER_THREADS_NUM=8" >> $CONFIG_FILE

# Avvia in background il processo server
cd server
./fsp_server 1> ../tests/server_out/server_out.txt &
SERVER_PID=$!
cd ..

# Esegue i processi dei client in background per 30 secondi
cd client
end=$(($SECONDS+2))

numero=0
while [ $SECONDS -lt $end ]; do
./fsp -f /tmp/file_storage.sk -p -w ../tests/files/dir_01 -D ../tests/rejected_files -l ../tests/files/dir_01/si.png -c ../tests/files/dir_01/sk.png,../tests/files/dir_01/td.png -R 2 -d ../tests/downloaded_files -u ../tests/files/dir_01/si.png 1> /Users/francesco/Desktop/es/{$numero}.txt 2> /Users/francesco/Desktop/err.txt &
#./fsp -f /tmp/file_storage.sk -W ../tests/files/dir_01/si.png,../tests/files/dir_02/Mama.jpg -D ../tests/rejected_files -c ../tests/files/dir_01/af.png,../tests/files/dir_01/ki.png -w ../tests/files/dir_02,2 -R 2 -d ../tests/downloaded_files -r ../tests/files/dir_02/heart.jpg &> /dev/null &
numero=$((numero+1))
done

cd ..

# Invia il segnale SIGHUP al server
kill -2 $SERVER_PID
