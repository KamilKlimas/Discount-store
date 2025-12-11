CC = gcc

CFLAGS = -Wall -g //wall daje ostrzezenia

all: kierownik

kierownik: kierownik.c ipc.o
	$(CC) $CFLAGS kierownik.c ipc.o -o kierownik

ipc.o: ip.c ipc.h
	$(CC) $(CFLAGS) -c ipc.c

clean:
	rm -f *.o kierownik