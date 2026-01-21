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

//#define TRYB_TURBO

#ifdef TRYB_TURBO
	#define SIM_SLEEP_US(x) (void)(x)
	#define SIM_SLEEP_S(x)  (void)(x)
#else
	#define SIM_SLEEP_US(x) usleep(x)
	#define SIM_SLEEP_S(x)  sleep(x)
#endif

#define FTOK_PATH "/tmp/dyskont_projekt"

// Definicje kolorów ANSI
#define ANSI_RESET   "\x1b[0m"
#define ANSI_BOLD    "\x1b[1m"
#define ANSI_RED     "\x1b[31m"      // Kierownik / Alarmy
#define ANSI_GREEN   "\x1b[32m"      // Klient (zakupy)
#define ANSI_YELLOW  "\x1b[33m"      // Pracownik
#define ANSI_BLUE    "\x1b[34m"      // Kasjer
#define ANSI_MAGENTA "\x1b[35m"      // Kasa Samoobsługowa
#define ANSI_CYAN    "\x1b[36m"      // Generator / System
#define ANSI_WHITE   "\x1b[37m"

#define LOG_FILE_KIEROWNIK "logi_kierownik.txt"
#define LOG_FILE_KASJER "logi_kasjer.txt"
#define LOG_FILE_KLIENT "logi_klient.txt"
#define LOG_FILE_KASA_SAMO "logi_kasa_samo.txt"
#define LOG_FILE_PRACOWNIK "logi_pracownik.txt"
#define LOG_FILE_GENERATOR "logi_generator.txt"
#define LOG_FILE_SYSTEM "logi_system.txt"

#define LOG_TO_FILE(file, prefix, fmt, ...) do { \
FILE *_lf = fopen(file, "a"); \
if (_lf) { \
fprintf(_lf, prefix fmt "\n", ##__VA_ARGS__); \
fclose(_lf); \
} \
} while(0)

