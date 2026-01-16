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
volatile sig_atomic_t dzialaj = 1;

void cleanUp() {
    if (sklep != NULL) odlacz_pamiec_dzielona(sklep);
}

void ObslugaSygnalu(int sig) {
    if (sig == SIGQUIT) dzialaj = 0;
}

int main(int argc, char *argv[]) {

    signal(SIGINT, SIG_IGN);

    setbuf(stdout, NULL);

    if (argc < 2) {
        LOG_KASA_SAMO(id_kasy+1,"[BLAD] Brak ID kasy samoobsługowej \n");
        exit(1);
    }
    id_kasy = atoi(argv[1]);

    key_t klucz = ftok(FTOK_PATH, 'S');
    id_pamieci = podlacz_pamiec_dzielona();
    sklep = mapuj_pamiec_dzielona(id_pamieci);
    id_semafora = alokujSemafor(klucz, 3, 0);

    signal(SIGQUIT, ObslugaSygnalu);

    LOG_KASA_SAMO(id_kasy+1,"(PID: %d) gotowa.\n", getpid());
    waitSemafor(id_semafora, SEM_KASY, 0);
    sklep->kasy_samo[id_kasy].aktualna_kwota = 0.0;
    signalSemafor(id_semafora, SEM_KASY);

    int zgloszono_problem = 0;

    while (dzialaj) {
        if (sklep->statystyki.ewakuacja) {
            LOG_KASA_SAMO(id_kasy+1,"EWAKUACJA! Zamykam terminal.\n");
            break;
        }

        waitSemafor(id_semafora, SEM_KASY, 0);
        int otwarta = sklep->kasy_samo[id_kasy].otwarta;
        int zablokowana = sklep->kasy_samo[id_kasy].zablokowana;
        float wplata = sklep->kasy_samo[id_kasy].aktualna_kwota;
        pid_t klient_pid = sklep->kasy_samo[id_kasy].obslugiwany_klient;
        signalSemafor(id_semafora, SEM_KASY);

        if (otwarta==0) {
            SIM_SLEEP_S(1);
            continue;
        }

        //symulacja zgloszenia awarii/alkoholu
        if (zablokowana) {
            if (zgloszono_problem == 0)
            {
                if (sklep->kasy_samo[id_kasy].alkohol)
                {
                    LOG_KASA_SAMO(id_kasy+1, "WERYFIKACJA WIEKU!\n");
                }else
                {
                    LOG_KASA_SAMO(id_kasy+1, "AWARIA! Czekam na pomoc...\n");
                }
                zgloszono_problem = 1;
            }
            SIM_SLEEP_S(1);
            continue;
        }else
        {
            zgloszono_problem = 0;
        }

        if (wplata > 0.0) {
            LOG_KASA_SAMO(id_kasy+1, "Przetwarzam platnosc od klienta PID %d: %.2f zl\n",klient_pid, wplata);
            SIM_SLEEP_S(1);

            waitSemafor(id_semafora, SEM_UTARG, 0);
            sklep->statystyki.utarg += wplata;
            sklep->statystyki.liczba_obsluzonych_klientow++;
            signalSemafor(id_semafora, SEM_UTARG);

            waitSemafor(id_semafora, SEM_KASY, 0);
            sklep->kasy_samo[id_kasy].aktualna_kwota = 0.0;
            LOG_KASA_SAMO(id_kasy+1, "Platnosc zaakceptowana. Drukuje paragon.\n");
            signalSemafor(id_semafora, SEM_KASY);
        }

        SIM_SLEEP_S(1);
    }

    cleanUp();
    return 0;
}