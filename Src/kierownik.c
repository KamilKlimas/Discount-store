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
#include <sys/wait.h>


int CzyDziala = 1;
int id_pamieci;
int id_semafora;
int id_kolejki;

PamiecDzielona *sklep;

pid_t kasjer1_pid = 0;
pid_t kasjer2_pid = 0;

pid_t pids_samo[KASY_SAMOOBSLUGOWE];

//CNTRL C
void ObslugaSygnalu(int signal){
	if (signal == SIGINT) {
		printf("\nKierownik Otrzymano SIGINT. Zamykam sklep dla nowych klientow.\n");
		if(sklep != NULL) sklep->czy_otwarte = 0;
		CzyDziala = 0;
	}
}

void OtworzKase2(int sig)
{
	printf("\n Otwwieram kase numer 2\n");

	if (sklep != NULL)
	{
		waitSemafor(id_semafora, SEM_KASY, 0);
		sklep->kasa_stato[1].otwarta = 1;;
		sklep->kasa_stato[1].zamykanie_w_toku = 0;
		sklep->kasa_stato[1].czas_ostatniej_obslugi = time(NULL);
		signalSemafor(id_semafora,SEM_KASY);

		if (kasjer2_pid  > 0) kill(kasjer2_pid, SIGUSR1);
	}
}

void ZamknijKase(int sig)
{
	printf("\n zamykam kase\n");
	if (sklep != NULL)
	{
		waitSemafor(id_semafora,SEM_KASY, 0);
		if (sklep->kasa_stato[1].otwarta)
		{
			sklep->kasa_stato[1].zamykanie_w_toku = 1;
			printf("\n zamykam KASA 2...\n");
		}else if (sklep->kasa_stato[0].otwarta)
		{
			sklep->kasa_stato[0].zamykanie_w_toku = 1;
			printf("\n zamykam KASA 1...\n");
		}
		signalSemafor(id_semafora,SEM_KASY);
	}
}


void cleanUp()
{

	if (sklep != NULL)
	{
		odlacz_pamiec_dzielona(sklep);
	}
	if (id_pamieci != -1)
	{
		usun_pamiec_dzielona(id_pamieci);
	}
	zwolnijSemafor(id_semafora, SEM_KASY);
	zwolnijSemafor(id_semafora, SEM_UTARG);
	zwolnijSemafor(id_semafora, SEM_KOLEJKI);

	usun_kolejke(id_kolejki);

	remove(FTOK_PATH);
}

