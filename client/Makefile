CC = gcc
CFLAGS = -Wall -std=c99 -I include

.PHONY: clean

objects = obj/fsp.o \
          obj/fsp_client_request_queue.o \
          obj/fsp_opened_file.o \
          obj/fsp_opened_files_hash_table.o \
          obj/fsp_api.o \
          obj/fsp_reader.o \
          obj/fsp_parser.o

fsp: $(objects)
	$(CC) $(CFLAGS) $^ -o $@
obj/fsp.o: src/fsp.c | obj
	$(CC) $(CFLAGS) -c $< -o $@
obj/fsp_client_request_queue.o: src/fsp_client_request_queue.c include/fsp_client_request_queue.h | obj
	$(CC) $(CFLAGS) -c $< -o $@
obj/fsp_opened_file.o: src/fsp_opened_file.c include/fsp_opened_file.h | obj
	$(CC) $(CFLAGS) -c $< -o $@
obj/fsp_opened_files_hash_table.o: src/fsp_opened_files_hash_table.c include/fsp_opened_files_hash_table.h | obj
	$(CC) $(CFLAGS) -c $< -o $@
obj/fsp_api.o: src/fsp_api.c include/fsp_api.h | obj
	$(CC) $(CFLAGS) -c $< -o $@
obj/fsp_reader.o: src/fsp_reader.c include/fsp_reader.h | obj
	$(CC) $(CFLAGS) -c $< -o $@
obj/fsp_parser.o: src/fsp_parser.c include/fsp_parser.h | obj
	$(CC) $(CFLAGS) -c $< -o $@
obj:
	mkdir obj

clean:
	-rm -fR obj
	-rm -f fsp