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
#include <pthread.h>
#include <errno.h>

volatile sig_atomic_t CzyDziala = 1;
volatile sig_atomic_t zadano_zamkniecie = 0;
volatile sig_atomic_t tryb_ewakuacji = 0;

int id_pamieci = -1;
int id_semafora = -1;
int id_kolejki = -1;

PamiecDzielona *sklep;

pid_t kasjer1_pid = 0;
pid_t kasjer2_pid = 0;
pid_t pids_samo[KASY_SAMOOBSLUGOWE];
pid_t pracownik_pid = 0;

pthread_t t_cleanup;
int id_sem_cleanup = -1;
int sprzatanie_rozpoczete = 0;

void obslugaZombie(int sig) {
    (void) sig;
    while (waitpid(-1, NULL, WNOHANG) > 0);
}

void GenerujRaport() {
    if (sklep == NULL) return;
time_t t = time(NULL);
    struct tm *currentTime = localtime(&t);

    // "Raport z przebiegu symulacji zapisać w pliku (plikach) tekstowym."
    LOG_KIEROWNIK_BOTH("\nRAPORT KONCOWY:%04d-%02d-%02d\n---Utarg: %.2f---\n---Obsluzeni Klienci: %d---",
                  currentTime->tm_year + 1900, currentTime->tm_mon + 1, currentTime->tm_mday,
                  sklep->statystyki.utarg,
                  sklep->statystyki.liczba_obsluzonych_klientow);

    FILE *f;
    f = fopen("Raport-dnia.txt", "a");
    if (f != NULL) {
        fprintf(f, "\nRAPORT KONCOWY:%04d-%02d-%02d\n---Utarg: %.2f---\n---Obsluzeni Klienci: %d---",
                currentTime->tm_year + 1900, currentTime->tm_mon + 1, currentTime->tm_mday,
                sklep->statystyki.utarg,
                sklep->statystyki.liczba_obsluzonych_klientow);
        fclose(f);
    }
}

void *watekCzyszczacy(void *arg) {
    (void) arg;

    struct sembuf op = {0, -1, 0};
    while (semop(id_sem_cleanup, &op, 1) == -1) {
        if (errno == EINTR) continue;
        perror("semop cleanup wait");
        pthread_exit(NULL);
    }

    if (sprzatanie_rozpoczete) pthread_exit(NULL);
    sprzatanie_rozpoczete = 1;

    LOG_KIEROWNIK("[WATEK CLEANUP] Rozpoczynam procedure czyszczenia...");
    GenerujRaport();
    LOG_KIEROWNIK("[WATEK CLEANUP] Zabijam procesy potomne...");

    if (kasjer1_pid > 0 && kill(kasjer1_pid, SIGQUIT) == -1 && errno != ESRCH) perror("kill kasjer1");
    if (kasjer2_pid > 0 && kill(kasjer2_pid, SIGQUIT) == -1 && errno != ESRCH) perror("kill kasjer2");
    if (pracownik_pid > 0 && kill(pracownik_pid, SIGQUIT) == -1 && errno != ESRCH) perror("kill pracownik");
    for (int i = 0; i < KASY_SAMOOBSLUGOWE; i++) {
        if (pids_samo[i] > 0 && kill(pids_samo[i], SIGQUIT) == -1 && errno != ESRCH) perror("kill kasa_samo");
    }

    SIM_SLEEP_US(100000);

    if (sklep != NULL) {
        odlacz_pamiec_dzielona(sklep);
        sklep = NULL;
    }
    if (id_pamieci != -1) {
        usun_pamiec_dzielona(id_pamieci);
        id_pamieci = -1;
    }
    if (id_semafora != -1) {
        if (semctl(id_semafora, 0, IPC_RMID) == -1) perror("semctl IPC_RMID"); // Usuwa cały zestaw
        id_semafora = -1;
    }
    if (id_kolejki != -1) {
        usun_kolejke(id_kolejki);
        id_kolejki = -1;
    }

    if (remove(FTOK_PATH) == -1 && errno != ENOENT) {
        perror("remove FTOK_PATH");
    }

    LOG_KIEROWNIK(ANSI_BOLD ANSI_RED "[WATEK CLEANUP] Zasoby zwolnione." ANSI_RESET);

    struct sembuf op_signal = {1, 1, 0};
    if (semop(id_sem_cleanup, &op_signal, 1) == -1) {
        perror("semop cleanup signal");
    }

    return NULL;
}

