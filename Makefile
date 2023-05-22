CC = g++
CFLAGS = -Wall -Wextra -std=c++17

all: build

build: server subscriber

server: server.cpp
	$(CC) $(CFLAGS) $^ -o $@

subscriber: subscriber.cpp
	$(CC) $(CFLAGS) $^ -o $@

.PHONY: clean

clean:
	rm server subscriber
