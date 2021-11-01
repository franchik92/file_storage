CC = gcc
CFLAGS = -Wall -std=c99 -I include

.PHONY: clean

objects = obj/fsp_server.o \
          obj/fsp_file.o \
          obj/fsp_files_hash_table.o \
          obj/fsp_files_queue.o \
          obj/fsp_files_list.o \
          obj/fsp_client.o \
          obj/fsp_clients_hash_table.o \
          obj/fsp_sfd_queue.o \
          obj/fsp_reader.o \
          obj/fsp_parser.o

fsp_server: $(objects)
	$(CC) $(CFLAGS) $^ -o $@
obj/fsp_server.o: src/fsp_server.c | obj
	$(CC) $(CFLAGS) -c $< -o $@
obj/fsp_file.o: src/fsp_file.c include/fsp_file.h | obj
	$(CC) $(CFLAGS) -c $< -o $@
obj/fsp_files_hash_table.o: src/fsp_files_hash_table.c include/fsp_files_hash_table.h | obj
	$(CC) $(CFLAGS) -c $< -o $@
obj/fsp_files_queue.o: src/fsp_files_queue.c include/fsp_files_queue.h | obj
	$(CC) $(CFLAGS) -c $< -o $@
obj/fsp_files_list.o: src/fsp_files_list.c include/fsp_files_list.h | obj
	$(CC) $(CFLAGS) -c $< -o $@
obj/fsp_client.o: src/fsp_client.c include/fsp_client.h | obj
	$(CC) $(CFLAGS) -c $< -o $@
obj/fsp_clients_hash_table.o: src/fsp_clients_hash_table.c include/fsp_clients_hash_table.h | obj
	$(CC) $(CFLAGS) -c $< -o $@
obj/fsp_sfd_queue.o: src/fsp_sfd_queue.c include/fsp_sfd_queue.h | obj
	$(CC) $(CFLAGS) -c $< -o $@
obj/fsp_reader.o: src/fsp_reader.c include/fsp_reader.h | obj
	$(CC) $(CFLAGS) -c $< -o $@
obj/fsp_parser.o: src/fsp_parser.c include/fsp_parser.h | obj
	$(CC) $(CFLAGS) -c $< -o $@
obj:
	mkdir obj

clean:
	-rm -fR obj
	-rm -f fsp_server
	-rm -f /tmp/file_storage.sk ~/.file_storage/file_storage.log