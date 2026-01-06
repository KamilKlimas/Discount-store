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
#include <signal.h>


volatile sig_atomic_t running = 1;

int wejscie_do_sklepu;
int pid;

int id_pamieci;
PamiecDzielona *sklep;
void obslugaSIGINT(int sig)
{
    if (sig == SIGINT)
    {
        running = 0;
    }
}
int main()
{
    signal(SIGINT, obslugaSIGINT);

    setbuf(stdout, NULL);
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

    LOG_GENERATOR("ROZPOCZYNAM SYMULACJE DNIA\n");

    for (int i= 0; i < KLIENCI; i++)
    {

        if (running == 0) {
            LOG_GENERATOR("Otrzymano Ctrl+C. Zamykam sklep dla nowych klientów.");
            if(sklep != NULL) sklep->czy_otwarte = 0;
            break;
        }

        if (sklep->czy_otwarte == 0 || sklep->statystyki.ewakuacja == 1)
        {
            LOG_GENERATOR("Sklep zamkniety. Koncze wpuszczanie klientow.\n");
            running = 0;
            break;
        }

        // Faza 1: (Pierwszych 10 klientów) -> WOLNO
        if (i < 10) {
            if (i == 0) LOG_SYSTEM("FAZA 1");
            //1.5s - 2.5s
            wejscie_do_sklepu = rand() % 1000000 + 1500000;
        }
        // Faza 2: (Kolejnych 50 klientów) -> BARDZO SZYBKO
        else if (i < 60) {
            if (i == 10) LOG_SYSTEM("FAZA 2: GODZINY SZCZYTU");
            //0.1s - 0.3s
            wejscie_do_sklepu = rand() % 200000 + 100000;
        }
        // Faza 3: (Reszta) -> ŚREDNIO
        else {
            if (i == 60) LOG_SYSTEM("FAZA 3");
            //0.8s - 1.5s
            wejscie_do_sklepu = rand() % 700000 + 800000;
        }
        int kawalki = wejscie_do_sklepu / 100000;
        if (kawalki == 0) kawalki = 1;

        int przerwanie = 0;
        for(int k=0; k<kawalki; k++) {
            usleep(100000);
            if (sklep->statystyki.ewakuacja == 1) {
                przerwanie = 1;
                if(running == 0 || sklep->czy_otwarte == 0) break;
                break;
            }
        }

        if (przerwanie) {
            //printf("\nWykryto EWAKUACJE w trakcie czekania! Uciekam.\n");
            LOG_GENERATOR("Wykryto EWAKUACJE w trakcie czekania! Uciekam.\n");
            break;
        }

        if(running == 0) break;


        pid = fork();
        if (pid == 0)
        {
            //printf("\nStworzono proces potomny\n");
            LOG_GENERATOR("Stworzono proces potomny\n");
            execlp("./klient","klient", NULL);

            if (1) //jesli tutaj dotartł to klient nie istnieje -> moze byc bez ifa ale niech se ten if bedzie
            {
                perror("\nNie ma pliku klienta\nnn");
                exit(1);
            }

        }else if (pid > 0){
            if (i < 10 || i >= 60 || i % 5 == 0) {
                LOG_GENERATOR("Wchodzi klient [%d] (PID: %d)", i, pid);
            }
        }
        if (pid < 0)
        {
            perror("\nsystem nie pozwolil stworzyc procesu\n");
        }
    }

    //printf("\nWszyscy klienci sobie zyja, czekam az skoncza zakupy\n");
    LOG_GENERATOR("Wszyscy klienci sobie zyja, czekam az skoncza zakupy\n");
    for (int j=0; j <KLIENCI; j++)
    {
        if (sklep->czy_otwarte == 0)
        {
            //printf("Sklep zamknięty, nie wpuszczam więcej osób.\n");
            LOG_GENERATOR("Sklep zamknięty, nie wpuszczam więcej osób.\n");
            break;
        }
        while(wait(NULL) > 0);
    }
    //printf("\nSklep pusty, zamykam generator\n");
    LOG_GENERATOR("Sklep pusty, zamykam generator\n");
    return 0;



}
///