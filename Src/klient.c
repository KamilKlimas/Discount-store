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

int id_pamieci;
int id_semafora;
int id_kolejki;
PamiecDzielona *sklep;
int pid;
int czas;

int indeks;

int alkohol = 0;
int alkohol_lista[] = {16,17,18,19};

int exists(int arr[], int size, int value)
{
    for (int i = 0; i < size; i++)
    {
        if (arr[i] == value)
        {
            return 1;
        }
    }
    return 0;
}

void Uciekaj(int sig) {
    (void)sig;
    LOG_KLIENT(pid, "Slysze alarm! Rzucam koszyk i uciekam ze sklepu!\n");
    exit(0);
}

int main()
{
    setbuf(stdout, NULL);

    signal(SIGINT, SIG_IGN);

    pid = getpid();
    srand(pid^czas);

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
    id_semafora = alokujSemafor(klucz, 3, 0);

    id_kolejki = stworzKolejke();

    signal(SIGQUIT, Uciekaj);

    waitSemafor(id_semafora, SEM_KASY, 0);
    sklep ->statystyki.liczba_klientow_w_sklepie += 1;
    signalSemafor(id_semafora, SEM_KASY);

    int ile_prod = (rand() % 8) + 1;
    char **paragon = malloc(ile_prod * sizeof(char*));
    char *koszyk_id = malloc(ile_prod * sizeof(int));
    if (paragon == NULL || koszyk_id == NULL) {
        perror("Brak pamieci");
        exit(1);
    }
    int wiek = (rand()%57) + 14; //14-70 lat
    double moj_rachunek = 0.0;

    usleep(rand()%2000000 + 1000000); //1-3 sekundy chodzenia


    for (int i =0; i < ile_prod;i++){
        indeks = rand()%LICZBA_PRODUKTOW;
        waitSemafor(id_semafora, SEM_KASY, 0);
        if (sklep->produkty[indeks].sztuk >0){
            sklep->produkty[indeks].sztuk -=1;
            if (exists(alkohol_lista, 4, indeks))
            {
                alkohol = 1;
            }

            moj_rachunek+= sklep->produkty[indeks].cena;
            //LOG_KLIENT(pid,"Zostalo [%d] sztuk [%s]\n", sklep->produkty[indeks].sztuk, sklep->produkty[indeks].nazwa);
            paragon[i] = sklep->produkty[indeks].nazwa;
            koszyk_id[i] = indeks;

        }else
        {
            LOG_KLIENT(pid,"Brak towaru: [%s]\n", sklep->produkty[indeks].nazwa);
            koszyk_id[i] = -1;
            paragon[i] = NULL;
        }

        signalSemafor(id_semafora, SEM_KASY);
        usleep(300000);
    }

    int tryb = (rand() % 100 < SZANSA_SAMOOBSLUGA) ? 1 : 2;
    int obsluzony = 0;
    int nr_kasy = -1;

    //samoobsluga
    if (tryb == 1)
    {
        waitSemafor(id_semafora, SEM_KOLEJKI,0);
        //<- wspolna kolejka do kas samoobslugowych (jesli klient wybierze kase samoobslugowa to zostaje dodany na koniec koljki fifo
        dodajDoKolejkiFIFO(&sklep->kolejka_samoobsluga,  pid);
        signalSemafor(id_semafora, SEM_KOLEJKI);

        time_t start_wait = time(NULL);
        while (1)
        {
            waitSemafor(id_semafora, SEM_KOLEJKI,0);
            pid_t pierwwszy = podejrzyjPierwszegoFIFO(&sklep->kolejka_samoobsluga); //<-wspolna kolejka do kas samoobslugowych (sprawdzenie czy jest 1)
            signalSemafor(id_semafora, SEM_KOLEJKI);

            if (pierwwszy == pid)
            {
                waitSemafor(id_semafora,SEM_KASY,0); //<- wybor kasy (szukanie PIERWSZEJ wolnej)
                for (int i=0; i<KASY_SAMOOBSLUGOWE; i++)
                {
                    if (sklep->kasy_samo[i].otwarta == 1 && sklep->kasy_samo[i].zajeta == 0)
                    {
                        sklep->kasy_samo[i].zajeta = 1;
                        nr_kasy = i;
                        break;
                    }
                }
                signalSemafor(id_semafora, SEM_KASY);

                if (nr_kasy != -1)
                {
                    waitSemafor(id_semafora, SEM_KOLEJKI,0);
                    zdejmijZKolejkiFIFO(&sklep->kolejka_samoobsluga); //<- jesli znalazł to wychodzi z kolejki
                    signalSemafor(id_semafora, SEM_KOLEJKI);
                    break;
                }
            }

            if (difftime(time(NULL), start_wait)>MAX_CZAS_OCZEKIWANIA)
            {
                int stacjo_otwarta = 0;
                waitSemafor(id_semafora, SEM_KASY,0);
                if (sklep->kasa_stato[0].otwarta || sklep->kasa_stato[1].otwarta) stacjo_otwarta = 1;
                signalSemafor(id_semafora,SEM_KASY);

                if (stacjo_otwarta)
                {
                    LOG_KLIENT(pid, "czekam za dlugo (>%ds)! Ide do stacjonarnej.\n",MAX_CZAS_OCZEKIWANIA);
                    waitSemafor(id_semafora, SEM_KOLEJKI, 0);
                    // + tryb = 2 <- jesli czas oczekuwania jest dluzszy niz T to klient idzie do kasy stacjonarnej
                    usunZSrodkaKolejkiFIFO(&sklep->kolejka_samoobsluga, pid);
                    signalSemafor(id_semafora, SEM_KOLEJKI);
                    tryb = 2;
                    break;
                }
            }
            usleep(200000);
        }
    }

    //platnosc samoobsluga
    if (tryb == 1 && nr_kasy != -1) {
        int awaria = rand()%100;
        LOG_KLIENT(pid, "Przy kasie samo nr %d.\n", nr_kasy);
        if ((awaria > 90) || alkohol){
            waitSemafor(id_semafora, SEM_KASY,0);
            //<- weryfikacja wieku klienta jesli w jego zakupach znajduje sie alkohol
            if (alkohol == 1)
            {

                sklep->kasy_samo[nr_kasy].zablokowana = 1;
                sklep->kasy_samo[nr_kasy].alkohol = 1;
                sklep->kasy_samo[nr_kasy].wiek_klienta = wiek;
                LOG_KLIENT(pid, "Weryfikacja wieku na kasie %d (Alkohol w koszyku)\n",nr_kasy);

            }
            if (awaria > 90) //blokada kasy samoobslugowej
            {
                sklep->kasy_samo[nr_kasy].zablokowana = 1;
                LOG_KLIENT(pid, "Kasa %d zablokowana, czekaj na obsluge\n",nr_kasy);
            }
            signalSemafor(id_semafora, SEM_KASY);

            //czekanie na interwencje pracownik.c zeby zmienił flage z .zablokowana = 1 na 0
            while(1) {
                sleep(1);
                waitSemafor(id_semafora, SEM_KASY, 0);
                int blok = sklep->kasy_samo[nr_kasy].zablokowana;
                signalSemafor(id_semafora, SEM_KASY);
                if(!blok) break;
            }
        }

        //<-sprawdzenie czy klient jest zmuszony oddac alkohol
        waitSemafor(id_semafora, SEM_KASY, 0);
        int status_alko = sklep->kasy_samo[nr_kasy].alkohol;

        if (alkohol == 1 && status_alko == -1) //<- (-1) oznacza odrzucenie przez pracownika
        {
            LOG_KLIENT(pid, "Odmowa! Oddaję alkohol, kupuję resztę.\n");

            for (int k =0; k < ile_prod; k++)
            {
                int id_prod = koszyk_id[k];
                if (id_prod != -1 && exists(alkohol_lista,4,id_prod))
                {
                    sklep->produkty[id_prod].sztuk +=1;
                    moj_rachunek -=sklep->produkty[id_prod].cena;
                    koszyk_id[k] = -1;
                }
            }
            if (moj_rachunek < 0) moj_rachunek = 0;
            alkohol = 0;
        }
        sklep->kasy_samo[nr_kasy].alkohol = 0;
        signalSemafor(id_semafora, SEM_KASY);
        LOG_KLIENT(pid,"Kasa odblokowana, kontynuuję.\n");

        LOG_KLIENT(pid, "Przekazuję kwotę %.2f do terminala nr %d...\n", moj_rachunek, nr_kasy);

        waitSemafor(id_semafora, SEM_KASY, 0);
        sklep->kasy_samo[nr_kasy].obslugiwany_klient = pid; // Podpisz się
        sklep->kasy_samo[nr_kasy].aktualna_kwota = moj_rachunek; // Wprowadź kwotę
        signalSemafor(id_semafora, SEM_KASY);

        while (1) {
            waitSemafor(id_semafora, SEM_KASY, 0);
            float czy_juz = sklep->kasy_samo[nr_kasy].aktualna_kwota;
            signalSemafor(id_semafora, SEM_KASY);

            if (czy_juz == 0.0) {
                LOG_KLIENT(pid,"Terminal potwierdził płatność. Zabieram paragon.\n");
                break;
            }
            if (sklep->statystyki.ewakuacja) break;

            usleep(100000);
        }

        //juz po skonczonej platnosci drukowanie "paragonu
        waitSemafor(id_semafora, SEM_KASY, 0);
        LOG_KLIENT(pid,"[");
        int pierwszy = 1;
        for (int l = 0; l < ile_prod; l++)
        {
            if (koszyk_id[l] != -1 && paragon[l] != NULL) {
                if (!pierwszy)
                {
                    printf(", ");
                }
                printf("%s", paragon[l]);
                pierwszy = 0;
            }
        }
        printf("] - Kwota zakupow to: %.2f\n", moj_rachunek);
        signalSemafor(id_semafora, SEM_KASY);

        // Zwolnij kase
        waitSemafor(id_semafora, SEM_KASY, 0);
        sklep->kasy_samo[nr_kasy].zajeta = 0;
        signalSemafor(id_semafora, SEM_KASY);
        obsluzony = 1;
    }

    //platnosc stacjonarna
    if (tryb == 2 && !obsluzony)
    {
        // Wybierz kolejke (tam gdzie krocej) lub domyslnie K1, jesli K2 otwarta to mozna przejsc
        int wybrana_kasa = 0;


        waitSemafor(id_semafora, SEM_KOLEJKI, 0);
        if (sklep->kasa_stato[1].otwarta) {
            //zmiana decyzji o wyborze kasy ze wzgledu na dlugosc kolejki (domyslnie kasa 1)
            if (sklep->kolejka_stato[1].rozmiar < sklep->kolejka_stato[0].rozmiar) wybrana_kasa = 1;
        }
        dodajDoKolejkiFIFO(&sklep->kolejka_stato[wybrana_kasa], pid);
        signalSemafor(id_semafora, SEM_KOLEJKI);
        LOG_KLIENT(pid,"W kolejce do Stacjonarnej %d\n", wybrana_kasa + 1);
        struct messg_buffer msg;
        while (1)
        {
            int wynik = msgrcv(id_kolejki, &msg, sizeof(msg)-sizeof(long), (long)pid, IPC_NOWAIT);

            if (wynik != -1) {
                break;
            }

            waitSemafor(id_semafora, SEM_KOLEJKI, 0);

            int sasiad = (wybrana_kasa == 0) ? 1 : 0;

            int rozmiar_mojej = sklep->kolejka_stato[wybrana_kasa].rozmiar;
            int rozmiar_sasiada = sklep->kolejka_stato[sasiad].rozmiar;

            int czy_otwarta_sasiad = (sasiad == 0) ? sklep->kasa_stato[0].otwarta : sklep->kasa_stato[1].otwarta;
            int czy_moja_otwarta = sklep->kasa_stato[wybrana_kasa].otwarta;

            if (czy_otwarta_sasiad && rozmiar_sasiada == 0)
            {
                int uciekam = 0;

                if (czy_moja_otwarta && rozmiar_mojej > 1) uciekam = 1;
                if (!czy_moja_otwarta && rozmiar_mojej > 0) uciekam = 1;

                if (uciekam)
                {
                    if (usunZSrodkaKolejkiFIFO(&sklep->kolejka_stato[wybrana_kasa], pid) == 1)
                    {
                        wybrana_kasa = sasiad;
                        dodajDoKolejkiFIFO(&sklep->kolejka_stato[wybrana_kasa], pid);

                        char* powod = czy_moja_otwarta ? "tłok" : "zamknięta";
                        LOG_KLIENT(pid, "Zmieniam kolejkę! (Powód: %s) Ide do kasy %d\n", powod, wybrana_kasa + 1);
                    }
                }
            }
            signalSemafor(id_semafora, SEM_KOLEJKI);

            waitSemafor(id_semafora, SEM_KASY, 0);
            if(sklep->statystyki.ewakuacja) {
                signalSemafor(id_semafora, SEM_KASY);
                exit(0);
            }
            signalSemafor(id_semafora, SEM_KASY);

            usleep(200000);
        }

        // Odbieram wiadomosc skierowana do pid klienta
        if (msg.kwota == 0) { // Zaproszenie
            int id_kasjera = msg.ID_klienta;
            // Wysylam paragon
            msg.mesg_type = id_kasjera + KANAL_KASJERA_OFFSET;
            msg.ID_klienta = pid;
            msg.kwota = moj_rachunek;
            msg.wiek = wiek;
            msg.ma_alkohol = alkohol;
            WyslijDoKolejki(id_kolejki, &msg);

            if (msg.kwota == -2.0) {
                LOG_KLIENT(pid, "Kasjer odmówił sprzedaży alkoholu! Oddaję na półkę.\n");

                waitSemafor(id_semafora, SEM_KASY, 0);
                for (int k=0; k < ile_prod; k++) {
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
                WyslijDoKolejki(id_kolejki, &msg);

                OdbierzZKolejki(id_kolejki, &msg, (long)pid);
            }

            waitSemafor(id_semafora, SEM_KASY, 0);
            LOG_KLIENT(pid,"[");
            for (int l = 0; l < ile_prod; l++)
            {
                if (koszyk_id[l] != -1 && paragon[l] != NULL) {
                    printf("%s, ", paragon[l]);
                }
                printf("] - Kwota zakupow to: %.2f\n", moj_rachunek);
            }
            signalSemafor(id_semafora, SEM_KASY);
        }
    }
    free(paragon);
    free(koszyk_id);
    waitSemafor(id_semafora, SEM_KASY, 0);
    sklep->statystyki.liczba_klientow_w_sklepie--;
    signalSemafor(id_semafora, SEM_KASY);

    odlacz_pamiec_dzielona(sklep);
    return 0;
}