#ifdef TRYB_TURBO
#define LOG_KIEROWNIK(fmt, ...) do { \
LOG_TO_FILE(LOG_FILE_KIEROWNIK, "[KIEROWNIK] ", fmt, ##__VA_ARGS__); \
} while(0)
#else
#define LOG_KIEROWNIK(fmt, ...) do { \
printf(ANSI_BOLD ANSI_RED "[KIEROWNIK] " ANSI_RESET fmt "\n", ##__VA_ARGS__); \
LOG_TO_FILE(LOG_FILE_KIEROWNIK, "[KIEROWNIK] ", fmt, ##__VA_ARGS__); \
} while(0)
#endif

#define LOG_KIEROWNIK_BOTH(fmt, ...) do { \
printf(ANSI_BOLD ANSI_RED "[KIEROWNIK] " ANSI_RESET fmt "\n", ##__VA_ARGS__); \
LOG_TO_FILE(LOG_FILE_KIEROWNIK, "[KIEROWNIK] ", fmt, ##__VA_ARGS__); \
} while(0)

#ifdef TRYB_TURBO
#define LOG_KASJER(id, fmt, ...) do { \
LOG_TO_FILE(LOG_FILE_KASJER, "[KASJER %d] ", fmt, id, ##__VA_ARGS__); \
} while(0)
#else
#define LOG_KASJER(id, fmt, ...) do { \
printf(ANSI_BLUE "[KASJER %d] " ANSI_RESET fmt "\n", id, ##__VA_ARGS__); \
LOG_TO_FILE(LOG_FILE_KASJER, "[KASJER %d] ", fmt, id, ##__VA_ARGS__); \
} while(0)
#endif

#ifdef TRYB_TURBO
#define LOG_KLIENT(pid, fmt, ...) do { \
LOG_TO_FILE(LOG_FILE_KLIENT, "[KLIENT %d] ", fmt, pid, ##__VA_ARGS__); \
} while(0)
#else
#define LOG_KLIENT(pid, fmt, ...) do { \
printf(ANSI_GREEN "[KLIENT %d] " ANSI_RESET fmt, pid, ##__VA_ARGS__); \
LOG_TO_FILE(LOG_FILE_KLIENT, "[KLIENT %d] ", fmt, pid, ##__VA_ARGS__); \
} while(0)
#endif

#ifdef TRYB_TURBO
#define LOG_KASA_SAMO(id, fmt, ...) do { \
LOG_TO_FILE(LOG_FILE_KASA_SAMO, "[KASA SAMO %d] ", fmt, id, ##__VA_ARGS__); \
} while(0)
#else
#define LOG_KASA_SAMO(id, fmt, ...) do { \
printf(ANSI_MAGENTA "[KASA SAMO %d] " ANSI_RESET fmt "\n", id, ##__VA_ARGS__); \
LOG_TO_FILE(LOG_FILE_KASA_SAMO, "[KASA SAMO %d] ", fmt, id, ##__VA_ARGS__); \
} while(0)
#endif

#ifdef TRYB_TURBO
#define LOG_PRACOWNIK(fmt, ...) do { \
LOG_TO_FILE(LOG_FILE_PRACOWNIK, "[PRACOWNIK] ", fmt, ##__VA_ARGS__); \
} while(0)
#else
#define LOG_PRACOWNIK(fmt, ...) do { \
printf(ANSI_YELLOW "[PRACOWNIK] " ANSI_RESET fmt "\n", ##__VA_ARGS__); \
LOG_TO_FILE(LOG_FILE_PRACOWNIK, "[PRACOWNIK] ", fmt, ##__VA_ARGS__); \
} while(0)
#endif

#ifdef TRYB_TURBO
#define LOG_GENERATOR(fmt, ...) do { \
LOG_TO_FILE(LOG_FILE_GENERATOR, "[GENERATOR] ", fmt, ##__VA_ARGS__); \
} while(0)
#else
#define LOG_GENERATOR(fmt, ...) do { \
printf(ANSI_CYAN "[GENERATOR] " ANSI_RESET fmt "\n", ##__VA_ARGS__); \
LOG_TO_FILE(LOG_FILE_GENERATOR, "[GENERATOR] ", fmt, ##__VA_ARGS__); \
} while(0)
#endif

#define LOG_GENERATOR_BOTH(fmt, ...) do { \
printf(ANSI_CYAN "[GENERATOR] " ANSI_RESET fmt "\n", ##__VA_ARGS__); \
LOG_TO_FILE(LOG_FILE_GENERATOR, "[GENERATOR] ", fmt, ##__VA_ARGS__); \
} while(0)

#ifdef TRYB_TURBO
#define LOG_SYSTEM(fmt, ...) do { \
LOG_TO_FILE(LOG_FILE_SYSTEM, "[SYSTEM] ", fmt, ##__VA_ARGS__); \
} while(0)
#else
#define LOG_SYSTEM(fmt, ...) do { \
printf(ANSI_BOLD ANSI_WHITE "[SYSTEM] " ANSI_RESET fmt "\n", ##__VA_ARGS__); \
LOG_TO_FILE(LOG_FILE_SYSTEM, "[SYSTEM] ", fmt, ##__VA_ARGS__); \
} while(0)
#endif


//Global constants - stałe systemu
#define MAX_PRODUKTOW 50
#define KASY_SAMOOBSLUGOWE 6 // "...łącznie 8 kas: 2 kasy stacjonarne i 6 kas samoobsługowych."
#define KASY_STACJONARNE 2
#define MIN_PRODUKTOW_KOSZYK 3
#define SZANSA_SAMOOBSLUGA 95 // "Większość klientów korzysta z kas samoobsługowych (ok. 95%), pozostali (ok. 5%) stają w kolejce do kas stacjonarnych."
#define KANAL_KASJERA_OFFSET 100

#define MARGINES_KOLEJKI_BAJTY 4096
#define MAX_MIEJSC_W_SKLEPIE 120
#define MAX_LICZBA_KLIENTOW 20000

//Dynamic checkout opening menagment - dynamiczne otwieranie kas
#define K_KLIENTOW_NA_KASE 5 //For K customers 1 checkout - co K klientów 1 kasa
#define MIN_CZYNNYCH_KAS_SAMO 3 // "Zawsze działają co najmniej 3 kasy samoobsługowe;"

//Staffed checkout - kasa stacjonarna
#define OTWORZ_KASE_1_PRZY 3 // "Jeżeli liczba osób stojących w kolejce do kasy jest większa niż 3 otwierana jest kasa 1;"
#define ZAMKNIJ_KASE_PO 30 // "Jeżeli po obsłużeniu ostatniego klienta w kolejce przez 30 sekund nie pojawi się następny klient, kasa jest zamykana;"
#define MAX_CZAS_OCZEKIWANIA 10 // "Jeżeli czas oczekiwania w kolejce na kasę samoobsługową jest dłuższy niż T..."

//Kolejka do kas
#define MAX_DLUGOSC_KOLEJKI 300


//semafory
#define SEM_KASY 0
#define SEM_UTARG 1
#define SEM_KOLEJKI 2
#define SEM_WEJSCIE 3

#define KATEGORIE 8
#define LICZBA_PRODUKTOW 32
static const char* nazwy_kategorii[KATEGORIE] __attribute__((unused)) = {
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
	int sztuk;
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
	int platnosc_w_toku;
	int alkohol;
	int wiek_klienta;
	float aktualna_kwota;
} KasaSamoobslugowa;

//strukt kasa stacjonarna
typedef struct {
	int otwarta;
	int zajeta;
	pid_t kasjer_pid;
	pid_t obslugiwany_klientl;
	int liczba_obsluzonych;
	float suma_sprzedazy;
	int platnosc_w_toku;
	time_t czas_ostatniej_obslugi;
	int zamykanie_w_toku;
	int liczba_do_obsluzenia;
} KasaStacjonarna;

//statystyki
typedef struct {
	int liczba_klientow_w_sklepie;
	int liczba_obsluzonych_klientow;
	float suma_sprzedazy;
	time_t czas_startu;
	int ewakuacja;
	float paragon_klienta;
	double utarg;
	int klienci_w_kolejce_do_stacjonarnej;
} StatystykiGlobalne;

//struktura IPC (pamiec dielona)
typedef struct {
	StatystykiGlobalne statystyki;
	KasaSamoobslugowa kasy_samo[KASY_SAMOOBSLUGOWE];
	KasaStacjonarna kasa_stato[KASY_STACJONARNE ];

	KolejkaKlientow kolejka_samoobsluga; //<- wspolna kolejka do kas samoobslugowych
	KolejkaKlientow kolejka_stato[KASY_STACJONARNE ];

	int czy_otwarte;

	Produkt produkty[MAX_PRODUKTOW];
	int liczba_produktow;

	pid_t kierownik_pid;
	pid_t pracownik_pid;
} PamiecDzielona;

struct messg_buffer
{
	long mesg_type;
	double kwota;
	int ID_klienta;
	int wiek;
	int ma_alkohol;
};


//FUNKCJE IPC
key_t utworz_klucz(char id);
int utworz_pamiec_dzielona(size_t rozmiar);
int podlacz_pamiec_dzielona();
PamiecDzielona *mapuj_pamiec_dzielona(int shmid);
void odlacz_pamiec_dzielona(PamiecDzielona *shm);
void usun_pamiec_dzielona(int shmid);

//funkcje semafory
int alokujSemafor(key_t klucz, int number, int flagi);
int zwolnijSemafor(int semID, int number);
void inicjalizujSemafor(int semID, int number, int val);
int waitSemafor(int semID, int number, int flags);
int signalSemafor(int semID, int number);
int valueSemafor(int semID, int number);

//funckje kolejka komunikatow
int stworzKolejke();
int WyslijDoKolejki(int msgid, struct messg_buffer *msg);
int BezpieczneWyslanieKlienta(int msgid, struct messg_buffer *msg);
int OdbierzZKolejki(int msgid, struct messg_buffer *msg,long typ_adresata);
void usun_kolejke(int msgid);

//FIFO
void inicjalizujKolejkeFIFO(KolejkaKlientow *k);
int dodajDoKolejkiFIFO(KolejkaKlientow *k,pid_t pid);
pid_t zdejmijZKolejkiFIFO(KolejkaKlientow *k);
pid_t podejrzyjPierwszegoFIFO(KolejkaKlientow *k);
int usunZSrodkaKolejkiFIFO(KolejkaKlientow *k,pid_t pid);

//Handler do walidacji danych wejsciowych
int inputExceptionHandler(const char * komunikat);

#endif
