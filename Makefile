# ssh-forwardd Makefile

CC = clang
CFLAGS = -Wall -Wextra -O2 -std=c99
PREFIX = /usr/local

SRC = src/ssh-forwardd.c
BIN = ssh-forwardd

.PHONY: all clean install uninstall

all: $(BIN)

$(BIN): $(SRC)
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(BIN)

install: $(BIN)
	install -d $(PREFIX)/bin
	install -m 755 $(BIN) $(PREFIX)/bin/

uninstall:
	rm -f $(PREFIX)/bin/$(BIN)
