CC = gcc
CFLAGS = -Wall -Wextra -g -D_GNU_SOURCE -IInclude
LIBS = -lm

TARGETS = kierownik kasjer klient pracownik generator

all: $(TARGETS)

kierownik: Src/kierownik.c Src/ipc.c Include/ipc.h
	$(CC) $(CFLAGS) Src/kierownik.c Src/ipc.c -o kierownik $(LIBS)

kasjer: Src/kasjer.c Src/ipc.c Include/ipc.h
	$(CC) $(CFLAGS) Src/kasjer.c Src/ipc.c -o kasjer $(LIBS)

klient: Src/klient.c Src/ipc.c Include/ipc.h
	$(CC) $(CFLAGS) Src/klient.c Src/ipc.c -o klient $(LIBS)

pracownik: Src/pracownik.c Src/ipc.c Include/ipc.h
	$(CC) $(CFLAGS) Src/pracownik.c Src/ipc.c -o pracownik $(LIBS)

generator: Src/generator.c Src/ipc.c Include/ipc.h
	$(CC) $(CFLAGS) Src/generator.c Src/ipc.c -o generator $(LIBS)

clean:
	rm -f $(TARGETS) *.o

clean_ipc:
	ipcrm --all 2>/dev/null || true
	rm -f /tmp/dyskont_projekt

rebuild: clean clean_ipc all
