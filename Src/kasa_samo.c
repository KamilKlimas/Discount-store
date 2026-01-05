//
// Created by kamil-klimas on 05.1.2026.
// kasy_samo.c
//

#include "ipc.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/shm.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>

int id_kasy;
int id_pamieci;
PamiecDzielona *sklep;
int id_semafora;
int dzialaj = 1;

void cleanUp() {
    if (sklep != NULL) odlacz_pamiec_dzielona(sklep);
}

void ObslugaSygnalu(int sig) {
    if (sig == SIGQUIT) dzialaj = 0;
}

int main(int argc, char *argv[]) {
    setbuf(stdout, NULL);
    if (argc < 2) {
        printf("[BLAD] Brak ID kasy samoobsługowej\n");
        exit(1);
    }
    id_kasy = atoi(argv[1]);

    key_t klucz = ftok(FTOK_PATH, 'S');
    id_pamieci = podlacz_pamiec_dzielona();
    sklep = mapuj_pamiec_dzielona(id_pamieci);
    id_semafora = alokujSemafor(klucz, 3, 0);

    signal(SIGQUIT, ObslugaSygnalu);
    signal(SIGINT, ObslugaSygnalu);

    printf("Kasa samoobsługowa nr %d (PID: %d) gotowa.\n", id_kasy, getpid());

    waitSemafor(id_semafora, SEM_KASY, 0);
    sklep->kasy_samo[id_kasy].aktualna_kwota = 0.0;
    signalSemafor(id_semafora, SEM_KASY);

    while (dzialaj) {
        if (sklep->statystyki.ewakuacja) {
            printf("[Kasa %d] EWAKUACJA! Zamykam terminal.\n", id_kasy);
            break;
        }

        waitSemafor(id_semafora, SEM_KASY, 0);
        int otwarta = sklep->kasy_samo[id_kasy].otwarta;
        int zablokowana = sklep->kasy_samo[id_kasy].zablokowana;
        float wplata = sklep->kasy_samo[id_kasy].aktualna_kwota;
        pid_t klient_pid = sklep->kasy_samo[id_kasy].obslugiwany_klient;
        signalSemafor(id_semafora, SEM_KASY);

        if (otwarta==0) {
            sleep(1);
            continue;
        }


        if (zablokowana) {
            printf("[Kasa %d] AWARIA/WERYFIKACJA! Czekam na pomoc...\n", id_kasy);
            sleep(1);
            continue;
        }

        if (wplata > 0.0) {
            printf("[Kasa %d] Przetwarzam płatność od klienta PID %d: %.2f PLN\n", id_kasy, klient_pid, wplata);
            sleep(1);

            waitSemafor(id_semafora, SEM_UTARG, 0);
            sklep->statystyki.utarg += wplata;
            sklep->statystyki.liczba_obsluzonych_klientow++;
            signalSemafor(id_semafora, SEM_UTARG);

            waitSemafor(id_semafora, SEM_KASY, 0);
            sklep->kasy_samo[id_kasy].aktualna_kwota = 0.0;
            printf("[Kasa %d] Płatność zaakceptowana. Drukuję paragon.\n", id_kasy);
            signalSemafor(id_semafora, SEM_KASY);
        }

        usleep(200000);
    }

    cleanUp();
    return 0;
}