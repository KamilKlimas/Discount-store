//
// Created by kamil-klimas on 10.12.2025.
//

#ifndef IPC_H
#define IPC_H

#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/msg.h>
#include <sys/types.h>
#include <time.h>

#define FTOK_PATH "/CLionProjects/Discount-store"

//Global constants - stałe systemu
#define MAX_KLIENTOW 100
#define MAX_PRODUKTOW 50
#define KASY_SAMOOBSLUGOWE 6
#define KASY_STACJONARNE 2
#define MIN_PRODUKTOW_KOSZYK 3
#define SZANSA_SAMOOBSLUGA 95

//szanse na zasniecie w kolejce

//rodzicdziecko


//Dynamic checkout opening menagment - dynamiczne otwieranie kas
#define K_KLIENTOW_NA_KASE 10 //For K customers 1 checkout - co K klientów 1 kasa
#define MIN_CZYNNYCH_KAS_SAMO 3

//Staffed checkout - kasa stacjonarna
#define OTWORZ_KASE_1_PRZY 3 // >=3 customers in queue - >=3 klientow w kolejce
#define ZAMKNIJ_KASE_PO 30 // 30 sec waittime without customers - 30 sekund oczekiwania brak klientow

//Kolejka do kas
#define MAX_DLUGOSC_KOLEJKI 200
#define MAX_CZAS_OCZEKIWANIA 60

#define KATEGORIE 8
static const char* nazwy_kategorii[KATEGORIE] = {
	"Owoce",
	"Warzywa",
	"Pieczywo",
	"Nabial",
	"Alkohol",
	"Wedliny",
	"Mrozonki",
	"Chemia"
};
//strukt na typ produktu
typedef struct {
	char nazwa[50];
	char kategoria[20];
	float cena;
}Produkt;

//strukt zakupy klienta
typedef struct {
	int produkty_ID[K_KLIENTOW_NA_KASE];
	int liczba_produktow;
	int ma_alkohol;
} KoszykZakupow;

//strukt klient w kolejce
typedef struct {
	pid_t pid;
	int id;
	time_t czas_wejscia;
//dopisz potem do usypiania + rodzic dziecko
} KlientWKolejce;

//FIFO
typedef struct {
	KlientWKolejce klienci[MAX_DLUGOSC_KOLEJKI];
	int poczatek;
	int koniec;
	int rozmiar;
} KolejkaKlientow;

//strukt kasa samoobslugowa
typedef struct{
	int otwarta;
	int zajeta;
	int zablokowana;
	pid_t obslugiwany_klient;
	int liczba_obsluzonych;
	float suma_sprzedazy;
} KasaSamoobslugowa;

//strukt kasa stacjonarna
typedef struct {
	int otwarta;
	int zajeta;
	pid_t kasjer_pid;
	pid_t obslugiwany_klientl
	int liczba_obsluzonych;
	float suma_sprzedazy;
} KasaStacjonarna;

//statystyki
typedef struct {
	int liczba_klientow_w_sklepie;
	int liczba_obsluzonych_klientow;
	float suma_sprzedazy;
	time_t czas_startu;
	int ewakuacja;
	//rodzicdziecko, zasniecia ??
} StatystykiGlobalne;

//struktura IPC (pamiec dielona)
typedef struct {
	StatystykiGlobalne statystyki;
	KasaSamoobslugowa kasy_samo[KASY_SAMOOBSLUGOWE];
	KasaStacjonarna kasa_stato[KASY_STACJONERNE];

	KolejkaKlientow kolejka_samoobsluga;
	KolejkaKlientow kolejka_stato[KASY_STACJONERNE];

	Produkt produkty[MAX_PRODUKTOW];
	int liczba_produktow;

	pid_t kierownik_pid;
	pid_t pracownik_pid;
} PamiecDzielona;

//FUNKCJE IPC
key_t utworz_klucz(char id);
int utworz_pamiec_dzielona(size_t rozmiar);
int podlacz_pamiec_dzielona();
PamiecDzielona *mapuj_pamiec_dzielona(int shmid);
void odlacz_pamiec_dzielona(PamiecDzielona *shm);
void usun_pamiec_dzielona(int shmid);