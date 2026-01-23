//
// Created by kamil-klimas on 10.12.2025.
//

#include <signal.h>
#include "ipc.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/shm.h>
#include <time.h>
#include <unistd.h>
#include <sched.h>
#include <errno.h>

int id_pamieci;
int id_semafora;
int id_kolejki;
PamiecDzielona *sklep;
int pid;

int czas;
int indeks;
int alkohol = 0;
int alkohol_lista[] = {16, 17, 18, 19};

static volatile sig_atomic_t wymuszone_wyjscie = 0;

static int wejscie_zarejestrowane = 0;
static int w_kolejce_samo = 0;
static int w_kolejce_stacjo = 0;
static int kasa_stacjo_idx = -1;
static int klient_aktywny = 0;

static char **paragon = NULL;
static int *koszyk_id = NULL;
static int ile_prod = 0;

int exists(int arr[], int size, int value) {
    for (int i = 0; i < size; i++) {
        if (arr[i] == value) {
            return 1;
        }
    }
    return 0;
}

void Uciekaj(int sig) {
    (void) sig;
    wymuszone_wyjscie = 1;
}

static void CleanupAndExit(int code) {
    if (sklep != NULL) {
        if (w_kolejce_samo) {
            if (waitSemafor(id_semafora, SEM_KOLEJKI, 0) != -1) {
                usunZSrodkaKolejkiFIFO(&sklep->kolejka_samoobsluga, pid);
                signalSemafor(id_semafora, SEM_KOLEJKI);
            }
            w_kolejce_samo = 0;
        }

        if (w_kolejce_stacjo && kasa_stacjo_idx >= 0) {
            if (waitSemafor(id_semafora, SEM_KOLEJKI, 0) != -1) {
                usunZSrodkaKolejkiFIFO(&sklep->kolejka_stato[kasa_stacjo_idx], pid);
                signalSemafor(id_semafora, SEM_KOLEJKI);
            }
            w_kolejce_stacjo = 0;
        }

        if (wejscie_zarejestrowane) {
            if (waitSemafor(id_semafora, SEM_KASY, 0) != -1) {
                sklep->statystyki.liczba_klientow_w_sklepie--;
                signalSemafor(id_semafora, SEM_KASY);
            }
            if (klient_aktywny) {
                waitSemafor(id_semafora, SEM_KLIENCI, 0);
                klient_aktywny = 0;
            }
            signalSemafor(id_semafora, SEM_WEJSCIE);
            wejscie_zarejestrowane = 0;
        }
    }

    if (paragon) {
        free(paragon);
        paragon = NULL;
    }
    if (koszyk_id) {
        free(koszyk_id);
        koszyk_id = NULL;
    }
    if (sklep != NULL) {
        odlacz_pamiec_dzielona(sklep);
        sklep = NULL;
    }
    exit(code);
}

void WypiszParagon(char **paragon, int *koszyk_id, int ile_prod, double finalna_kwota) {
    char bufor[2048];
    int offset = 0;

    bufor[0] = '\0';
    offset += snprintf(bufor + offset, sizeof(bufor) - offset, "PARAGON: [ ");

    int first = 1;
    for (int i = 0; i < ile_prod; i++) {
        if (koszyk_id[i] != -1 && paragon[i] != NULL) {
            if (!first) offset += snprintf(bufor + offset, sizeof(bufor) - offset, ", ");
            offset += snprintf(bufor + offset, sizeof(bufor) - offset, "%s", paragon[i]);
            first = 0;
        }
    }
    snprintf(bufor + offset, sizeof(bufor) - offset, " ] SUMA: %.2f zl", finalna_kwota);

    LOG_KLIENT(pid, "%s\n", bufor);
}

