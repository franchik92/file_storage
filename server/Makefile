CC = gcc
CFLAGS = -Wall -std=c99
INCLUDES = -I include
LIBS = -pthread

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

fsp_server: $(objects) | file_storage
	$(CC) $(CFLAGS) $^ -o $@ $(LIBS)
obj/fsp_server.o: src/fsp_server.c | obj
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@
obj/%.o: src/%.c include/%.h | obj
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@
file_storage:
	mkdir ~/.file_storage
obj:
	mkdir obj

clean:
	-rm -fR ~/.file_storage obj
	-rm -f fsp_server
	-rm -f /tmp/file_storage.sk