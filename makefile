CC = gcc
CFLAGS = -Wall -Wextra -O2

TARGETS = server client

SERVER_SRC = server.c hashmap.c
CLIENT_SRC = client.c

.PHONY: all clean

all: $(TARGETS)

server: $(SERVER_SRC)
	$(CC) $(CFLAGS) -o $@ $^

client: $(CLIENT_SRC)
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f $(TARGETS)
