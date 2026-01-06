# Kompilator i flagi
CC = gcc
CFLAGS = -Wall -g -Wextra
INCLUDES = -IInclude
SRCDIR = Src

# Plik wspolny dla wszystkich modulow
COMMON_SRC = $(SRCDIR)/ipc.c
COMMON_DEP = $(COMMON_SRC) Include/ipc.h

# Lista plikow wykonywalnych do zbudowania
# Uwaga: nazwy musza odpowiadac tym uzywanym w execlp() w kodzie!
TARGETS = kierownik kasjer klient pracownik generator kasy_samo

# Domyslny cel (buduje wszystko)
all: $(TARGETS)

# --- Reguly budowania poszczegolnych programow ---

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

# Specjalna regula dla kasy_samo (bo plik zrodlowy to kasa_samo.c, a exec to kasy_samo)
kasy_samo: $(SRCDIR)/kasa_samo.c $(COMMON_DEP)
	$(CC) $(CFLAGS) $(INCLUDES) -o kasy_samo $(SRCDIR)/kasa_samo.c $(COMMON_SRC)

# --- Narzedzia pomocnicze ---

# Czyszczenie plikow wykonywalnych i smieci systemowych (IPC)
clean:
	rm -f $(TARGETS) *.o
	@echo "Wyczyszczono pliki binarne."

# Usuwanie ewentualnych smieci w /tmp (klucz IPC)
clean_ipc:
	rm -f /tmp/dyskont_projekt

# Pomocniczy cel do szybkiego startu (najpierw wyczysci, potem zbuduje)
rebuild: clean all

.PHONY: all clean clean_ipc rebuild