//
// Created by kamil-klimas on 10.12.2025.
//

#include "ipc.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/shm.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sched.h>

int czyDziala = 1;
volatile int status_pracy = 0;

int moje_id;
int id_pamieci;
int id_semafora;
int id_kolejki;
PamiecDzielona *sklep;

void cleanUpKasy() {
    if (sklep != NULL) {
        if (moje_id >= 0 && moje_id < KASY_STACJONARNE) {
            sklep->kasa_stato[moje_id].otwarta = 0;
        }
        odlacz_pamiec_dzielona(sklep);
        LOG_KASJER(moje_id + 1, "konczy zmiane");
    }
}

void ObslugaSygnalu(int sig) {
    if (sig == SIGUSR1) {
        status_pracy = 1;
    } else if (sig == SIGUSR2) {
        LOG_KASJER(moje_id+1, "Zamykam kase na polecenie kierownika\n");
    } else if (sig == SIGQUIT) {
        _exit(0);

    }
}

int main(int argc, char *argv[]) {
    setbuf(stdout, NULL);
    if (signal(SIGINT, SIG_IGN) == SIG_ERR) {
        perror("signal SIGINT");
        exit(1);
    }
    if (signal(SIGUSR1, ObslugaSygnalu) == SIG_ERR) {
        perror("signal SIGUSR1");
        exit(1);
    }
    if (signal(SIGUSR2, ObslugaSygnalu) == SIG_ERR) {
        perror("signal SIGUSR2");
        exit(1);
    }
    if (signal(SIGQUIT, ObslugaSygnalu) == SIG_ERR) {
        perror("signal SIGQUIT");
        exit(1);
    }


    atexit(cleanUpKasy);

    if (argc < 2) {
        LOG_SYSTEM("brak argumentu od kierownika\n");
        exit(1);
    }

    moje_id = atoi(argv[1]);
    key_t klucz = ftok("/tmp/dyskont_projekt", 'S');
    if (klucz == -1) {
        perror("\nblad ftok\n");
        exit(1);
    }

    id_pamieci = podlacz_pamiec_dzielona();
    if (id_pamieci == -1) {
        perror("\nblad podlacz_pamiec_dzielona\n");
        exit(1);
    }
    sklep = mapuj_pamiec_dzielona(id_pamieci);

    id_semafora = alokujSemafor(klucz, 5, 0);
    id_kolejki = stworzKolejke();

    struct messg_buffer msg;
    long moj_typ_nasluchu = moje_id + KANAL_KASJERA_OFFSET;

    waitSemafor(id_semafora, SEM_KASY, 0);
    status_pracy = sklep->kasa_stato[moje_id].otwarta;
    signalSemafor(id_semafora,SEM_KASY);

    LOG_KASJER(moje_id + 1, "zaczyna prace (typ nasluchu: %ld)\n", moj_typ_nasluchu);
    while (1) {
        waitSemafor(id_semafora, SEM_KASY, 0);
        int zamykanie = sklep->kasa_stato[moje_id].zamykanie_w_toku;
        int limit = sklep->kasa_stato[moje_id].liczba_do_obsluzenia;
        signalSemafor(id_semafora, SEM_KASY);


        // "Jeśli w kolejce do kasy czekali klienci (przed ogłoszeniem decyzji o jej zamknięciu) to powinni zostać obsłużeni przez tę kasę."
        if (zamykanie) {
            if (limit <= 0) {
                //dziala na zasadzie: sygnal zamkniecia
                status_pracy = 0; //zapisz ile teraz w kolejce -> (liczba_do_obsluzenia) =limit
                waitSemafor(id_semafora, SEM_KASY, 0); //obsluguj dopoki limit <= 0
                sklep->kasa_stato[moje_id].otwarta = 0;
                sklep->kasa_stato[moje_id].zamykanie_w_toku = 0;
                signalSemafor(id_semafora,SEM_KASY);
            }
        } else {
            waitSemafor(id_semafora, SEM_KASY, 0);
            status_pracy = sklep->kasa_stato[moje_id].otwarta;
            signalSemafor(id_semafora, SEM_KASY);
        }

        if (status_pracy == 0) {
            SIM_SLEEP_US(100000);
            continue;
        }

        // "Jeśli w kolejce do kasy czekali klienci (przed ogłoszeniem decyzji o jej zamknięciu) to powinni zostać obsłużeni przez tę kasę."
        pid_t klient_pid = 0;
        waitSemafor(id_semafora, SEM_KOLEJKI, 0);
        if (sklep->kolejka_stato[moje_id].rozmiar > 0) {
            klient_pid = zdejmijZKolejkiFIFO(&sklep->kolejka_stato[moje_id]);
        }
        signalSemafor(id_semafora, SEM_KOLEJKI);

        if (klient_pid > 0) {
            if (zamykanie) {
                waitSemafor(id_semafora, SEM_KASY, 0);
                sklep->kasa_stato[moje_id].liczba_do_obsluzenia -= 1;
                signalSemafor(id_semafora, SEM_KASY);
            }

            LOG_KASJER(moje_id+1, "Wolam klienta %d do kasy.\n", klient_pid);

            waitSemafor(id_semafora, SEM_KASY, 0);
            sklep->kasa_stato[moje_id].zajeta = 1;
            sklep->kasa_stato[moje_id].obslugiwany_klientl = klient_pid;
            signalSemafor(id_semafora, SEM_KASY);

            //wezwanie klienta
            msg.mesg_type = (long) klient_pid;
            msg.ID_klienta = moje_id;
            msg.kwota = 0;
            WyslijDoKolejki(id_kolejki, &msg);

            //oczekiwanie na paragon
            OdbierzZKolejki(id_kolejki, &msg, moj_typ_nasluchu);

            // "Przy zakupie produktów z alkoholem konieczna weryfikacja kupującego przez obsługę (wiek>18);"
            if (msg.ma_alkohol == 1 && msg.wiek < 18) {
                LOG_KASJER(moje_id + 1, "Klient %d jest nieletni (%d lat)! Odmawiam sprzedazy alkoholu.",
                msg.ID_klienta, msg.wiek);
                msg.mesg_type = (long) klient_pid;
                msg.kwota = -2.0; //kod odrzucenia
                WyslijDoKolejki(id_kolejki, &msg);

                OdbierzZKolejki(id_kolejki, &msg, moj_typ_nasluchu);

                LOG_KASJER(moje_id + 1, "Klient %d skorygowal zakupy. Nowa kwota: %.2f", msg.ID_klienta, msg.kwota);
            } else if (msg.ma_alkohol == 1) {
                LOG_KASJER(moje_id + 1, "Weryfikacja wieku pomyslna (%d lat). Sprzedaje alkohol.", msg.wiek);
            }

            LOG_KASJER(moje_id +1, "Zakupy od Klienta %d na: %.2f zl\n", msg.ID_klienta, msg.kwota);

            SIM_SLEEP_US(rand() % 500000 + 500000);

            waitSemafor(id_semafora, SEM_UTARG, 0);
            sklep->statystyki.utarg += msg.kwota;
            sklep->statystyki.liczba_obsluzonych_klientow += 1;
            signalSemafor(id_semafora, SEM_UTARG);

            // Potwierdzenie zaplaty
            msg.mesg_type = (long) klient_pid;
            msg.kwota = -1.0;
            WyslijDoKolejki(id_kolejki, &msg);

            LOG_KASJER(moje_id+1, "obsluzylem klienta %d\n", msg.ID_klienta);
            waitSemafor(id_semafora, SEM_KASY, 0);
            sklep->kasa_stato[moje_id].zajeta = 0;
            sklep->kasa_stato[moje_id].czas_ostatniej_obslugi = time(NULL);
            signalSemafor(id_semafora, SEM_KASY);
        } else {
            SIM_SLEEP_US(100000);
        }
    }
    return 0;
}