void ObslugaSygnalu(int signal) {
    if (signal == SIGINT) {
        // "Na polecenie kierownika sklepu (sygnał 2) jest zamykana dana kasa (1 lub 2)." (sygnał sterujący zamknięciem wejścia)
        LOG_KIEROWNIK_BOTH("Otrzymano SIGINT -> Zamykam sklep dla nowych klientow\n");

        CzyDziala = 0;
        zadano_zamkniecie = 1;

        if (sklep != NULL) {
            sklep->czy_otwarte = 0;
        }

        if (id_semafora != -1) {
            if (semctl(id_semafora, SEM_WEJSCIE, SETVAL, MAX_MIEJSC_W_SKLEPIE) == -1) {
                perror("semctl SETVAL SEM_WEJSCIE");
            }
        }
    }
}

void OtworzKase2(int sig) // <- kasa 2 otwierana tylko na polecenie kierownika
{
    (void) sig;
    if (sklep != NULL) {
        // "Na polecenie kierownika sklepu (sygnał 1) jest otwierana kasa 2."
        waitSemafor(id_semafora, SEM_KASY, 0);
        sklep->kasa_stato[1].otwarta = 1;
        sklep->kasa_stato[1].zamykanie_w_toku = 0;
        sklep->kasa_stato[1].liczba_do_obsluzenia = 0;
        sklep->kasa_stato[1].czas_ostatniej_obslugi = time(NULL);
        signalSemafor(id_semafora,SEM_KASY);

        if (kasjer2_pid > 0) kill(kasjer2_pid, SIGUSR1);
        LOG_KIEROWNIK_BOTH("Otwarto kase numer 2\n");
    }
}

void ZamknijKase1(int sig) {
    (void) sig;
    if (sklep != NULL) {
        // "Na polecenie kierownika sklepu (sygnał 2) jest zamykana dana kasa (1 lub 2)."
        waitSemafor(id_semafora, SEM_KASY, 0);
        if (sklep->kasa_stato[0].otwarta) {
            sklep->kasa_stato[0].zamykanie_w_toku = 1;

            waitSemafor(id_semafora, SEM_KOLEJKI, 0);
            sklep->kasa_stato[0].liczba_do_obsluzenia = sklep->kolejka_stato[0].rozmiar;
            signalSemafor(id_semafora, SEM_KOLEJKI);
            LOG_KIEROWNIK_BOTH("Zamykam KASA 1 (Do obsluzenia: %d)\n", sklep->kasa_stato[0].liczba_do_obsluzenia);
        }
        signalSemafor(id_semafora, SEM_KASY);
    }
}

void ZamknijKase2(int sig) {
    (void) sig;
    if (sklep != NULL) {
        // "Na polecenie kierownika sklepu (sygnał 2) jest zamykana dana kasa (1 lub 2)."
        waitSemafor(id_semafora, SEM_KASY, 0);
        if (sklep->kasa_stato[1].otwarta) {
            sklep->kasa_stato[1].zamykanie_w_toku = 1;

            waitSemafor(id_semafora, SEM_KOLEJKI, 0);
            sklep->kasa_stato[1].liczba_do_obsluzenia = sklep->kolejka_stato[1].rozmiar;
            signalSemafor(id_semafora, SEM_KOLEJKI);
            LOG_KIEROWNIK_BOTH("Zamykam KASA 2 (Do obsluzenia: %d)\n", sklep->kasa_stato[1].liczba_do_obsluzenia);
        }
        signalSemafor(id_semafora, SEM_KASY);
    }
}

