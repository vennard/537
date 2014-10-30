# JLV - 10/17/14
# Early version - use only as framework example

#CFLAGS= -Wall -Wextra -Werror

all: server client repeater

server: server.c
	$(CC) $(CFLAGS) server.c -o server

client: client.c
	$(CC) $(CFLAGS) client.c -o client 

repeater: repeater.c
	$(CC) $(CFLAGS) repeater.c -o repeater

clean:
	-rm server client repeater

