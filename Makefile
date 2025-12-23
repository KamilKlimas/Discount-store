CC = gcc

CFLAGS = -Wall -g -IInclude
VPATH = Src

all: kierownik klient kasjer generator
.PHONY: clean

ipc.o: ipc.c Include/ipc.h

kierownik.o: kierownik.c Include/ipc.h

kasjer.o: kasjer.c Include/ipc.h

klient.o: klient.c Include/ipc.h

kierownik: kierownik.o ipc.o

klient: klient.o ipc.o

kasjer: kasjer.o ipc.o

generator: generator.o ipc.o

generator.o: generator.c


clean:
	rm -f *.o kierownik klient kasjer
