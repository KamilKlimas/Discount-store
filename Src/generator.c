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
#include <sched.h>



volatile sig_atomic_t running = 1;

int wejscie_do_sklepu;
int pid;

int id_pamieci;
PamiecDzielona *sklep;

void obslugaSIGCHLD(int sig)
{
    (void)sig;
    while (waitpid(-1, NULL, WNOHANG) > 0);
}

void obslugaSIGINT(int sig)
{
    if (sig == SIGINT)
    {
        running = 0;
        exit(0);
    }
}

int main()
{
    signal(SIGINT, obslugaSIGINT);
    signal(SIGCHLD, obslugaSIGCHLD);

    setbuf(stdout, NULL);
    srand(time(NULL));

    key_t klucz = ftok("/tmp/dyskont_projekt", 'S');
    if (klucz == -1)
    {
        perror("\nbrak argumentu od kierownika\n");
        exit(1);
    }

    id_pamieci = podlacz_pamiec_dzielona();
    if (id_pamieci == -1)
    {
        perror("\nblad podlacz_pamiec_dzielona\n");
        exit(1);
    }
    sklep = mapuj_pamiec_dzielona(id_pamieci);
    int id_semafora = alokujSemafor(klucz, 3, 0);

    int liczba_klientow = inputExceptionHandler("Podaj liczbe klientów do symulacji");

    LOG_GENERATOR("ROZPOCZYNAM SYMULACJE DLA %d KLIENTÓW\n", liczba_klientow);

    for (int i= 0; i < liczba_klientow; i++)
    {

        if (running == 0) {
            LOG_GENERATOR("Otrzymano Ctrl+C. Zamykam sklep dla nowych klientów.");
            if(sklep != NULL) sklep->czy_otwarte = 0;
            break;
        }

        if (sklep->czy_otwarte == 0 || sklep->statystyki.ewakuacja == 1)
        {
            LOG_GENERATOR("Sklep zamkniety. Koncze wpuszczanie klientow.\n");
            running = 0;
            break;
        }

        while(1) {
            if (!running || sklep->statystyki.ewakuacja) break;

            waitSemafor(id_semafora, SEM_KASY, 0);
            int aktualnie_w_sklepie = sklep->statystyki.liczba_klientow_w_sklepie;
            signalSemafor(id_semafora, SEM_KASY);

            if (aktualnie_w_sklepie < MAX_KLIENCI_W_SKLEPIE) {
                break;
            }

            while(running && !sklep->statystyki.ewakuacja) {
                SIM_SLEEP_US(100000);

                waitSemafor(id_semafora, SEM_KASY, 0);
                int stan = sklep->statystyki.liczba_klientow_w_sklepie;
                signalSemafor(id_semafora, SEM_KASY);

                if(stan <= PROG_WZNOWIENIA) {
                    break;
                }
            }
        }

        waitSemafor(id_semafora, SEM_KASY, 0);
        if (!running || sklep->statystyki.ewakuacja) break;
        signalSemafor(id_semafora, SEM_KASY);

        long delay = 0;
        if (i < 10) delay = rand() % 500000 + 500000;       // Faza 1: Wolno (0.5 - 1.0s)
        else if (i < liczba_klientow * 0.6) delay = rand() % 50000; // Faza 2: Bardzo szybko (0 - 0.05s)
        else delay = rand() % 300000;
        SIM_SLEEP_US(delay);

        pid = fork();
        if (pid == 0)
        {
            execlp("./klient","klient", NULL);

            perror("\nNie ma pliku klienta\n");
            exit(1);

        } else if (pid > 0) {
            if (i < 5 || i >= liczba_klientow-5 || i % 20 == 0) {
                LOG_GENERATOR("Wchodzi klient [%d/%d] (PID: %d)", i+1, liczba_klientow, pid);
            }
        } else {
            perror("Fork failed");
            SIM_SLEEP_US(100000);
            i--;
        }
    }

    LOG_GENERATOR("Wszyscy zaplanowani klienci wygenerowani. Zamykam wejście.");
    waitSemafor(id_semafora, SEM_KASY, 0);
    int dzialaj = 0;
    signalSemafor(id_semafora, SEM_KASY);

    LOG_GENERATOR("Czekam na opuszczenie sklepu przez ostatnich klientów...\n");
    while(1) {
        waitSemafor(id_semafora, SEM_KASY, 0);
        int stan = sklep->statystyki.liczba_klientow_w_sklepie;
        int ewakuacja = sklep->statystyki.ewakuacja;
        signalSemafor(id_semafora, SEM_KASY);

        if (stan <= 0 && dzialaj == 0) {
            break;
        }
        if (ewakuacja) break;

        SIM_SLEEP_US(100000);
    }

    while(wait(NULL) > 0);

    LOG_GENERATOR("Sklep pusty, zamykam generator\n");
    return 0;

}