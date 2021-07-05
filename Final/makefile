all: server client

server: main.c server.h Database.h Database.c
	gcc main.c Database.c -o server -Wall -lrt -lpthread -ggdb3

client: client.c
	gcc client.c -o client -Wall -lrt -lpthread -ggdb3

clean:
	rm server
	rm client
