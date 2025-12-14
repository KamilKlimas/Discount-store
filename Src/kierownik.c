//
// Created by kamil-klimas on 10.12.2025.
//
#include "ipc.h"
#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>


// proces glowny - do uzupelniania:
/*
    - obsluga sygnalow
    - czyszczenie IPC
*/

int CzyDziala = 1;
//id zasobow - https://www.geeksforgeeks.org/operating-systems/ipc-shared-memory/
int id_pamieci;
int id_semafora; //to na przyszlosc
int id_kolejki; //to na przyszlosc

PamiecDzielona *sklep;

//CNTRL C
void ObslugaSygnalu(int signal){
	printf("Sygnal zamkniecia sklepu ->koniec");
	CzyDziala = 0;
}

void cleanUp()
{
	if (sklep != NULL)
	{
		odlacz_pamiec_dzielona(sklep);
		if (id_pamieci != -1)
		{
			usun_pamiec_dzielona(id_pamieci);
		}
		zwolnijSemafor(id_semafora, 0);
		printf("Pamiec wyczyszczona");
	}

}


/*



id_pamieci = utworz_pamiec_dzielona()
*/

int main()
{
	//ftok potrzebuje pliku do wygenerowania klucza
	FILE* fptr;
	fptr = fopen("/tmp/dyskont_projekt", "w");
	if (fptr != NULL)
	{
		fclose(fptr);
	}

	key_t klucz = ftok("/tmp/dyskont_projekt", 'S');
	if (klucz == -1)
	{
		perror("blad ftok");
		exit(1);
	}

	id_semafora = alokujSemafor(klucz, 2, 0666 | IPC_CREAT); // potem zmien na minimalne uprawnienia !!!!!!!!!!!!!!!!
	inicjalizujSemafor(id_semafora, SEM_KASY, 1); //otwrty na start
	inicjalizujSemafor(id_semafora, SEM_UTARG, 1);// tak samo otwartyS

	//tworzenie zasobow -> wywołane od sizeof bo "nie znamy" wielkosci pamiecidziel..
	id_pamieci = utworz_pamiec_dzielona(sizeof(PamiecDzielona));
	sklep = mapuj_pamiec_dzielona(id_pamieci);

	sklep -> liczba_produktow = 32;
	for (int j=0; j<KASY_SAMOOBSLUGOWE ;j++ )
	{
		sklep -> kasy_samo[j].otwarta = 1;
	}

	//podpiecie SIGINT do voida z obsluga sygnalu
	signal(SIGINT, ObslugaSygnalu);





	while (CzyDziala == 1)
	{
		printf("Kierownik, PID - %d\n", getpid());
		waitSemafor(id_semafora, SEM_KASY, 0);
		signalSemafor(id_semafora, SEM_KASY);


		sleep(1);
	}
	cleanUp();
	printf("Koniec pracy");

	return 0;
}