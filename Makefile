CC = gcc

CFLAGS = -Wall -g -IInclude

SRC_DIR = Src
INC_DIR = Include

all:kierownik

ipc.o: $(SRC_DIR)/ipc.c $(INC_DIR)/ipc.h
	$(CC) $(CFLAGS) -c $(SRC_DIR)/ipc.c -o ipc.o

kierownik: $(SRC_DIR)/kierownik.c ipc.o
	$(CC) $(CFLAGS) $(SRC_DIR)/kierownik.c ipc.o -o kierownik

clean:
	rm -f *.o kierownik