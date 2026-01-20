//
// Created by kamil-klimas on 19.12.2025.
//
#include "ipc.h"
#include <sys/wait.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <stdio.h>
#include <signal.h>
#include <errno.h>
#include <pthread.h>

#ifdef TRYB_TURBO
    #define TRYB_NAPIS "TURBO"
#else
    #define TRYB_NAPIS "NORMALNY"
#endif

volatile sig_atomic_t running = 1;
int wejscie_do_sklepu;
int pid;
int id_pamieci;

PamiecDzielona *sklep;

pthread_t t_zombie;
sigset_t maska_sigchld;

void *watekSprzatajacy(void *arg) {
    (void) arg;
    int sig;
    while (1) {
        int err = sigwait(&maska_sigchld, &sig);
        if (err != 0) {
            perror("sigwait error");
            continue;
        }

        if (sig == SIGCHLD) {
            while (waitpid(-1, NULL, WNOHANG) > 0);
        }
    }
}

void obslugaSIGINT(int sig) {
    if (sig == SIGINT) { running = 0; }
}

int main() {
    if (sigemptyset(&maska_sigchld) == -1) {
        perror("sigemptyset");
        exit(1);
    }
    if (sigaddset(&maska_sigchld, SIGCHLD) == -1) {
        perror("sigaddset SIGCHLD");
        exit(1);
    }

    if (pthread_sigmask(SIG_BLOCK, &maska_sigchld, NULL) != 0) {
        perror("Blad blokowania SIGCHLD");
        exit(1);
    }

    //wątek sprzątający asynchronicznie
    if (pthread_create(&t_zombie, NULL, watekSprzatajacy, NULL) != 0) {
        perror("Blad tworzenia watku zombie");
        exit(1);
    }

    if (signal(SIGINT, obslugaSIGINT) == SIG_ERR) {
        perror("signal SIGINT");
        exit(1);
    }

    setbuf(stdout, NULL);
    srand(time(NULL));

    key_t klucz = ftok("/tmp/dyskont_projekt", 'S');
    if (klucz == -1) {
        perror("\nbrak argumentu od kierownika\n");
        exit(1);
    }

    id_pamieci = podlacz_pamiec_dzielona();
    if (id_pamieci == -1) {
        perror("\nblad podlacz_pamiec_dzielona\n");
        exit(1);
    }
    sklep = mapuj_pamiec_dzielona(id_pamieci);

    int id_semafora = alokujSemafor(klucz, 4, 0);

    int liczba_klientow = inputExceptionHandler("Podaj liczbe klientow do symulacji");

    // "Klienci przychodzą do sklepu w dowolnych momentach czasu..."
    LOG_GENERATOR_BOTH("Wpuszczam %d procesow (tryb generatora: %s)", liczba_klientow, TRYB_NAPIS);


    int utworzone = 0;
    for (int i = 0; i < liczba_klientow; i++) {
        if (running == 0) {
#ifndef TRYB_TURBO
            LOG_GENERATOR("Otrzymano Ctrl+C. Zamykam sklep dla nowych klientow.");
#endif
            break;
        }

#ifndef TRYB_TURBO
        if (sklep->czy_otwarte == 0 || sklep->statystyki.ewakuacja == 1) {
            LOG_GENERATOR("Sklep zamkniety. Koncze wpuszczanie klientow.\n");
            running = 0;
            break;
        }

        waitSemafor(id_semafora, SEM_KASY, 0);
        int otwarte = sklep->czy_otwarte;
        int ewakuacja = sklep->statystyki.ewakuacja;
        signalSemafor(id_semafora, SEM_KASY);

        if (!running || !otwarte || ewakuacja) {
            LOG_GENERATOR("Sklep zamkniety lub ewakuacja. Wstrzymuje tworzenie klientow.");
            break;
        }
#else
        (void) id_semafora;
#endif

#ifndef TRYB_TURBO
        long delay = 0;
        if (i < 10) delay = rand() % 500000 + 500000; // Faza 1: Wolno (0.5 - 1.0s)
        else if (i < liczba_klientow * 0.6) delay = rand() % 50000; // Faza 2: Bardzo szybko (0 - 0.05s)
        else delay = rand() % 300000;
        SIM_SLEEP_US(delay);
#endif

        pid = fork();
        if (pid == 0) {
            sigset_t empty;
            sigemptyset(&empty);
            pthread_sigmask(SIG_SETMASK, &empty, NULL);

            execlp("./klient", "klient", NULL);
            perror("\nNie ma pliku klienta\n");
            exit(1);
        } else if (pid > 0) {
            utworzone++;
#ifndef TRYB_TURBO
            if (i < 5 || i >= liczba_klientow - 5 || i % 20 == 0) {
                LOG_GENERATOR("Wchodzi klient [%d/%d] (PID: %d)", i + 1, liczba_klientow, pid);
            }
#endif
        } else {
            perror("Fork failed");
            SIM_SLEEP_US(100000);
            i--;
        }
    }

    LOG_GENERATOR_BOTH("Zakonczylem wpuszczanie klientow. Utworzone: %d/%d", utworzone, liczba_klientow);

    pthread_cancel(t_zombie);
    pthread_join(t_zombie, NULL);

    odlacz_pamiec_dzielona(sklep);
    return 0;
}
