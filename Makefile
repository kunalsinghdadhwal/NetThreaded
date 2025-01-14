CC=g++
CFLAGS= -g -Wall

all: clean proxy

proxy: proxy_server_cache.c
		$(CC) $(CFLAGS) -o proxy_parse.o -c proxy_parse.c -lpthread
		$(CC) $(CFLAGS) -o proxy.o -c proxy_server_cache.c -lpthread
		$(CC) $(CFLAGS) -o proxy proxy_parse.o proxy.o -lpthread


clean: 
		rm -f proxy *.o

