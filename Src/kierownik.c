//
// Created by kamil-klimas on 10.12.2025.
//
#include "ipc.h"
#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
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
	printf("\nSygnal zamkniecia sklepu ->koniec\nnnn");
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
		printf("\nPamiec wyczyszczona\n");
	}

}


/*



id_pamieci = utworz_pamiec_dzielona()
*/

//slowniki z pythona tęsknie...
const char* baza_towarow[8][4] = {
	{"Jablko", "Banan", "Gruszka", "Winogrono"},// 0: Owoce
	{"Ziemniak", "Marchew", "Pomidor", "Ogorek"},// 1: Warzywa
	{"Chleb", "Bulka", "Bagietka", "Paczek"},// 2: Pieczywo
	{"Mleko", "Ser zolty", "Jogurt", "Maslo"},// 3: Nabial
	{"Piwo", "Wino", "Wodka", "Whisky"},// 4: Alkohol
	{"Szynka", "Kielbasa", "Boczek", "Kabanos"},// 5: Wedliny
	{"Frytki", "Pizza", "Lody", "Warzywa na patelnie"},// 6: Mrozonki
	{"Proszek", "Plyn do naczyn", "Mydlo", "Domestos"}// 7: Chemia
};

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
		perror("\nblad ftok\n");
		exit(1);
	}

	id_semafora = alokujSemafor(klucz, 2, 0666 | IPC_CREAT); // potem zmien na minimalne uprawnienia !!!!!!!!!!!!!!!!
	inicjalizujSemafor(id_semafora, SEM_KASY, 1); //otwrty na start
	inicjalizujSemafor(id_semafora, SEM_UTARG, 1);// tak samo otwartyS

	//tworzenie zasobow -> wywołane od sizeof bo "nie znamy" wielkosci pamiecidziel..
	id_pamieci = utworz_pamiec_dzielona(sizeof(PamiecDzielona));
	sklep = mapuj_pamiec_dzielona(id_pamieci);

	sklep->statystyki.czas_startu = time(NULL);
	sklep ->statystyki.ewakuacja = 0;
	sklep->statystyki.liczba_klientow_w_sklepie =0;
	sklep->statystyki.liczba_obsluzonych_klientow =0;
	sklep->statystyki.suma_sprzedazy = 0.0;
	sklep->statystyki.utarg = 0.0;

	sklep -> liczba_produktow = 32;
	for (int j=0; j<KASY_SAMOOBSLUGOWE ;j++ )
	{
		sklep -> kasy_samo[j].otwarta = 1;
	}

	//podpiecie SIGINT do voida z obsluga sygnalu
	signal(SIGINT, ObslugaSygnalu);

	printf("\nKierownik: Wykładanie towaru na półki...\n");

	for (int i =0; i < sklep->liczba_produktow; i++)
	{
		int id_kategoria = i%KATEGORIE;
		int id_konkretnego = (i/KATEGORIE)%4;
		strcpy(sklep ->produkty[i].nazwa, baza_towarow[id_kategoria][id_konkretnego]);
		strcpy(sklep->produkty[i].kategoria, nazwy_kategorii[id_kategoria]);

		sklep -> produkty[i].cena = 2.0 + (rand() % 2000)/ 100.0;
		sklep ->produkty[i].sztuk = 50;
	}

	printf("\nKierownik: sklep zatowarowany\n");




	while (CzyDziala == 1)
	{
		printf("\nKierownik, PID - %d\n", getpid());
		sleep(1);
	}
	cleanUp();
	printf("\nKoniec pracy\n");

	return 0;
}