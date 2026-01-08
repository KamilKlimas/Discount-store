//
// Created by kamil-klimas on 10.12.2025.
//

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
    (void)signal;
    LOG_PRACOWNIK("Otrzymano sygnal zakonczenia pracy\n" );
    dzialaj = 0;
}

void ewakuacja(int signalNum)
{
    if (signalNum == SIGQUIT)
    {
        LOG_PRACOWNIK("KONIEC PRACY\n");
        exit(0);
    }
}


int main()
{
    setbuf(stdout, NULL);
    signal(SIGINT, SIG_IGN);
    signal(SIGQUIT, ewakuacja);

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



    while (dzialaj ==1)
    {
        for (int j=0; j < KASY_SAMOOBSLUGOWE; j++)
        {
            waitSemafor(id_semafora, SEM_KASY, 0);
            int czy_zablokowana= sklep->kasy_samo[j].zablokowana;
            int status_alko = sklep->kasy_samo[j].alkohol;
            int wiek_klienta = sklep->kasy_samo[j].wiek_klienta;
            signalSemafor(id_semafora, SEM_KASY);

            if (czy_zablokowana == 1)
            {
                sleep(1);
                if (status_alko == 1)
                {
                    LOG_PRACOWNIK("Weryfikacja wieku przy kasie %d. Klient ma %d lat.\n", j, wiek_klienta);

                    waitSemafor(id_semafora, SEM_KASY, 0);
                    if (wiek_klienta < 18) {
                        LOG_PRACOWNIK("NIELETNI! Zabieram alkohol z kasy %d i odkładam na półkę.\n", j);
                        sklep->kasy_samo[j].alkohol = -1;
                    } else {
                        LOG_PRACOWNIK("Pełnoletni (OK). Zatwierdzam alkohol na kasie %d.\n", j);
                        sklep->kasy_samo[j].alkohol = 2;
                    }
                    sklep->kasy_samo[j].zablokowana = 0;
                    signalSemafor(id_semafora, SEM_KASY);
                }
                else
                {
                    LOG_PRACOWNIK("Usuwam awarię techniczną przy kasie %d\n", j);
                    waitSemafor(id_semafora, SEM_KASY, 0);
                    sklep->kasy_samo[j].zablokowana = 0;
                    signalSemafor(id_semafora, SEM_KASY);
                }
            }

        }

        if (sklep->czy_otwarte == 1)
        {
            for (int i=0; i < sklep->liczba_produktow; i++)
            {
                waitSemafor(id_semafora, SEM_KASY, 0);
                if (sklep->produkty[i].sztuk < 35)
                {
                    sklep->produkty[i].sztuk = 50;
                    LOG_PRACOWNIK("%d: Uzupelnilem %s do 50 sztuk\n", getpid(), sklep->produkty[i].nazwa);
                }
                signalSemafor(id_semafora, SEM_KASY);

            }
            usleep(200000);
        }else
        {
            usleep(100000);
        }
    }
    odlacz_pamiec_dzielona(sklep);
    return 0;
}