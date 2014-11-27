# JLV - 10/17/14
# Early version - use only as framework example

CC= gcc
CFLAGS= -std=c99 -Wall -Wextra -Werror

all: CFLAGS += -DDEBUG=0
all: server client 
	
debug: CFLAGS += -DDEBUG=1 -g
debug: server client 

server: server.c
	$(CC) $(CFLAGS) server.c common.c common.h packet_buffer.c packet_buffer.h -o server

client: client.c
	$(CC) $(CFLAGS) client.c common.c common.h packet_buffer.c packet_buffer.h -o client 

clean:
	rm -f server client repeater graph.png graph_datafile client_pic.bmp client_random

