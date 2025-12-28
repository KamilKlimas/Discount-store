//
// Created by kamil-klimas on 19.12.2025.
//
#include "ipc.h"
#include <sys/wait.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <stdio.h>


int wejscie_do_sklepu;
int pid;

int id_pamieci;
PamiecDzielona *sklep;

int main()
{
    srand(time(NULL));

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

    for (int i= 0; i < KLIENCI; i++)
    {
        if (sklep->czy_otwarte == 0 || sklep->statystyki.ewakuacja == 1)
        {
            printf("\nSklep zamkniety. Koncze wpuszczanie klientow.\n");
            break;
        }

        wejscie_do_sklepu = rand()%3000000 + 500000; //usleep dziala a mikrosekundach :)
        usleep(wejscie_do_sklepu);

        pid = fork();
        if (pid == 0)
        {
            printf("\nStworzono proces potomny\n");
            execlp("./klient","klient", NULL);

            if (1) //jesli tutaj dotartł to klient nie istnieje -> moze byc bez ifa ale niech se ten if bedzie
            {
                perror("\nNie ma pliku klienta\nnn");
                exit(1);
            }

            }
        if (pid > 0){
            printf("\nWpuscilem klienta [%d] (PID: [%d])\n", i, pid);
        }
        if (pid < 0)
        {
            perror("\nsystem nie pozwolil stworzyc procesu\n");
        }
    }

    printf("\nWszyscy klienci sobie zyja, czekam az skoncza zakupy\n");
    for (int j=0; j <KLIENCI; j++)
    {
        if (sklep->czy_otwarte == 0)
        {
            printf("Sklep zamknięty, nie wpuszczam więcej osób.\n");
            break;
        }
        wait(NULL);
    }
    printf("\nSklep pusty, zamykam generator\n");
    return 0;



}
///