void Ewakuacja(int sig) {
    (void) sig;
    // "Na polecenie kierownika sklepu (sygnał 3) klienci natychmiast opuszczają supermarket..."
    LOG_KIEROWNIK_BOTH("ALARM -> EWAKUACJA\n");
    if (signal(SIGQUIT, SIG_IGN) == SIG_ERR) {
        perror("signal SIGQUIT");
    }

    if (sklep != NULL) {
        sklep->statystyki.ewakuacja = 1;
        sklep->czy_otwarte = 0;
    }

    if (id_semafora != -1) {
        if (semctl(id_semafora, SEM_WEJSCIE, SETVAL, MAX_MIEJSC_W_SKLEPIE) == -1) {
            perror("semctl SETVAL SEM_WEJSCIE");
        }
    }

    if (pracownik_pid > 0 && kill(pracownik_pid, SIGQUIT) == -1 && errno != ESRCH) perror("kill pracownik");
    if (kasjer1_pid > 0 && kill(kasjer1_pid, SIGQUIT) == -1 && errno != ESRCH) perror("kill kasjer1");
    if (kasjer2_pid > 0 && kill(kasjer2_pid, SIGQUIT) == -1 && errno != ESRCH) perror("kill kasjer2");
    for (int i = 0; i < KASY_SAMOOBSLUGOWE; i++) {
        if (pids_samo[i] > 0 && kill(pids_samo[i], SIGQUIT) == -1 && errno != ESRCH) perror("kill kasa_samo");
    }

    CzyDziala = 0;
    zadano_zamkniecie = 1;
    tryb_ewakuacji = 1;
}

const char *baza_towarow[8][4] = {
    {"Jablko", "Banan", "Gruszka", "Winogrono"}, // 0: Owoce
    {"Ziemniak", "Marchew", "Pomidor", "Ogorek"}, // 1: Warzywa
    {"Chleb", "Bulka", "Bagietka", "Paczek"}, // 2: Pieczywo
    {"Mleko", "Ser zolty", "Jogurt", "Maslo"}, // 3: Nabial
    {"Piwo", "Wino", "Wodka", "Whisky"}, // 4: Alkohol
    {"Szynka", "Kielbasa", "Boczek", "Kabanos"}, // 5: Wedliny
    {"Frytki", "Pizza", "Lody", "Warzywa na patelnie"}, // 6: Mrozonki
    {"Proszek", "Plyn do naczyn", "Mydlo", "Domestos"} // 7: Chemia
};

double cennik[8][4] = {
    {3.50, 4.20, 5.00, 9.90}, // Ceny owoców
    {2.00, 2.50, 8.50, 6.00}, // Ceny warzyw
    {4.50, 0.90, 3.20, 3.50}, // Pieczywo
    {3.80, 25.00, 1.90, 6.50}, // Nabiał
    {4.00, 30.00, 40.00, 80.00}, // Alkohol
    {45.00, 28.00, 32.00, 55.00}, // Wędliny
    {12.00, 18.00, 15.00, 7.50}, // Mrożonki
    {35.00, 8.00, 3.00, 12.00} // Chemia
};

