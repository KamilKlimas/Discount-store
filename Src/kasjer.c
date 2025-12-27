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
#include <sys/wait.h>
int czyDziala = 1;
int moje_id;
volatile int status_pracy = 1; //volatile tells the compiler not to optimize anything that has to do with the volatile variable.
/*There are at least three common reasons to use it, all involving situations where the value of the variable can change without action from the visible code:
- When you interface with hardware that changes the value itself
- when there's another thread running that also uses the variable
- when there's a signal handler that might change the value of the variable.
 */
int id_pamieci;
PamiecDzielona *sklep;

int id_semafora;
int id_kolejki;

void cleanUpKasy()
{

    if (sklep != NULL)
    {
        sklep ->kasa_stato[moje_id].otwarta = 0;
        odlacz_pamiec_dzielona(sklep);
        printf("\nKasjer nr [%d] konczy zmiane, do zobaczenia w niedziele handlowa:\n)",moje_id);
    }

}

void ObslugaSygnalu(int sig) {
    if (sig == SIGUSR1) {
        status_pracy = 1;
        // printf("\nKasjer %d Zaczynam prace.\n", moje_id);
    }
    else if (sig == SIGUSR2) {
        status_pracy = 0;
        // printf("\nKasjer %d Koncze prace.\n", moje_id);
    }
    else if (sig == SIGQUIT) { exit(0); }
}

int main (int argc, char *argv[])
{
    setbuf(stdout, NULL);

    if (argc < 2)
    {
        printf("\nbrak argumentu od kierownika\n");
        exit(1);
    }

    moje_id = atoi(argv[1]);

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

    id_semafora = alokujSemafor(klucz, 3, 0);
    id_kolejki = stworzKolejke();

    signal(SIGUSR1, ObslugaSygnalu);
    signal(SIGUSR2, ObslugaSygnalu);
    signal(SIGQUIT, ObslugaSygnalu);

    struct messg_buffer msg;
    long moj_typ_nasluchu = moje_id +KANAL_KASJERA_OFFSET;

    printf("\nkasjer [%d] zaczyna prace (typ nasluchu: %ld)\n",moje_id, moj_typ_nasluchu);
    /*
    waitSemafor(id_semafora, SEM_KASY, 0);
    sklep ->kasa_stato[moje_id].otwarta = status_pracy;
    sklep ->kasa_stato[moje_id].zajeta = 0;
    signalSemafor(id_semafora, SEM_KASY);
    */
    //srand(time(NULL)+moje_id);

    while (1)
    {
        if (status_pracy == 0)
        {
           pause();
            continue;
        }

        pid_t klient_pid = 0;
        waitSemafor(id_semafora, SEM_KOLEJKI, 0);
        if (sklep->kolejka_stato[moje_id].rozmiar > 0)
        {
            klient_pid = zdejmijZKolejkiFIFO(&sklep->kolejka_stato[moje_id]);
        }
        signalSemafor(id_semafora, SEM_KOLEJKI);

        if (klient_pid >0)
        {
            printf("Kasjer %d Wolam klienta %d do kasy.\n", moje_id, klient_pid);

            waitSemafor(id_semafora, SEM_KASY, 0);
            sklep->kasa_stato[moje_id].zajeta = 1;
            sklep->kasa_stato[moje_id].obslugiwany_klientl = klient_pid;
            signalSemafor(id_semafora, SEM_KASY);

            //wezwanie klienta
            msg.mesg_type = (long)klient_pid;
            msg.ID_klienta = moje_id;
            msg.kwota = 0;
            WyslijDoKolejki(id_kolejki, &msg);

            //oczekiwanie na paragon
            OdbierzZKolejki(id_kolejki, &msg, moj_typ_nasluchu);
            printf("\nKasjer %d Zakupy od Klienta %d na: %.2f zl\n", moje_id, msg.ID_klienta, msg.kwota);
            sleep(2);//symalacja kasowania

            waitSemafor(id_semafora, SEM_UTARG, 0);
            sklep->statystyki.utarg += msg.kwota;
            sklep->statystyki.liczba_obsluzonych_klientow +=1;
            signalSemafor(id_semafora, SEM_UTARG);

            // Potwierdzenie zaplaty
            msg.mesg_type = (long)klient_pid;
            msg.kwota = -1.0;
            WyslijDoKolejki(id_kolejki, &msg);
            printf("\nObsluzylem klienta %d\n", msg.ID_klienta);
            waitSemafor(id_semafora, SEM_KASY, 0);
            sklep->kasa_stato[moje_id].zajeta = 0;
            sklep->kasa_stato[moje_id].czas_ostatniej_obslugi = time(NULL);
            signalSemafor(id_semafora, SEM_KASY);
        }else
        {
            usleep(200000);
        }
    }
    return 0;
}
//