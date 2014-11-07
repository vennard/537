# JLV - 10/17/14
# Early version - use only as framework example

CC= gcc
CFLAGS= -std=c99 -Wall -Wextra -Werror

all: CFLAGS += -DDEBUG=0
all: server client repeater
	
debug: CFLAGS += -DDEBUG=1 -g
debug: server client repeater

server: server.c
	$(CC) $(CFLAGS) server.c common.c common.h -o server

client: client.c
	$(CC) $(CFLAGS) client.c common.c common.h -o client 

repeater: repeater.c
	$(CC) $(CFLAGS) repeater.c common.c common.h -o repeater

clean:
	rm -f server client repeater graph.png graph_datafile client_pic.bmp client_random

