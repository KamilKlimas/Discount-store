//
// Created by kamil-klimas on 10.12.2025.
//

#include "ipc.h"
#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <sched.h>

int dzialaj = 1;
int id_pamieci;
PamiecDzielona *sklep;
int id_semafora;

void cleanUPPracownik() {
    if (sklep != NULL) {
        odlacz_pamiec_dzielona(sklep);
        sklep = NULL;
    }
    LOG_PRACOWNIK("Zakonczenie procesu pracownika\n");
}

void ewakuacja(int signalNum) {
    if (signalNum == SIGQUIT) {
        if (sklep != NULL && sklep->statystyki.ewakuacja == 1) {
            LOG_PRACOWNIK("EWAKUACJA! Uciekam!\n");
        } else {
            LOG_PRACOWNIK("koncze prace.\n");
        }
        _exit(0);
    }
}

int main() {
    if (signal(SIGINT, SIG_IGN) == SIG_ERR) {
        perror("signal SIGINT");
        exit(1);
    }

    atexit(cleanUPPracownik);

    setbuf(stdout, NULL);
    if (signal(SIGQUIT, ewakuacja) == SIG_ERR) {
        perror("signal SIGQUIT");
        exit(1);
    }

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

    while (dzialaj == 1) {
        for (int j = 0; j < KASY_SAMOOBSLUGOWE; j++) {
            if (waitSemafor(id_semafora, SEM_KASY, 0) == -1) {
                dzialaj = 0;
                break;
            }
            int czy_zablokowana = sklep->kasy_samo[j].zablokowana;
            int status_alko = sklep->kasy_samo[j].alkohol;
            int wiek_klienta = sklep->kasy_samo[j].wiek_klienta;
            signalSemafor(id_semafora, SEM_KASY);

            // "Co pewien losowy czas kasa się blokuje ... – wówczas konieczna jest interwencja obsługi aby odblokować kasę."
            if (czy_zablokowana == 1) {
                SIM_SLEEP_US(200000);
                if (status_alko == 1) {
                    // "Przy zakupie produktów z alkoholem konieczna weryfikacja kupującego przez obsługę (wiek>18);"
                    LOG_PRACOWNIK("Weryfikacja wieku przy kasie %d. Klient ma %d lat.\n", j, wiek_klienta);

                    if (waitSemafor(id_semafora, SEM_KASY, 0) == -1) {
                        dzialaj = 0;
                        break;
                    }
                    if (wiek_klienta < 18) {
                        // alkohol: 0 - brak, 1 - weryfikacja, 2 - zatwierdzony, -1 - odrzucony
                        LOG_PRACOWNIK("NIELETNI! Zabieram alkohol z kasy %d i odkladam na polke.\n", j);
                        sklep->kasy_samo[j].alkohol = -1;
                    } else {
                        // alkohol: 0 - brak, 1 - weryfikacja, 2 - zatwierdzony, -1 - odrzucony
                        LOG_PRACOWNIK("Pelnolatni (OK). Zatwierdzam alkohol na kasie %d.\n", j);
                        sklep->kasy_samo[j].alkohol = 2;
                    }
                    sklep->kasy_samo[j].zablokowana = 0;
                    signalSemafor(id_semafora, SEM_KASY);
                } else {
                    LOG_PRACOWNIK("Usuwam awarie techniczna przy kasie %d\n", j);
                    if (waitSemafor(id_semafora, SEM_KASY, 0) == -1) {
                        dzialaj = 0;
                        break;
                    }
                    sklep->kasy_samo[j].zablokowana = 0;
                    signalSemafor(id_semafora, SEM_KASY);
                }
            }
        }

        if (!dzialaj) {break;}

        if (sklep->czy_otwarte == 1) {
            for (int i = 0; i < sklep->liczba_produktow; i++) {
                if (waitSemafor(id_semafora, SEM_KASY, 0) == -1) {
                    dzialaj = 0;
                    break;
                }
                if (sklep->produkty[i].sztuk < 35) {
                    sklep->produkty[i].sztuk = 50;
                    LOG_PRACOWNIK("%d: Uzupelnilem %s do 50 sztuk\n", getpid(), sklep->produkty[i].nazwa);
                }
                signalSemafor(id_semafora, SEM_KASY);
            }
            SIM_SLEEP_US(100000);
        }
    }

    return 0;
}