void Ewakuacja(int sig) {
	printf("\nKierownik ALARM! EWAKUACJA!\n");
	signal(SIGQUIT, SIG_IGN);

	if (sklep != NULL) {
		sklep->statystyki.ewakuacja = 1;
		sklep->czy_otwarte = 0;
		kill(0, SIGQUIT);
	}

	printf("Kierownik: Czekam na opuszczenie sklepu przez klientow i personel...\n");
	sleep(2);

	cleanUp();
	printf("\nUszami rusz i znikaj juz\n");
	exit(0);
}

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
	setbuf(stdout, NULL);

	signal(SIGQUIT, Ewakuacja);
	signal(SIGINT, ObslugaSygnalu);
	signal(SIGUSR1, OtworzKase2);
	signal(SIGUSR2, ZamknijKase);

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

	id_semafora = alokujSemafor(klucz, 3, 0666 | IPC_CREAT); // potem zmien na minimalne uprawnienia !!!!!!!!!!!!!!!!
	inicjalizujSemafor(id_semafora, SEM_KASY, 1);
	inicjalizujSemafor(id_semafora, SEM_UTARG, 1);
	inicjalizujSemafor(id_semafora, SEM_KOLEJKI, 1);

	id_kolejki = stworzKolejke();

	//tworzenie zasobow -> wywołane od sizeof bo "nie znamy" wielkosci pamiecidziel..
	id_pamieci = utworz_pamiec_dzielona(sizeof(PamiecDzielona));
	sklep = mapuj_pamiec_dzielona(id_pamieci);

	sklep->statystyki.czas_startu = time(NULL);
	//sklep->statystyki.paragon_klienta = 0.0;
	sklep ->statystyki.ewakuacja = 0;
	sklep->statystyki.liczba_klientow_w_sklepie =0;
	sklep->statystyki.liczba_obsluzonych_klientow =0;
	//sklep->statystyki.suma_sprzedazy = 0.0;
	sklep->statystyki.utarg = 0.0;
	sklep->statystyki.liczba_obsluzonych_klientow = 0;
	//sklep->statystyki.suma_sprzedazy = 0.0;
	sklep->statystyki.klienci_w_kolejce_do_stacjonarnej = 0;
	sklep->czy_otwarte = 1;
	sklep -> liczba_produktow = 32;

	inicjalizujKolejkeFIFO(&sklep->kolejka_samoobsluga);
	for (int i =0; i<KASY_STACJONARNE ;i++ )
	{
		inicjalizujKolejkeFIFO(&sklep->kolejka_stato[i]);
	}
	for (int i =0; i<KASY_STACJONARNE ;i++ )
	{
		sklep->kasa_stato[i].otwarta = 0;
		sklep->kasa_stato[i].zajeta  = 0;
		sklep->kasa_stato[i].czas_ostatniej_obslugi = time(NULL);
	}

	printf("\nKierownik: Uruchomiono %d terminali kas samoobslugowych.\n", KASY_SAMOOBSLUGOWE);

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

	if (fork() == 0)
	{
		printf("\n pracownik proszony na stanowisko\n");
		execlp("./pracownik", "pracownik",NULL);
		exit(1);
	}
	sleep(1);

	kasjer1_pid = fork();
	if (kasjer1_pid == 0)
	{
		printf("\n Kasjer 1 proszony o gotowosc\n");
		execlp("./kasjer", "kasjer","0", NULL);
		exit(1);
	}
	sleep(1);

	kasjer2_pid = fork();
	if (kasjer2_pid == 0)
	{
		printf("\n Kasjer 2 proszony o gotowosc\n");
		execlp("./kasjer", "kasjer","1", NULL);
		exit(1);
	}
	sleep(1);

	for (int i = 0; i < KASY_SAMOOBSLUGOWE; i++) {
		pids_samo[i] = fork();
		if (pids_samo[i] == 0) {
			char arg_id[5];
			sprintf(arg_id, "%d", i);
			execlp("./kasy_samo", "kasy_samo", arg_id, NULL);
			perror("Blad execlp kasy_samo");
			exit(1);
		}
	}
	sleep(1);

	printf("\n sklep otwarty. PID %d\n", getpid());
	printf("\nDostepne polecenia: "
		"\n - kill -SIGUSR1 %d (Otworz Kase 2) "
		"\n - kill -SIGUSR2 (Zamknij kase)"
		"\n - SIGQUIT (ewakuacja)\n", getpid());



	while (CzyDziala == 1)
	{
        waitSemafor(id_semafora, SEM_KASY, 0);
        int klienci_total = sklep->statystyki.liczba_klientow_w_sklepie;

        int aktualne_czynne = 0;
        for (int i=0; i<KASY_SAMOOBSLUGOWE; i++) {
            if (sklep->kasy_samo[i].otwarta)
            {
	            aktualne_czynne+=1;
            }
        }
        signalSemafor(id_semafora, SEM_KASY);

        //otwieranie: Na każdych K klientów min 1 kasa, ale min 3
        int wymagane_samo = (klienci_total + K_KLIENTOW_NA_KASE - 1) / K_KLIENTOW_NA_KASE;
        if (wymagane_samo < MIN_CZYNNYCH_KAS_SAMO) wymagane_samo = MIN_CZYNNYCH_KAS_SAMO;
        if (wymagane_samo > KASY_SAMOOBSLUGOWE) wymagane_samo = KASY_SAMOOBSLUGOWE;

        waitSemafor(id_semafora, SEM_KASY, 0);

        if (aktualne_czynne < wymagane_samo) {
            for (int i=0; i<KASY_SAMOOBSLUGOWE; i++) {
                if (!sklep->kasy_samo[i].otwarta && aktualne_czynne < wymagane_samo) {
                    sklep->kasy_samo[i].otwarta = 1;
                    sklep->kasy_samo[i].zablokowana = 0;
                    sklep->kasy_samo[i].aktualna_kwota = 0.0; // Reset
                    aktualne_czynne++;
                    printf("\nKierownik: Otwieram kase samoobslugowa %d (Klienci: %d)\n", i, klienci_total);
                }
            }
        }
        // zamykanie: Jeśli liczba klientów jest mniejsza niż K*(N-3)
        else if (aktualne_czynne > MIN_CZYNNYCH_KAS_SAMO) {
            int prog_zamykania = K_KLIENTOW_NA_KASE * (aktualne_czynne - 3);

            if (klienci_total < prog_zamykania) {
                for (int i = KASY_SAMOOBSLUGOWE - 1; i >= 0; i--) {
                    if (sklep->kasy_samo[i].otwarta && !sklep->kasy_samo[i].zajeta) {
                        sklep->kasy_samo[i].otwarta = 0;
                        printf("\nKierownik: Zamykam kase samoobslugowa %d (Klienci: %d < Prog: %d)\n", i, klienci_total, prog_zamykania);
                        break;
                    }
                }
            }
        }
        signalSemafor(id_semafora, SEM_KASY);

		//kasy stacjonarne
		waitSemafor(id_semafora, SEM_KOLEJKI, 0);
		int kolejka_do_k1 = sklep->kolejka_stato[0].rozmiar;
		signalSemafor(id_semafora, SEM_KOLEJKI);

		waitSemafor(id_semafora, SEM_KASY, 0);
		if (kolejka_do_k1 > 3 && sklep->kasa_stato[0].otwarta == 0)
		{
			printf("\nKolejka do kasy nr1: %d. Otwieram kase...\n",kolejka_do_k1);
			sklep->kasa_stato[0].otwarta = 1;
			sklep->kasa_stato[0].zamykanie_w_toku = 0;
			kill(kasjer1_pid, SIGUSR1);
		}
		signalSemafor(id_semafora, SEM_KASY);

		for (int j=0; j<KASY_STACJONARNE; j++)
		{
			if (sklep->kasa_stato[j].otwarta == 1)
			{
				waitSemafor(id_semafora, SEM_KOLEJKI, 0);
				int pusta = (sklep->kolejka_stato[j].rozmiar == 0);
				signalSemafor(id_semafora, SEM_KOLEJKI);

				if (pusta && sklep->kasa_stato[j].zamykanie_w_toku == 0)
				{
					double seconds = difftime(time(NULL), sklep->kasa_stato[j].czas_ostatniej_obslugi);
					if (seconds > ZAMKNIJ_KASE_PO || sklep->kasa_stato[j].zamykanie_w_toku == 1)
					{
						printf("\nZamykam kase stacjonarna %d\n", j);
						waitSemafor(id_semafora, SEM_KASY, 0);
						sklep->kasa_stato[j].otwarta = 0;
						sklep->kasa_stato[j].zamykanie_w_toku = 0;
						pid_t pid_j = (j==0) ? kasjer1_pid : kasjer2_pid;
						kill(pid_j, SIGUSR2);
						signalSemafor(id_semafora, SEM_KASY);
					}else
					{
						waitSemafor(id_semafora, SEM_KASY, 0);
						if (sklep->kasa_stato[j].zajeta == 1)
						{
							sklep->kasa_stato[j].czas_ostatniej_obslugi = time(NULL);
						}
						signalSemafor(id_semafora, SEM_KASY);
					}
				}
			}
		}
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
	wait(NULL);
	printf("\nSygnal zamkniecia sklepu ->koniec\n");
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
	printf("\nKoniec pracy\n");
	kill(kasjer1_pid, SIGQUIT);
	kill(kasjer2_pid, SIGQUIT);
	for (int i = 0; i < KASY_SAMOOBSLUGOWE; i++) {
		kill(pids_samo[i], SIGQUIT);
	}
	cleanUp();


	return 0;
}