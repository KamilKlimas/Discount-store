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

int zakupy;
int indeks;

int main()
{
    setbuf(stdout, NULL);
    pid = getpid();
    czas = time(NULL);
    srand(pid^czas);
    zakupy = (rand()%10) + 1;

    float moj_rachunek = 0.0;

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
    id_semafora = alokujSemafor(klucz, 2, 0);

    id_kolejki = stworzKolejke();

    //wejscie kleinta do sklepu
    waitSemafor(id_semafora, SEM_KASY, 0);
    sklep ->statystyki.liczba_klientow_w_sklepie += 1;
    signalSemafor(id_semafora, SEM_KASY);

    sleep(2);


    for (int i =0; i <= zakupy;i++){ //ile produktow chce wziasc klient
        indeks = rand()%LICZBA_PRODUKTOW; // z kazda iteracja losowy produkt
    //wejscie w strefe krytyczna -> nie moze dwoch na raz brac to samo mleko bo sie inwentura nie bedzie zgadzac
        waitSemafor(id_semafora, SEM_KASY, 0); //SEM_KASY - to glowny semafor, nie ma potrzeby dodawac 3 bo ten blokuje na chwileczke wszystkie dzialania w sklepie
        if (sklep->produkty[indeks].sztuk >0){
            sklep->produkty[indeks].sztuk -=1;
            moj_rachunek+= sklep->produkty[indeks].cena;
            printf("\nZostalo [%d] sztuk [%s]\n", sklep->produkty[indeks].sztuk, sklep->produkty[indeks].nazwa);
        }else //czy const char sie wyprintuje?
        {
            printf("\nBrak towaru: [%s]\n", sklep->produkty[indeks].nazwa);
        }

        //tutaj bedzie trza zrobic ładne "drukowanie paragonu i zapisywanie do pliku"

        printf("\nKwota zakupow to: %.2f\n", moj_rachunek);
    //wyjscie ze strefy krytycznej
    signalSemafor(id_semafora, SEM_KASY);
        sleep(1);
    }
    //osobna losowosc dla petli (dajmy ze maks 4 produkty)
    //osobna losowosc dla produtu %LICZBA_PRODUKTOW

    //SZUKANIE WOLNEJ KASY
    int znaleziono_kase = -1;
    int wybor = 0;// 1 - samo, 2 - stacjo
    while (1)
    {
        waitSemafor(id_semafora, SEM_KASY, 0);

        for (int i =0; i <KASY_SAMOOBSLUGOWE;i++)
        {
            if (sklep->kasy_samo[i].otwarta == 1 && sklep->kasy_samo[i].zajeta == 0)
            {
                sklep->kasy_samo[i].zajeta = 1;
                znaleziono_kase = i;
                wybor = 1;
                printf("\nPodchodze do kasy samoobslugowej[%d]\n", i);
                break;
            }
        }

        if (znaleziono_kase == -1)
        {
            for (int i = 0; i <KASY_STACJONARNE; i++ )
            {
                if (sklep->kasa_stato[i].otwarta == 1 && sklep->kasa_stato[i].zajeta == 0)
                {
                    sklep->kasa_stato[i].zajeta = 1;
                    znaleziono_kase = i;
                    wybor =2;
                    printf("\nPodchodze do kasy stacjonarnej [%d]\n", i);
                    break;
                }
            }
        }

        signalSemafor(id_semafora, SEM_KASY); //kolejka rusza dalej bo kasa tego klienta została znaleziona

        //platnosc
        if (znaleziono_kase != -1)
        {
            printf("\nPodchodze do kasy...\n ");
            break;
        }

        sleep(1);
    }

        waitSemafor(id_semafora, SEM_KASY,0);
        if (wybor == 1) sklep->kasy_samo[znaleziono_kase].platnosc_w_toku =1;
        if (wybor == 2) sklep->kasa_stato[znaleziono_kase].platnosc_w_toku =1;
        signalSemafor(id_semafora, SEM_KASY);

        if (wybor == 1)
        {
            printf("\nKlient %d: Place w kasie samoobslugowej\n",pid);
            sleep(2);

            waitSemafor(id_semafora, SEM_UTARG,0);
            sklep->statystyki.utarg += moj_rachunek;
            sklep->statystyki.liczba_obsluzonych_klientow += 1;
            signalSemafor(id_semafora,SEM_UTARG);

            printf("\nKlient %d: Zaplacilem samodzielnie, wychodze.\n",pid);
        }else if (wybor ==2)
        {
            struct messg_buffer msg;
            long typ_adresata = znaleziono_kase + 1 + KASY_SAMOOBSLUGOWE;
            msg.mesg_type = typ_adresata; // zeby typ byl zawsze >0 (pierwsza kasa to 0)
            msg.kwota = moj_rachunek;
            msg.ID_klienta = getpid();

            WyslijDoKolejki(id_kolejki, &msg);
            printf("\nKlient %d: Czekam na kasjera\n", pid);

            //printf("[Klient %d] Wysłałem paragon %.2f zł do kasy %d przez kolejkę.\n", pid, moj_rachunek, znaleziono_kase);
            OdbierzZKolejki(id_kolejki, &msg, (long)pid);
            printf("\nKlient %d: Kasjer mnie obsluzyl, wychodze \n",pid);
        }


        waitSemafor(id_semafora, SEM_KASY, 0);
        if (wybor == 1)
        {
            sklep->kasy_samo[znaleziono_kase].platnosc_w_toku =0;
            sklep->kasy_samo[znaleziono_kase].zajeta = 0;
        }
        if (wybor ==2)
        {
            sklep->kasa_stato[znaleziono_kase].platnosc_w_toku =0;
            sklep->kasa_stato[znaleziono_kase].zajeta = 0;
        }

        printf("\nKasa wolna\n");
        sklep->statystyki.liczba_klientow_w_sklepie -=1;
        signalSemafor(id_semafora, SEM_KASY);

        odlacz_pamiec_dzielona(sklep);
        return 0;
}