int main() {
    setbuf(stdout, NULL);

    if (signal(SIGINT, SIG_IGN) == SIG_ERR) {
        perror("signal SIGINT");
        exit(1);
    }

    pid = getpid();
    srand(pid ^ time(NULL));

    key_t klucz = ftok("/tmp/dyskont_projekt", 'S');
    if (klucz == -1) {
        if (errno == ENOENT) {
            exit(0);
        }
        perror("\nbrak argumentu od kierownika\n");
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

    if (signal(SIGQUIT, Uciekaj) == SIG_ERR) {
        perror("signal SIGQUIT");
        exit(1);
    }

    while (1) {
        struct sembuf op;
        op.sem_num = SEM_WEJSCIE;
        op.sem_op = -1;
        op.sem_flg = SEM_UNDO;

        if (semop(id_semafora, &op, 1) == 0) {
            break;
        }
        if (errno == EINTR) {
            if (wymuszone_wyjscie) {
                CleanupAndExit(0);
            }
            continue;
        }
        if (errno == EIDRM || errno == EINVAL) {
            CleanupAndExit(0);
        }
    }

    waitSemafor(id_semafora, SEM_KASY, 0);
    int otwarte = sklep->czy_otwarte;
    int ewakuacja = sklep->statystyki.ewakuacja;
    signalSemafor(id_semafora, SEM_KASY);

    if (!otwarte || ewakuacja) {
        signalSemafor(id_semafora, SEM_WEJSCIE);
        CleanupAndExit(0);
    }

    waitSemafor(id_semafora, SEM_KASY, 0);
    sklep->statystyki.liczba_klientow_w_sklepie += 1;
    signalSemafor(id_semafora, SEM_KASY);
    wejscie_zarejestrowane = 1;
    if (signalSemafor(id_semafora, SEM_KLIENCI) != -1) {
        klient_aktywny = 1;
    }

    // "...robiąc zakupy – od 3 do 10 produktów różnych kategorii (owoce, warzywa, pieczywo, nabiał, alkohol, wędliny, …)."
    ile_prod = (rand() % 10) + 1;
    paragon = malloc(ile_prod * sizeof(char*));
    koszyk_id = malloc(ile_prod * sizeof(int));
    if (paragon == NULL || koszyk_id == NULL) {
        perror("Brak pamieci");
        CleanupAndExit(1);
    }
    int wiek = (rand()%57) + 14; //14-70 lat
    double moj_rachunek = 0.0;

    SIM_SLEEP_US(rand()%2000000 + 1000000);


    for (int i =0; i < ile_prod;i++){
        indeks = rand()%LICZBA_PRODUKTOW;
        waitSemafor(id_semafora, SEM_KASY, 0);
        if (sklep->produkty[indeks].sztuk >0){
            sklep->produkty[indeks].sztuk -=1;
            if (exists(alkohol_lista, 4, indeks)){alkohol = 1;}

            moj_rachunek+= sklep->produkty[indeks].cena;
            paragon[i] = sklep->produkty[indeks].nazwa;
            koszyk_id[i] = indeks;

        }else
        {
            koszyk_id[i] = -1;
            paragon[i] = NULL;
        }

        signalSemafor(id_semafora, SEM_KASY);
        SIM_SLEEP_US(50000);
    }

    // "Większość klientów korzysta z kas samoobsługowych (ok. 95%), pozostali (ok. 5%) stają w kolejce do kas stacjonarnych."
    int tryb = (rand() % 100 < SZANSA_SAMOOBSLUGA) ? 1 : 2;
    int obsluzony = 0;
    int nr_kasy = -1;

    //samoobsluga
    if (tryb == 1) {
        waitSemafor(id_semafora, SEM_KOLEJKI, 0);
        // "Do wszystkich kas samoobsługowych jest jedna kolejka;"
        dodajDoKolejkiFIFO(&sklep->kolejka_samoobsluga, pid);
        signalSemafor(id_semafora, SEM_KOLEJKI);
        w_kolejce_samo = 1;

        time_t start_wait = time(NULL);
        while (1) {
            waitSemafor(id_semafora, SEM_KASY, 0);
            int ewak = sklep->statystyki.ewakuacja;
            signalSemafor(id_semafora, SEM_KASY);

            if (ewak) {
                waitSemafor(id_semafora, SEM_KOLEJKI, 0);
                usunZSrodkaKolejkiFIFO(&sklep->kolejka_samoobsluga, pid);
                signalSemafor(id_semafora, SEM_KOLEJKI);
                w_kolejce_samo = 0;
                CleanupAndExit(0);
            }

            waitSemafor(id_semafora, SEM_KOLEJKI, 0);
            pid_t pierwwszy = podejrzyjPierwszegoFIFO(&sklep->kolejka_samoobsluga);
            //<-wspolna kolejka do kas samoobslugowych (sprawdzenie czy jest 1)
            signalSemafor(id_semafora, SEM_KOLEJKI);

            if (pierwwszy == pid) {
                // "Klient podchodzi do pierwszej wolnej kasy;"
                waitSemafor(id_semafora,SEM_KASY, 0); //<- wybor kasy (szukanie PIERWSZEJ wolnej)
                for (int i = 0; i < KASY_SAMOOBSLUGOWE; i++) {
                    if (sklep->kasy_samo[i].otwarta == 1 && sklep->kasy_samo[i].zajeta == 0) {
                        sklep->kasy_samo[i].zajeta = 1;
                        nr_kasy = i;
                        break;
                    }
                }
                signalSemafor(id_semafora, SEM_KASY);

                if (nr_kasy != -1) {
                    waitSemafor(id_semafora, SEM_KOLEJKI, 0);
                    zdejmijZKolejkiFIFO(&sklep->kolejka_samoobsluga); //<- jesli znalazł to wychodzi z kolejki
                    signalSemafor(id_semafora, SEM_KOLEJKI);
                    w_kolejce_samo = 0;
                    break;
                }
            }

            // "Jeżeli czas oczekiwania w kolejce na kasę samoobsługową jest dłuższy niż T, klient może przejść do kasy stacjonarnej jeżeli jest otwarta;"
            if (difftime(time(NULL), start_wait) > MAX_CZAS_OCZEKIWANIA) //zmiana trybu ze wzgledu na czas oczekiwania
            {
                int stacjo_otwarta = 0;
                waitSemafor(id_semafora, SEM_KASY, 0);
                if (sklep->kasa_stato[0].otwarta || sklep->kasa_stato[1].otwarta) stacjo_otwarta = 1;
                signalSemafor(id_semafora,SEM_KASY);

                if (stacjo_otwarta) {
                    //LOG_KLIENT(pid, "czekam za dlugo (>%ds)! Ide do stacjonarnej.\n",MAX_CZAS_OCZEKIWANIA);
                    waitSemafor(id_semafora, SEM_KOLEJKI, 0);
                    //tryb = 2 <- jesli czas oczekuwania jest dluzszy niz T to klient idzie do kasy stacjonarnej
                    usunZSrodkaKolejkiFIFO(&sklep->kolejka_samoobsluga, pid);
                    signalSemafor(id_semafora, SEM_KOLEJKI);
                    w_kolejce_samo = 0;
                    tryb = 2;
                    break;
                }
            }
            if (wymuszone_wyjscie) {
                CleanupAndExit(0);
            }
            SIM_SLEEP_US(200000);
        }
    }

    //platnosc samoobsluga
    if (tryb == 1 && nr_kasy != -1) {
        int awaria = rand() % 100;
        LOG_KLIENT(pid, "Przy kasie samo nr %d.\n", nr_kasy);
        // "Co pewien losowy czas kasa się blokuje (np. waga towaru nie zgadza się z wyborem klienta) – wówczas konieczna jest interwencja obsługi aby odblokować kasę."
        if ((awaria > 90) || alkohol) {
            waitSemafor(id_semafora, SEM_KASY, 0);
            //<- weryfikacja wieku klienta jesli w jego zakupach znajduje sie alkohol
            // "Przy zakupie produktów z alkoholem konieczna weryfikacja kupującego przez obsługę (wiek>18);"
            if (alkohol == 1) {
                // alkohol: 0 - brak, 1 - weryfikacja, 2 - zatwierdzony, -1 - odrzucony
                sklep->kasy_samo[nr_kasy].zablokowana = 1;
                sklep->kasy_samo[nr_kasy].alkohol = 1;
                sklep->kasy_samo[nr_kasy].wiek_klienta = wiek;
                LOG_KLIENT(pid, "Weryfikacja wieku na kasie %d (Alkohol w koszyku)\n", nr_kasy);
            }
            if (awaria > 90) //blokada kasy samoobslugowej
            {
                sklep->kasy_samo[nr_kasy].zablokowana = 1;
                LOG_KLIENT(pid, "Kasa %d zablokowana, czekaj na obsluge\n", nr_kasy);
            }
            signalSemafor(id_semafora, SEM_KASY);

            //czekanie na interwencje pracownik.c zeby zmienił flage z .zablokowana = 1 na 0
            while (1) {
                waitSemafor(id_semafora, SEM_KASY, 0);
                int blok = sklep->kasy_samo[nr_kasy].zablokowana;
                int ewak = sklep->statystyki.ewakuacja;
                signalSemafor(id_semafora, SEM_KASY);

                if (!blok) break;
                if (ewak) {
                    waitSemafor(id_semafora, SEM_KASY, 0);
                    sklep->kasy_samo[nr_kasy].zajeta = 0;
                    sklep->kasy_samo[nr_kasy].zablokowana = 0;
                    signalSemafor(id_semafora, SEM_KASY);
                    CleanupAndExit(0);
                }

                if (wymuszone_wyjscie) {
                    CleanupAndExit(0);
                }

                SIM_SLEEP_US(200000);
            }
        }

        //<-sprawdzenie czy klient jest zmuszony oddac alkohol
        waitSemafor(id_semafora, SEM_KASY, 0);
        int status_alko = sklep->kasy_samo[nr_kasy].alkohol;

        if (alkohol == 1 && status_alko == -1) //<- (-1) oznacza odrzucenie przez pracownika
        {
            LOG_KLIENT(pid, "Odmowa! Oddaje alkohol, kupuje reszte.\n");

            for (int k = 0; k < ile_prod; k++) {
                int id_prod = koszyk_id[k];
                if (id_prod != -1 && exists(alkohol_lista, 4, id_prod)) {
                    sklep->produkty[id_prod].sztuk += 1;
                    moj_rachunek -= sklep->produkty[id_prod].cena;
                    koszyk_id[k] = -1;
                }
            }
            if (moj_rachunek < 0) moj_rachunek = 0;
            alkohol = 0;
        }
        sklep->kasy_samo[nr_kasy].alkohol = 0;
        signalSemafor(id_semafora, SEM_KASY);

        waitSemafor(id_semafora, SEM_KASY, 0);
        if (sklep->statystyki.ewakuacja || wymuszone_wyjscie) {
            signalSemafor(id_semafora, SEM_KASY);
            CleanupAndExit(0);
        }
        signalSemafor(id_semafora, SEM_KASY);
        LOG_KLIENT(pid, "Kasa odblokowana, kontynuuje.\n");

        if (moj_rachunek > 0.001) {
            waitSemafor(id_semafora, SEM_KASY, 0);
            sklep->kasy_samo[nr_kasy].obslugiwany_klient = pid;
            sklep->kasy_samo[nr_kasy].aktualna_kwota = moj_rachunek;
            signalSemafor(id_semafora, SEM_KASY);

            while (1) {
                waitSemafor(id_semafora, SEM_KASY, 0);
                float czy_juz = sklep->kasy_samo[nr_kasy].aktualna_kwota;
                int ewak = sklep->statystyki.ewakuacja;
                signalSemafor(id_semafora, SEM_KASY);

                waitSemafor(id_semafora, SEM_KASY, 0);
                if (sklep->statystyki.ewakuacja || wymuszone_wyjscie) {
                    signalSemafor(id_semafora, SEM_KASY);
                    CleanupAndExit(0);
                }
                signalSemafor(id_semafora, SEM_KASY);

                if (czy_juz == 0.0) {
                    LOG_KLIENT(pid, "Terminal potwierdzil platnosc. Zabieram paragon.\n");
                    break;
                }
                if (ewak) {
                    waitSemafor(id_semafora, SEM_KASY, 0);
                    sklep->kasy_samo[nr_kasy].zajeta = 0;
                    sklep->kasy_samo[nr_kasy].aktualna_kwota = 0;
                    signalSemafor(id_semafora, SEM_KASY);
                    CleanupAndExit(0);
                }

                if (wymuszone_wyjscie) {
                    CleanupAndExit(0);
                }


                SIM_SLEEP_US(100000);
            }
        } else {
            waitSemafor(id_semafora, SEM_UTARG, 0);
            sklep->statystyki.liczba_obsluzonych_klientow++;
            signalSemafor(id_semafora, SEM_UTARG);
        }

        // "Klient kasuje produkty, płaci kartą i otrzymuje wydruk (raport) z listą zakupów i zapłaconą kwotą ;"
        if (moj_rachunek > 0.001) {

            waitSemafor(id_semafora, SEM_KASY, 0);
            if (sklep->statystyki.ewakuacja || wymuszone_wyjscie) {
                signalSemafor(id_semafora, SEM_KASY);
                CleanupAndExit(0);
            }
            signalSemafor(id_semafora, SEM_KASY);

            waitSemafor(id_semafora, SEM_KASY, 0);
            WypiszParagon(paragon, koszyk_id, ile_prod, moj_rachunek);
            signalSemafor(id_semafora, SEM_KASY);
        }

        waitSemafor(id_semafora, SEM_KASY, 0);
        sklep->kasy_samo[nr_kasy].zajeta = 0;
        obsluzony = 1;
        signalSemafor(id_semafora, SEM_KASY);
    }

    //platnosc stacjonarna
    if (tryb == 2 && !obsluzony) {
        // "Jeżeli kasa 2 jest otwarta, to każdej z kas tworzy się osobna kolejka klientów (klienci z kolejki do kasy 1 mogą przejść do kolejki do kasy 2);"
        // Wybierz kolejke (tam gdzie krocej) lub domyslnie K1, jesli K2 otwarta to mozna przejsc
        int wybrana_kasa = 0;

        waitSemafor(id_semafora, SEM_KOLEJKI, 0);
        if (sklep->kasa_stato[1].otwarta) {
            //zmiana decyzji o wyborze kasy ze wzgledu na dlugosc kolejki (domyslnie kasa 1)
            if (sklep->kolejka_stato[1].rozmiar < sklep->kolejka_stato[0].rozmiar) wybrana_kasa = 1;
        }
        dodajDoKolejkiFIFO(&sklep->kolejka_stato[wybrana_kasa], pid);
        signalSemafor(id_semafora, SEM_KOLEJKI);
        w_kolejce_stacjo = 1;
        kasa_stacjo_idx = wybrana_kasa;

        waitSemafor(id_semafora, SEM_KASY, 0);
        if (sklep->statystyki.ewakuacja || wymuszone_wyjscie) {
            signalSemafor(id_semafora, SEM_KASY);
            CleanupAndExit(0);
        }
        signalSemafor(id_semafora, SEM_KASY);

        LOG_KLIENT(pid, "W kolejce do stacjonarnej %d\n", wybrana_kasa + 1);

        struct messg_buffer msg;

        while (1) {
            int wynik = msgrcv(id_kolejki, &msg, sizeof(msg) - sizeof(long), (long) pid, IPC_NOWAIT);
            if (wynik != -1) {
                if (msg.kwota == 0) {
                    break;
                }
                continue;
            }
            if (errno == EIDRM || errno == EINVAL) {
                CleanupAndExit(0);
            }
            if (errno != ENOMSG && errno != EINTR) {
                perror("msgrcv");
                CleanupAndExit(0);
            }

            //Mozliwosc zmiany kolejki jesli sasiad otwarty oraz jego kolejka mniejsza o 1
            waitSemafor(id_semafora, SEM_KOLEJKI, 0);
            int sasiad = (wybrana_kasa == 0) ? 1 : 0;
            int rozmiar_mojej = sklep->kolejka_stato[wybrana_kasa].rozmiar;
            int rozmiar_sasiada = sklep->kolejka_stato[sasiad].rozmiar;
            int czy_otwarta_sasiad = (sasiad == 0) ? sklep->kasa_stato[0].otwarta : sklep->kasa_stato[1].otwarta;
            int czy_moja_otwarta = sklep->kasa_stato[wybrana_kasa].otwarta;

            if (czy_otwarta_sasiad) {
                int oplaca_sie = (rozmiar_sasiada < (rozmiar_mojej - 1));
                int uciekam = (!czy_moja_otwarta && rozmiar_mojej > 0);

                if (!czy_moja_otwarta && rozmiar_mojej > 0) uciekam = 1;
                if (oplaca_sie || uciekam) {
                    if (usunZSrodkaKolejkiFIFO(&sklep->kolejka_stato[wybrana_kasa], pid) == 1) {
                        wybrana_kasa = sasiad;
                        dodajDoKolejkiFIFO(&sklep->kolejka_stato[wybrana_kasa], pid);
                        kasa_stacjo_idx = wybrana_kasa;

                        char *powod = czy_moja_otwarta ? "krotsza kolejka" : "moja zamknieta";
                        //
                        LOG_KLIENT(pid, "Zmieniam kolejke! (Powod: %s) Ide do kasy %d\n", powod, wybrana_kasa + 1);
                    }
                }
            }
            signalSemafor(id_semafora, SEM_KOLEJKI);

            waitSemafor(id_semafora, SEM_KASY, 0);
            if (sklep->statystyki.ewakuacja) {
                signalSemafor(id_semafora, SEM_KASY);
                CleanupAndExit(0);
            }
            signalSemafor(id_semafora, SEM_KASY);

            if (wymuszone_wyjscie) {
                CleanupAndExit(0);
            }

            SIM_SLEEP_US(200000);
        }
        w_kolejce_stacjo = 0;

        // Odbieram wiadomosc skierowana do pid klienta
        if (msg.kwota == 0) {
            // Zaproszenie
            int id_kasjera = msg.ID_klienta;
            // Wysylam paragon
            msg.mesg_type = id_kasjera + KANAL_KASJERA_OFFSET;
            msg.ID_klienta = pid;
            msg.kwota = moj_rachunek;
            msg.wiek = wiek;
            msg.ma_alkohol = alkohol;
            BezpieczneWyslanieKlienta(id_kolejki, &msg);

            OdbierzZKolejki(id_kolejki, &msg, (long) pid);

            if (msg.kwota == -2.0) {
                LOG_KLIENT(pid, "Kasjer odmowil sprzedazy alkoholu! Oddaje na polke.\n");

                waitSemafor(id_semafora, SEM_KASY, 0);
                for (int k = 0; k < ile_prod; k++) {
                    int id_prod = koszyk_id[k];
                    if (id_prod != -1 && exists(alkohol_lista, 4, id_prod)) {
                        sklep->produkty[id_prod].sztuk += 1;
                        moj_rachunek -= sklep->produkty[id_prod].cena;
                        koszyk_id[k] = -1;
                    }
                }
                if (moj_rachunek < 0) moj_rachunek = 0;
                alkohol = 0;
                signalSemafor(id_semafora, SEM_KASY);

                // Poprawiony rachunek wyslany do kasjera (bez alko)
                msg.mesg_type = id_kasjera + KANAL_KASJERA_OFFSET;
                msg.ID_klienta = pid;
                msg.kwota = moj_rachunek;
                msg.wiek = wiek;
                msg.ma_alkohol = 0;
                BezpieczneWyslanieKlienta(id_kolejki, &msg);

                OdbierzZKolejki(id_kolejki, &msg, (long) pid);
            }
            if (moj_rachunek > 0.001) {

                waitSemafor(id_semafora, SEM_KASY, 0);
                if (sklep->statystyki.ewakuacja || wymuszone_wyjscie) {
                    signalSemafor(id_semafora, SEM_KASY);
                    CleanupAndExit(0);
                }
                signalSemafor(id_semafora, SEM_KASY);

                waitSemafor(id_semafora, SEM_KASY, 0);
                WypiszParagon(paragon, koszyk_id, ile_prod, moj_rachunek);
                signalSemafor(id_semafora, SEM_KASY);
            }
        }
    }
    w_kolejce_stacjo = 0;
    CleanupAndExit(0);
}
