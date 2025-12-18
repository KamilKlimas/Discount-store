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
int czyDziala = 1;
int moje_id;

int id_pamieci;
PamiecDzielona *sklep;

int id_semafora;

void cleanUpKasy()
{
    sklep ->kasy_samo[moje_id].otwarta = 0;
    if (sklep != NULL)
    {
        odlacz_pamiec_dzielona(sklep);
        printf("\nKasjer nr [%d] konczy zmiane, do zobaczenia w niedziele handlowa:\n)",moje_id);
    }

}

void ObslugaSygnalu(int signal){
    printf("\nOtrzymano sygnal zakonczenia pracy\n");
    czyDziala = 0;
}

int main (int argc, char *argv[])
{
    setbuf(stdout, NULL); // na potrzebe Cliona

    if (argc < 2) //argc bedzie zawsze minimum 1 wiec sprawdzamy czy dostalismy tylko nazwe programu czy jeszcze cosik wiecej
    {
        printf("\nbrak argumentu od kierownika\n");
        exit(1);
    }
    moje_id = atoi(argv[1]);

    moje_id = atoi(argv[1]); //atoi zmieni stringa na liczbe

    key_t klucz = ftok("/tmp/dyskont_projekt", 'S');

    if (klucz == -1)
    {
        perror("\nblad ftok\n");
        exit(1);
    }


    id_pamieci = podlacz_pamiec_dzielona();  //shmget
    if (id_pamieci == -1)
    {
        perror("\nblad podlacz_pamiec_dzielona\n");
        exit(1);
    }
    sklep = mapuj_pamiec_dzielona(id_pamieci); //shmat, trzeba było stworzyc wskaznik na sklep!!!(PamiecDzielona* sklep)

    id_semafora = alokujSemafor(klucz, 2, 0);

    signal(SIGINT, ObslugaSygnalu);

    printf("\nkasjer [%d] zaczyna prace\n", moje_id);
    waitSemafor(id_semafora, SEM_KASY, 0);
    sklep ->kasy_samo[moje_id].otwarta = 1;
    sklep ->kasy_samo[moje_id].zajeta = 0;
    signalSemafor(id_semafora, SEM_KASY);

    srand(time(NULL)+moje_id);

    while (czyDziala == 1)
    {
        waitSemafor(id_semafora, SEM_KASY, 0);
        int czy_jest_klient = sklep->kasy_samo[moje_id].zajeta;
        signalSemafor(id_semafora, SEM_KASY);

        if (czy_jest_klient == 1)
        {
            printf("\nObsluguje klienta\n");
            //
            sleep(2);
            float r = rand()%100;
            waitSemafor(id_semafora, SEM_UTARG, 0);
            printf("\nDokladam [%.2f] zl do utargu\n", r);
            sklep ->statystyki.utarg += r;
            signalSemafor(id_semafora, SEM_UTARG);
            while (1)
            {
                sleep(1);
                waitSemafor(id_semafora, SEM_KASY,0);
                int nadal_zajeta = sklep->kasy_samo[moje_id].zajeta;
                signalSemafor(id_semafora, SEM_KASY);

                if (nadal_zajeta == 0)
                {
                    printf("\nKlient poszedl. Kasa wolna\n");
                    break;
                }
            }
        }else
        {
            printf("\nnikogo nie ma\n");
            waitSemafor(id_semafora, SEM_UTARG, 0);
            float aktualny_utarg = sklep->statystyki.utarg;
            signalSemafor(id_semafora, SEM_UTARG);

            printf("\nutarg to: [%f]\n", sklep->statystyki.utarg);
            sleep(2);
        }





        sleep(1);
    }
    cleanUpKasy();


    return 0;
}
