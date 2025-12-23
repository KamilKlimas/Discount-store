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
#include <time.h>


// proces glowny - do uzupelniania:
/*
    - obsluga sygnalow
    - czyszczenie IPC
*/

int CzyDziala = 1;

//id zasobow - https://www.geeksforgeeks.org/operating-systems/ipc-shared-memory/
int id_pamieci;
int id_semafora;
int id_kolejki;

PamiecDzielona *sklep;

//CNTRL C
void ObslugaSygnalu(int signal){
	if (sklep != NULL) {
		sklep->czy_otwarte = 0;
	}
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
		usun_kolejke(id_kolejki);
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

double cennik[8][4] = {
	{3.50, 4.20, 5.00, 9.99},// Ceny owoców
	{2.00, 2.50, 8.50, 6.00},// Ceny warzyw
	{4.50, 0.90, 3.20, 3.50},// Pieczywo
	{3.80, 25.00, 1.90, 6.50},// Nabiał
	{4.00, 30.00, 40.00, 80.00},// Alkohol
	{45.00, 28.00, 32.00, 55.00},// Wędliny
	{12.00, 18.00, 15.00, 7.50},// Mrożonki
	{35.00, 8.00, 3.00, 12.00}// Chemia
};


int main()
{
	time_t t = time(NULL);
	struct tm *currentTime = localtime(&t);

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
	inicjalizujSemafor(id_semafora, SEM_UTARG, 1);// tak samo otwarty

	id_kolejki = stworzKolejke();

	//tworzenie zasobow -> wywołane od sizeof bo "nie znamy" wielkosci pamiecidziel..
	id_pamieci = utworz_pamiec_dzielona(sizeof(PamiecDzielona));
	sklep = mapuj_pamiec_dzielona(id_pamieci);

	sklep->statystyki.czas_startu = time(NULL);
	sklep->statystyki.paragon_klienta = 0.0;
	sklep ->statystyki.ewakuacja = 0;
	sklep->statystyki.liczba_klientow_w_sklepie =0;
	sklep->statystyki.liczba_obsluzonych_klientow =0;
	sklep->statystyki.suma_sprzedazy = 0.0;
	sklep->statystyki.utarg = 0.0;

	sklep -> liczba_produktow = 32;
	for (int j=0; j<KASY_SAMOOBSLUGOWE ;j++ )
	{
		sklep -> kasy_samo[j].otwarta = 1;
		sklep -> kasy_samo[j].zajeta = 0;
		sklep -> kasy_samo[j].platnosc_w_toku = 0;
		sklep->kasy_samo[j].obslugiwany_klient = 0;
	}

	for (int i =0; i<KASY_STACJONARNE ;i++ )
	{
		sklep->kasa_stato[i].otwarta = 0;
		sklep->kasa_stato[i].zajeta  = 0;
		sklep -> kasa_stato[i].platnosc_w_toku = 0;
		sklep->kasa_stato[i].obslugiwany_klientl = 0;
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
		sklep ->produkty[i].cena = cennik[id_kategoria][id_konkretnego];
		sklep ->produkty[i].sztuk = 50;
	}

	printf("\nKierownik: sklep zatowarowany\n");

	sklep->czy_otwarte = 1;



	while (CzyDziala == 1)
	{
		printf("\nKierownik, PID - %d\n", getpid());
		sleep(1);
	}

	while (1)
	{
		if (sklep->statystyki.liczba_klientow_w_sklepie == 0)
		{
			printf("\nWszyscy klienci juz skonczyli zakupy, mozna zamykac\n");
			break;
		}
		printf("\nczekam na ostatnich %d klientow\n", sklep->statystyki.liczba_klientow_w_sklepie);
		sleep(1);
	}
	printf("\nSygnal zamkniecia sklepu ->koniec\nnnn");
	printf("\nRAPORT KONCOWY:%04d-%02d-%02d\n---Utarg: %.2f---\n---Obsluzeni Klienci: %d---",
		currentTime->tm_year + 1900, currentTime->tm_mon + 1, currentTime->tm_mday,
			sklep->statystyki.utarg,
			sklep->statystyki.liczba_obsluzonych_klientow);

	FILE *f;
	f = fopen("Raport-dnia", "a");
	fprintf(f, "\nRAPORT KONCOWY:%04d-%02d-%02d\n---Utarg: %.2f---\n---Obsluzeni Klienci: %d---",
		currentTime->tm_year + 1900, currentTime->tm_mon + 1, currentTime->tm_mday,
			sklep->statystyki.utarg,
			sklep->statystyki.liczba_obsluzonych_klientow);
	fclose(f);
	cleanUp();
	printf("\nKoniec pracy\n");

	return 0;
}
//