int main() {
    if (signal(SIGINT, SIG_IGN) == SIG_ERR) {
        perror("signal SIGINT");
        exit(1);
    }
    setbuf(stdout, NULL);

    if (remove(LOG_FILE_KIEROWNIK) == -1 && errno != ENOENT) perror("remove log_kierownik");
    if (remove(LOG_FILE_KASJER) == -1 && errno != ENOENT) perror("remove log_kasjer");
    if (remove(LOG_FILE_KLIENT) == -1 && errno != ENOENT) perror("remove log_klient");
    if (remove(LOG_FILE_KASA_SAMO) == -1 && errno != ENOENT) perror("remove log_kasa_samo");
    if (remove(LOG_FILE_PRACOWNIK) == -1 && errno != ENOENT) perror("remove log_pracownik");
    if (remove(LOG_FILE_GENERATOR) == -1 && errno != ENOENT) perror("remove log_generator");
    if (remove(LOG_FILE_SYSTEM) == -1 && errno != ENOENT) perror("remove log_system");

    id_sem_cleanup = semget(IPC_PRIVATE, 2, 0600 | IPC_CREAT);
    if (id_sem_cleanup == -1) {
        perror("Blad tworzenia semafora dla watku");
        exit(1);
    }

    inicjalizujSemafor(id_sem_cleanup, 0, 0);
    inicjalizujSemafor(id_sem_cleanup, 1, 0);

    if (pthread_create(&t_cleanup, NULL, watekCzyszczacy, NULL) != 0) {
        perror("Blad tworzenia watku czyszczacego");
        exit(1);
    }

    if (signal(SIGQUIT, Ewakuacja) == SIG_ERR) {
        perror("signal SIGQUIT");
        exit(1);
    }

    struct sigaction sa;
    sa.sa_handler = ObslugaSygnalu;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("sigaction SIGINT");
        exit(1);
    }

    if (signal(SIGUSR1, OtworzKase2) == SIG_ERR) { perror("signal SIGUSR1"); exit(1); }
    if (signal(SIGUSR2, ZamknijKase2) == SIG_ERR) { perror("signal SIGUSR2"); exit(1); }
    if (signal(SIGRTMIN, ZamknijKase1) == SIG_ERR) { perror("signal SIGRTMIN"); exit(1); }
    if (signal(SIGCHLD, obslugaZombie) == SIG_ERR) { perror("signal SIGCHLD"); exit(1); }

    time_t t = time(NULL);
    (void) t;

    FILE *fptr;
    fptr = fopen("/tmp/dyskont_projekt", "w");
    if (fptr != NULL) {
        fclose(fptr);
    }

    key_t klucz = ftok("/tmp/dyskont_projekt", 'S');
    if (klucz == -1) {
        perror("\nblad ftok\n");
        exit(1);
    }

    id_semafora = alokujSemafor(klucz, 4, 0600 | IPC_CREAT);
    inicjalizujSemafor(id_semafora, SEM_KASY, 1);
    inicjalizujSemafor(id_semafora, SEM_UTARG, 1);
    inicjalizujSemafor(id_semafora, SEM_KOLEJKI, 1);
    inicjalizujSemafor(id_semafora, SEM_WEJSCIE, MAX_MIEJSC_W_SKLEPIE);

    id_kolejki = stworzKolejke();

    id_pamieci = utworz_pamiec_dzielona(sizeof(PamiecDzielona));
    sklep = mapuj_pamiec_dzielona(id_pamieci);

    sklep->statystyki.czas_startu = time(NULL);
    sklep->statystyki.ewakuacja = 0;
    sklep->statystyki.liczba_klientow_w_sklepie = 0;
    sklep->statystyki.liczba_obsluzonych_klientow = 0;
    sklep->statystyki.utarg = 0.0;
    sklep->statystyki.liczba_obsluzonych_klientow = 0;
    sklep->statystyki.klienci_w_kolejce_do_stacjonarnej = 0;
    sklep->czy_otwarte = 1;
    sklep->liczba_produktow = 32;

    inicjalizujKolejkeFIFO(&sklep->kolejka_samoobsluga); //<- wspolna kolejka do kas samoobslugowych
    for (int i = 0; i < KASY_STACJONARNE; i++) { inicjalizujKolejkeFIFO(&sklep->kolejka_stato[i]); }
    for (int i = 0; i < KASY_STACJONARNE; i++) //kasy stacjonarne zamkniete na starcie
    {
        sklep->kasa_stato[i].otwarta = 0;
        sklep->kasa_stato[i].zajeta = 0;
        sklep->kasa_stato[i].czas_ostatniej_obslugi = time(NULL);
        sklep->kasa_stato[i].liczba_do_obsluzenia = 0;
    }

    LOG_KIEROWNIK("Uruchomiono %d terminali kas samoobslugowych.\n", KASY_SAMOOBSLUGOWE);
    LOG_KIEROWNIK("Wykladanie towaru na polki...\n");

    for (int i = 0; i < sklep->liczba_produktow; i++) {
        int id_kategoria = i % KATEGORIE;
        int id_konkretnego = (i / KATEGORIE) % 4;
        strcpy(sklep->produkty[i].nazwa, baza_towarow[id_kategoria][id_konkretnego]);
        strcpy(sklep->produkty[i].kategoria, nazwy_kategorii[id_kategoria]);
        sklep->produkty[i].cena = cennik[id_kategoria][id_konkretnego];
        sklep->produkty[i].sztuk = 50;
    }

    LOG_KIEROWNIK("sklep zatowarowany\n");

    pracownik_pid = fork();
    if (pracownik_pid == 0) {
        LOG_KIEROWNIK("pracownik proszony na stanowisko\n");
        if (signal(SIGQUIT, SIG_DFL) == SIG_ERR) { perror("signal SIGQUIT"); exit(1); }
        execlp("./pracownik", "pracownik",NULL);
        perror("execlp pracownik");
        exit(1);
    } else if (pracownik_pid < 0) {
        perror("fork pracownik");
        exit(1);
    }
    SIM_SLEEP_S(1);

    kasjer1_pid = fork();
    if (kasjer1_pid == 0) {
        LOG_KIEROWNIK("Kasjer 1 proszony o gotowosc\n");
        if (signal(SIGQUIT, SIG_DFL) == SIG_ERR) { perror("signal SIGQUIT"); exit(1); }
        execlp("./kasjer", "kasjer", "0", NULL);
        perror("execlp kasjer1");
        exit(1);
    } else if (kasjer1_pid < 0) {
        perror("fork kasjer1");
        exit(1);
    }
    SIM_SLEEP_S(1);

    kasjer2_pid = fork();
    if (kasjer2_pid == 0) {
        LOG_KIEROWNIK("Kasjer 2 proszony o gotowosc\n");
        if (signal(SIGQUIT, SIG_DFL) == SIG_ERR) { perror("signal SIGQUIT"); exit(1); }
        execlp("./kasjer", "kasjer", "1", NULL);
        perror("execlp kasjer2");
        exit(1);
    } else if (kasjer2_pid < 0) {
        perror("fork kasjer2");
        exit(1);
    }
    SIM_SLEEP_S(1);

    for (int i = 0; i < KASY_SAMOOBSLUGOWE; i++) {
        pids_samo[i] = fork();
        if (pids_samo[i] == 0) {
            if (signal(SIGQUIT, SIG_DFL) == SIG_ERR) { perror("signal SIGQUIT"); exit(1); }
            char arg_id[5];
            sprintf(arg_id, "%d", i);
            execlp("./kasy_samo", "kasy_samo", arg_id, NULL);
            perror("Blad execlp kasy_samo");
            exit(1);
        } else if (pids_samo[i] < 0) {
            perror("fork kasa_samo");
            exit(1);
        }
    }
    SIM_SLEEP_S(1);

    // "W pewnym dyskoncie jest łącznie 8 kas: 2 kasy stacjonarne i 6 kas samoobsługowych."
    LOG_KIEROWNIK_BOTH("Sklep otwarty. PID %d\n", getpid());
    LOG_KIEROWNIK("Dostepne polecenia:\n"
    "-> kill -SIGUSR1 %d (Otworz Kase 2)\n"
    "-> kill -SIGUSR2 %d (Zamknij Kase 2)\n"
    "-> kill -%d %d (Zamknij Kase 1)\n"
    "-> kill -SIGQUIT %d (ewakuacja)\n", getpid(), getpid(), SIGRTMIN, getpid(), getpid());

    LOG_KIEROWNIK("----------------------------------------------------------\n");

    while (CzyDziala == 1) {
        if (sklep->czy_otwarte == 0) {
            LOG_KIEROWNIK("Generator zamknal sklep. Rozpoczynam procedure zamykania.");
            CzyDziala = 0;
            break;
        }

        if (waitSemafor(id_semafora, SEM_KASY, 0) == -1) {
            if (CzyDziala == 0) break;
            continue;
        }
        if (CzyDziala == 0) {
            signalSemafor(id_semafora, SEM_KASY);
            break;
        }

        int klienci_total = sklep->statystyki.liczba_klientow_w_sklepie;
        int aktualne_czynne = 0;
        for (int i = 0; i < KASY_SAMOOBSLUGOWE; i++) {
            if (sklep->kasy_samo[i].otwarta) {
                aktualne_czynne += 1;
            }
        }
        signalSemafor(id_semafora, SEM_KASY);

        // "Na każdych K klientów znajdujących się na terenie dyskontu powinno przypadać min. 1 czynne stanowisko kasowe."
        // "Zawsze działają co najmniej 3 kasy samoobsługowe;"
        int wymagane_samo = (klienci_total + K_KLIENTOW_NA_KASE - 1) / K_KLIENTOW_NA_KASE;
        if (wymagane_samo < MIN_CZYNNYCH_KAS_SAMO) wymagane_samo = MIN_CZYNNYCH_KAS_SAMO;
        if (wymagane_samo > KASY_SAMOOBSLUGOWE) wymagane_samo = KASY_SAMOOBSLUGOWE;

        waitSemafor(id_semafora, SEM_KASY, 0);
        if (aktualne_czynne < wymagane_samo) {
            for (int i = 0; i < KASY_SAMOOBSLUGOWE; i++) {
                if (!sklep->kasy_samo[i].otwarta && aktualne_czynne < wymagane_samo) {
                    sklep->kasy_samo[i].otwarta = 1;
                    sklep->kasy_samo[i].zablokowana = 0;
                    sklep->kasy_samo[i].aktualna_kwota = 0.0;
                    aktualne_czynne++;
                    LOG_KIEROWNIK("Otwieram kase samoobslugowa %d (Klienci: %d)\n", i, klienci_total);
                }
            }
        }
        // "Jeśli liczba klientów jest mniejsza niż K*(N-3), gdzie N oznacza liczbę czynnych kas, to jedna z kas zostaje zamknięta;"
        else if (aktualne_czynne > MIN_CZYNNYCH_KAS_SAMO) {
            int prog_zamykania = K_KLIENTOW_NA_KASE * (aktualne_czynne - 3);
            if (klienci_total < prog_zamykania) {
                for (int i = KASY_SAMOOBSLUGOWE - 1; i >= 0; i--) {
                    if (sklep->kasy_samo[i].otwarta && !sklep->kasy_samo[i].zajeta) {
                        sklep->kasy_samo[i].otwarta = 0;
                        LOG_KIEROWNIK("Zamykam kase samoobslugowa %d (Klienci: %d < Prog: %d)\n", i+1, klienci_total,
                                      prog_zamykania);
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
        // "Jeżeli liczba osób stojących w kolejce do kasy jest większa niż 3 otwierana jest kasa 1;"
        {
            LOG_KIEROWNIK_BOTH("Kolejka do kasy nr1: %d. Otwieram kase...\n", kolejka_do_k1);
            sklep->kasa_stato[0].otwarta = 1;
            sklep->kasa_stato[0].zamykanie_w_toku = 0;
            sklep->kasa_stato[0].liczba_do_obsluzenia = 0;
            kill(kasjer1_pid, SIGUSR1);
        }
        signalSemafor(id_semafora, SEM_KASY);

        for (int j = 0; j < KASY_STACJONARNE; j++) {
            if (sklep->kasa_stato[j].otwarta == 1) {
                waitSemafor(id_semafora, SEM_KOLEJKI, 0);
                int pusta = (sklep->kolejka_stato[j].rozmiar == 0);
                signalSemafor(id_semafora, SEM_KOLEJKI);


                if (pusta && sklep->kasa_stato[j].zamykanie_w_toku == 0) {
                    double seconds = difftime(time(NULL), sklep->kasa_stato[j].czas_ostatniej_obslugi);
                    // "Jeżeli po obsłużeniu ostatniego klienta w kolejce przez 30 sekund nie pojawi się następny klient, kasa jest zamykana;"
                    if (seconds > ZAMKNIJ_KASE_PO || sklep->kasa_stato[j].zamykanie_w_toku == 1) {
                        LOG_KIEROWNIK_BOTH("Zamykam kase stacjonarna %d\n", j+1);
                        waitSemafor(id_semafora, SEM_KASY, 0);
                        sklep->kasa_stato[j].otwarta = 0;
                        sklep->kasa_stato[j].zamykanie_w_toku = 0;
                        pid_t pid_j = (j == 0) ? kasjer1_pid : kasjer2_pid;
                        kill(pid_j, SIGUSR2);
                        signalSemafor(id_semafora, SEM_KASY);
                    } else {
                        waitSemafor(id_semafora, SEM_KASY, 0);
                        if (sklep->kasa_stato[j].zajeta == 1) {
                            sklep->kasa_stato[j].czas_ostatniej_obslugi = time(NULL);
                        }
                        signalSemafor(id_semafora, SEM_KASY);
                    }
                }
            }
        }
        SIM_SLEEP_S(1);
    }

    waitSemafor(id_semafora, SEM_KASY, 0);
    sklep->czy_otwarte = 0;
    int ile_klientow = sklep->statystyki.liczba_klientow_w_sklepie;
    signalSemafor(id_semafora, SEM_KASY);

    if (tryb_ewakuacji) {
        LOG_KIEROWNIK("EWAKUACJA - natychmiastowe zamkniecie!");
        goto cleanup_and_exit;
    }

    if (ile_klientow <= 0) {
        LOG_KIEROWNIK("Brak klientow. Koniec pracy.");
        goto cleanup_and_exit;
    }

    LOG_KIEROWNIK("Czekam na ostatnich klientow (%d)...", ile_klientow);
    signal(SIGCHLD, SIG_DFL);

    //oczekiwanie na klientow konczacych swoje dzialanie
    while (1) {
        waitSemafor(id_semafora, SEM_KASY, 0);
        int ile = sklep->statystyki.liczba_klientow_w_sklepie;
        signalSemafor(id_semafora, SEM_KASY);

        if (ile <= 0) {
            LOG_KIEROWNIK("Sklep pusty. Wszyscy wyszli.");
            break;
        }

        waitSemafor(id_semafora, SEM_KOLEJKI, 0);
        int k1_size = sklep->kolejka_stato[0].rozmiar;
        int k2_size = sklep->kolejka_stato[1].rozmiar;
        signalSemafor(id_semafora, SEM_KOLEJKI);

        // Rozwiazanie dla klientow stojacych w kolejce do stacjo (1 lub 2) gdy zostalo za wczesniej wywołane SIGINT
        waitSemafor(id_semafora, SEM_KASY, 0);
        if (k1_size > 0) {
            LOG_KIEROWNIK("Zamykanie: Zostalo %d osob w kolejce do K1. Wymuszam otwarcie!", k1_size);
            sklep->kasa_stato[0].otwarta = 1;
            sklep->kasa_stato[0].zamykanie_w_toku = 0;
            sklep->kasa_stato[0].czas_ostatniej_obslugi = time(NULL);
            kill(kasjer1_pid, SIGUSR1);
        }
        if (k2_size > 0 && !sklep->kasa_stato[1].otwarta) {
            sklep->kasa_stato[1].otwarta = 1;
            sklep->kasa_stato[1].zamykanie_w_toku = 0;
            sklep->kasa_stato[1].czas_ostatniej_obslugi = time(NULL);
            kill(kasjer2_pid, SIGUSR1);
        }
        signalSemafor(id_semafora, SEM_KASY);

        SIM_SLEEP_S(1);
    }
cleanup_and_exit:
    LOG_KIEROWNIK("Rozpoczynam finalne sprzatanie...");

    struct sembuf op_signal = {0, 1, 0};
    semop(id_sem_cleanup, &op_signal, 1);

    struct sembuf op_wait = {1, -1, 0};
    while (semop(id_sem_cleanup, &op_wait, 1) == -1) {
        if (errno == EINTR) continue;
        perror("semop cleanup wait");
        break;
    }

    pthread_join(t_cleanup, NULL);

    if (id_sem_cleanup != -1) {
        if (semctl(id_sem_cleanup, 0, IPC_RMID) == -1) {
            perror("semctl cleanup IPC_RMID");
        }
        id_sem_cleanup = -1;
    }

    LOG_KIEROWNIK("Zamkniecie zakonczone pomyslnie.");
    return 0;
}
