//
// Created by kamil-klimas on 10.12.2025.
//

//funkcje:
/*
    - dokladanie towaru na polki
    - odblokuj kase
    - weryfikacja wieku
*/

#include "ipc.h"
#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

int dzialaj = 1;

int id_pamieci;
PamiecDzielona *sklep;

int id_semafora;

void ObslugaSygnalu(int signal){
    //printf("\nOtrzymano sygnal zakonczenia pracy\n");
    LOG_PRACOWNIK("Otrzymano sygnal zakonczenia pracy\n" );
    dzialaj = 0;
}

void ewakuacja(int signalNum)
{
    if (signalNum == SIGQUIT)
    {
        //printf("\nEWAKUACJA\n");
        LOG_PRACOWNIK("EWAKUACJA\n");
        exit(0);
    }
}


int main()
{
    setbuf(stdout, NULL);

    key_t klucz = ftok("/tmp/dyskont_projekt", 'S');
    if (klucz == -1)
    {
        perror("\nblad ftok\n");
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
    signal(SIGINT, ObslugaSygnalu);
    signal(SIGQUIT, ewakuacja);


    while (dzialaj ==1)
    {
        for (int i=0; i < sklep->liczba_produktow; i++)
        {
            waitSemafor(id_semafora, SEM_KASY, 0);
            if (sklep->produkty[i].sztuk < 35)
            {
                sklep->produkty[i].sztuk = 50;
                //printf("\nPracownik %d: Uzupelnilem %s do 50 sztuk\n", getpid(), sklep->produkty[i].nazwa);
                LOG_PRACOWNIK("%d: Uzupelnilem %s do 50 sztuk\n", getpid(), sklep->produkty[i].nazwa);
            }
            signalSemafor(id_semafora, SEM_KASY);
        }
        for (int j=0; j < KASY_SAMOOBSLUGOWE; j++)
        {
            waitSemafor(id_semafora, SEM_KASY, 0);
            int czy_zablokowana= sklep->kasy_samo[j].zablokowana;
            signalSemafor(id_semafora, SEM_KASY);

            if (czy_zablokowana == 1)
            {
                sleep(2);
                //printf("\nPracownik %d: Dotarlem na miejsce i odblokowywuje kase %d\n",getpid(),j);
                LOG_PRACOWNIK("Dotarlem na miejsce i odblokowywuje kase %d\n",j);
                waitSemafor(id_semafora, SEM_KASY, 0);
                sklep->kasy_samo[j].zablokowana = 0;
                signalSemafor(id_semafora, SEM_KASY);
            }

        }
        usleep(200000);
    }
    odlacz_pamiec_dzielona(sklep);
    return 0;
}////