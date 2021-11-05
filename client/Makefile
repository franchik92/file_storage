CC = gcc
CFLAGS = -Wall -std=c99
INCLUDES = -I include
LDFLAGS = -Wl,-rpath,'$${ORIGIN}' -L .
LIBS = -lfsp_api

.PHONY: clean

objects = obj/fsp.o \
          obj/fsp_client_request_queue.o \
          obj/fsp_opened_file.o \
          obj/fsp_opened_files_hash_table.o
lib_objects = obj/fsp_api.o \
              obj/fsp_reader.o \
              obj/fsp_parser.o

fsp: $(objects) libfsp_api.so
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS) $(LIBS)
obj/fsp.o: src/fsp.c | obj
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@
obj/%.o: src/%.c include/%.h | obj
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@
libfsp_api.so: $(lib_objects)
	$(CC) -shared $^ -o $@
obj/fsp_api.o: src/fsp_api.c include/fsp_api.h | obj
	$(CC) $(CFLAGS) $(INCLUDES) -c -fPIC $< -o $@
obj/fsp_reader.o: src/fsp_reader.c include/fsp_reader.h | obj
	$(CC) $(CFLAGS) $(INCLUDES) -c -fPIC $< -o $@
obj/fsp_parser.o: src/fsp_parser.c include/fsp_parser.h | obj
	$(CC) $(CFLAGS) $(INCLUDES) -c -fPIC $< -o $@
obj:
	mkdir obj
clean:
	-rm -fR obj
	-rm -f fsp libfsp_api.so