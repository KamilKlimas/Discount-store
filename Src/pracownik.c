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
    printf("\nOtrzymano sygnal zakonczenia pracy\n");
    dzialaj = 0;
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

    id_semafora = alokujSemafor(klucz, 2, 0);
    signal(SIGINT, ObslugaSygnalu);

    while (dzialaj ==1)
    {
        for (int i=0; i < sklep->liczba_produktow; i++)
        {
            if (sklep->produkty[i].sztuk < 10)
            {
                waitSemafor(id_semafora, SEM_KASY, 0);
                sklep->produkty[i].sztuk = 50;
                signalSemafor(id_semafora, SEM_KASY);
                printf("\nUzupelnilem %s do 50 sztuk\n", sklep->produkty[i].nazwa);
            }
        }
        printf("Ide na kawe");
        sleep(5);
    }
    odlacz_pamiec_dzielona();
    return 0;
}//