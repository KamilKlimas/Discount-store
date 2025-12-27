//
// Created by kamil-klimas on 10.12.2025.
//

//funkcje:
/*
    - wejscie/wyjscie ze sklepu
    - robienie zakupow
    - czekanie na wolna kase
    -
*/

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

void uciekaj(int sig) {
    if(sig == SIGQUIT)
    {
        printf("\nKlient %d ALARM! UCIEKAM!\n", getpid()); exit(0);
    }
}

int main()
{
    setbuf(stdout, NULL);
    pid = getpid();
    srand(pid^czas);

    double moj_rachunek = 0.0;

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

    signal(SIGQUIT, uciekaj);

    waitSemafor(id_semafora, SEM_KASY, 0);
    sklep ->statystyki.liczba_klientow_w_sklepie += 1;
    signalSemafor(id_semafora, SEM_KASY);

    int ile_prod = (rand() % 8) + 1;
    char **paragon = malloc(ile_prod * sizeof(char));
    sleep(1);


    for (int i =0; i <= ile_prod;i++){ //ile produktow chce wziasc klient
        indeks = rand()%LICZBA_PRODUKTOW; // z kazda iteracja losowy produkt
        //wejscie w strefe krytyczna -> nie moze dwoch na raz brac to samo mleko bo sie inwentura nie bedzie zgadzac
        waitSemafor(id_semafora, SEM_KASY, 0); //SEM_KASY - to glowny semafor, nie ma potrzeby dodawac 3 bo ten blokuje na chwileczke wszystkie dzialania w sklepie
        if (sklep->produkty[indeks].sztuk >0){
            sklep->produkty[indeks].sztuk -=1;
            if (exists(alkohol_lista, 4, indeks))
            {
                alkohol = 1;
            }

            moj_rachunek+= sklep->produkty[indeks].cena;
            //printf("\nZostalo [%d] sztuk [%s]\n", sklep->produkty[indeks].sztuk, sklep->produkty[indeks].nazwa);
            paragon[i] = sklep->produkty[indeks].nazwa;
        }else
        {
            printf("\nBrak towaru: [%s]\n", sklep->produkty[indeks].nazwa);
        }

        signalSemafor(id_semafora, SEM_KASY);
    }

    int tryb = (rand() % 100 < SZANSA_SAMOOBSLUGA) ? 1 : 2;
    int obsluzony = 0;
    int nr_kasy = -1;

    //samoobsluga
    if (tryb == 1)
    {
        waitSemafor(id_semafora, SEM_KOLEJKI,0);
        dodajDoKolejkiFIFO(&sklep->kolejka_samoobsluga,  pid);
        signalSemafor(id_semafora, SEM_KOLEJKI);

        time_t start_wait = time(NULL);
        while (1)
        {
            waitSemafor(id_semafora, SEM_KOLEJKI,0);
            pid_t pierwwszy = podejrzyjPierwszegoFIFO(&sklep->kolejka_samoobsluga);
            signalSemafor(id_semafora, SEM_KOLEJKI);

            if (pierwwszy == pid)
            {
                waitSemafor(id_semafora,SEM_KASY,0);
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
                    zdejmijZKolejkiFIFO(&sklep->kolejka_samoobsluga);
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
                    printf("Klient %d, czekam za dlugo (>%ds)! Ide do stacjonarnej.\n", pid, MAX_CZAS_OCZEKIWANIA);
                    waitSemafor(id_semafora, SEM_KOLEJKI, 0);
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
        printf("Klient %d Przy kasie samo nr %d.\n", pid, nr_kasy);

        // Symulacja kasowania + awarie
        if ((rand()%100 > 80) || alkohol) {
            waitSemafor(id_semafora, SEM_KASY, 0);
            sklep->kasy_samo[nr_kasy].zablokowana = 1;
            sklep->kasy_samo[nr_kasy].alkohol = alkohol;
            printf("Klient %d BLOKADA KASY SAMO %d (Alkohol/Blad)!\n", pid, nr_kasy);
            signalSemafor(id_semafora, SEM_KASY);

            // Czekaj na odblokowanie
            while(1) {
                sleep(1);
                waitSemafor(id_semafora, SEM_KASY, 0);
                int blok = sklep->kasy_samo[nr_kasy].zablokowana;
                signalSemafor(id_semafora, SEM_KASY);
                if(!blok) break;
            }
            printf("Klient %d Kasa odblokowana.\n", pid);
        }

        sleep(1); // Platnosc
        printf("\n[");
        for (int l = 0; l < ile_prod; l++)
        {
            printf("%s ", paragon[l]);
            if (l < ile_prod-1) printf(", ");
        }
        printf("] - Kwota zakupow to: %.2f\n", moj_rachunek);

        waitSemafor(id_semafora, SEM_UTARG, 0);
        sklep->statystyki.utarg += moj_rachunek;
        sklep->statystyki.liczba_obsluzonych_klientow++;
        signalSemafor(id_semafora, SEM_UTARG);

        // Zwolnij kase
        waitSemafor(id_semafora, SEM_KASY, 0);
        sklep->kasy_samo[nr_kasy].zajeta = 0;
        signalSemafor(id_semafora, SEM_KASY);
        obsluzony = 1;
    }

    //platnosc stacjonarna
    if (tryb == 2 && !obsluzony) {
        // Wybierz kolejke (tam gdzie krocej) lub domyslnie K1, jesli K2 otwarta to mozna przejsc
        int wybrana_kasa = 0;


        waitSemafor(id_semafora, SEM_KOLEJKI, 0);
        if (sklep->kasa_stato[1].otwarta) {
            if (sklep->kolejka_stato[1].rozmiar < sklep->kolejka_stato[0].rozmiar) wybrana_kasa = 1;
        }
        dodajDoKolejkiFIFO(&sklep->kolejka_stato[wybrana_kasa], pid);
        signalSemafor(id_semafora, SEM_KOLEJKI);

        printf("Klient %d W kolejce do Stacjonarnej %d\n", pid, wybrana_kasa);


        struct messg_buffer msg;
        // Odbieram wiadomosc skierowana do pid klienta
        if (OdbierzZKolejki(id_kolejki, &msg, (long)pid) != -1) {
            if (msg.kwota == 0) { // Zaproszenie
                int id_kasjera = msg.ID_klienta;
                // Wysylam paragon
                msg.mesg_type = id_kasjera + KANAL_KASJERA_OFFSET;
                msg.ID_klienta = pid;
                msg.kwota = moj_rachunek;
                WyslijDoKolejki(id_kolejki, &msg);

                // Czekam na potwierdzenie
                OdbierzZKolejki(id_kolejki, &msg, (long)pid);
                printf("\n[");
                for (int l = 0; l < ile_prod; l++)
                {
                    printf("%s ", paragon[l]);
                    if (l < ile_prod-1) printf(", ");
                }
                printf("] - Kwota zakupow to: %.2f\n", moj_rachunek);
            }
        }
    }
    free(paragon);
    waitSemafor(id_semafora, SEM_KASY, 0);
    sklep->statystyki.liczba_klientow_w_sklepie--;
    signalSemafor(id_semafora, SEM_KASY);

    odlacz_pamiec_dzielona(sklep);
    return 0;
}
//


