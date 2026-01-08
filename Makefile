CC = gcc
CFLAGS = -Wall -g -Wextra
INCLUDES = -IInclude
SRCDIR = Src

COMMON_SRC = $(SRCDIR)/ipc.c
COMMON_DEP = $(COMMON_SRC) Include/ipc.h

TARGETS = kierownik kasjer klient pracownik generator kasy_samo

all: $(TARGETS)

kierownik: $(SRCDIR)/kierownik.c $(COMMON_DEP)
	$(CC) $(CFLAGS) $(INCLUDES) -o kierownik $(SRCDIR)/kierownik.c $(COMMON_SRC)

kasjer: $(SRCDIR)/kasjer.c $(COMMON_DEP)
	$(CC) $(CFLAGS) $(INCLUDES) -o kasjer $(SRCDIR)/kasjer.c $(COMMON_SRC)

klient: $(SRCDIR)/klient.c $(COMMON_DEP)
	$(CC) $(CFLAGS) $(INCLUDES) -o klient $(SRCDIR)/klient.c $(COMMON_SRC)

pracownik: $(SRCDIR)/pracownik.c $(COMMON_DEP)
	$(CC) $(CFLAGS) $(INCLUDES) -o pracownik $(SRCDIR)/pracownik.c $(COMMON_SRC)

generator: $(SRCDIR)/generator.c $(COMMON_DEP)
	$(CC) $(CFLAGS) $(INCLUDES) -o generator $(SRCDIR)/generator.c $(COMMON_SRC)

kasy_samo: $(SRCDIR)/kasa_samo.c $(COMMON_DEP)
	$(CC) $(CFLAGS) $(INCLUDES) -o kasy_samo $(SRCDIR)/kasa_samo.c $(COMMON_SRC)


clean:
	rm -f $(TARGETS) *.o
	@echo "Wyczyszczono pliki binarne."

clean_ipc:
	rm -f /tmp/dyskont_projekt

rebuild: clean all

.PHONY: all clean clean_ipc rebuild