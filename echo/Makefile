CC = gcc
CFLAGS = -O2 -Wall -I ../tiny

client: echoclient.c
	$(CC) $(CFLAGS) -o client echoclient.c ../tiny/csapp.c
server: echoserver.c
	$(CC) $(CFLAGS) -o server echoserver.c ../tiny/csapp.c
clean:
	rm -f *.o server