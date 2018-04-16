FLAGS=-Wall -Wextra
LIBS=-lpthread
LL=gcc
CC=gcc $(FLAGS)

all: Server	Client

debug: Server	Client

Server:	gameserver.o
	$(LL) $^ -o gameserver $(LIBS)

Client: player.o
	$(LL) $^ -o player $(LIBS)

Server.o: gameserver.c mutual.h
	$(CC) gameserver.c -c -o gameserver.o

Client.o: player.c mutual.h
	$(CC) player.c -c -o player.o

.PHONY:	clean

clean:
	rm -f test *.o	gameserver player