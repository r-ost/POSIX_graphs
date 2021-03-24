CC=gcc
CFLAGS= -std=gnu99 -Wall

all: prog client

prog: prog.c
	${CC} -o prog prog.c ${CFLAGS}

client: client.c
	${CC} -o client client.c ${CFLAGS}

.PHONY: clean

clean:
	rm -f prog client