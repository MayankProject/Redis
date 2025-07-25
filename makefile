CC = gcc
CFLAGS = -Wall -Wextra -O2 -Iinclude

TARGETS = server client

SERVER_SRC = src/server.c src/hashmap.c src/avl.c src/zset.c
CLIENT_SRC = src/client.c

.PHONY: all clean

all: $(TARGETS)

server: $(SERVER_SRC)
	$(CC) $(CFLAGS) -o $@ $^ -ggdb

client: $(CLIENT_SRC)
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f $(TARGETS)