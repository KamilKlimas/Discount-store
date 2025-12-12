//
// Created by kamil-klimas on 10.12.2025.
//
#include "ipc.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>

// proces glowny - do uzupelniania:
/*
    - obsluga sygnalow
    - czyszczenie IPC
*/

int shmid;
PamiecDzielona* shm;
int dziala =1; //do petli glownej

//cntrl C
void obsluga_sygnalu()
{

    printf("zamykanie sklepu")
    czy_dziala =0;
}

void cleanup()
{
    if (shm) odlacz_pamiec_dzielona(shm);
    if (shmid != -1) usun_pamiec_dzielona(shmid);
}

int main()
{
    sprintf("KIEROWNIK: Otwieramy sklep \n");


    // tworzenie IPC
    shmid = utworz_pamiec_dzielona(sizeof(PamiecDzielona));
    shm = mapuj_pamiec_dzielona(shmid);

    for (int i=0; i <= KASY_SAMOOBSLUGOWE;  i++)
    {
        shm->kasy_samo[i].czynna = 1;
        shm->kasy_samo[i].zajeta = 0;
        shm->kasy_samo[i].liczba_obsluzonych = 0;
    }

    for (int i=0; i <= KASY_STACJONARNE;  i++)
    {
        shm->kasy_stato[i].otwarta = 0;
        shm->kasy_stato[i].zajeta = 0;
    }

    shm->kierownik_pid = getpid();
    signal(SIGINT, obsluga_sygnalu);
    printf("Kierownik: gotowosc doo. pid=%d\n",getpid());

    while (dziala)
    {

    }

    cleanup();
    printf("Kierownik: koniec\n");
    return 0;

}
