//
// Created by kamil-klimas on 11.12.2025.
//

#include "ipc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/shm.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <sched.h>



key_t utworz_klucz(char id)
{
    key_t klucz = ftok(FTOK_PATH, id);
    if (klucz == -1)
    {
        perror("ftok");
        exit(EXIT_FAILURE);
    }
    return klucz;
}


//FUNKCJE PAMIECI DZIELONEJ
int utworz_pamiec_dzielona(size_t rozmiar)
{
    key_t klucz = utworz_klucz('S');
    int shmid = shmget(klucz, rozmiar, IPC_CREAT | 0600); // zmien potem uprawnienia na 0600 (własciciel R+W)
    if (shmid == -1)
    {
        perror("shmget");
        exit(EXIT_FAILURE);
    }
    return shmid;
}

key_t podlacz_pamiec_dzielona()
{
    key_t klucz = utworz_klucz('S');
    int shmid = shmget(klucz, 0,0);
    if (shmid == -1)
    {
        perror("shmget");
        exit(EXIT_FAILURE);
    }
    return shmid;
}

PamiecDzielona* mapuj_pamiec_dzielona(int shmid)
{
    void *ptr = shmat(shmid, NULL, 0);
    if (ptr == (void *)-1)
    {
        perror("shmat");
        exit(EXIT_FAILURE);
    }
    return (PamiecDzielona*)ptr;
}

void odlacz_pamiec_dzielona(PamiecDzielona*shm)
{
    if (shmdt(shm) == -1)
    {
        perror("shmdt");
    }
}

void usun_pamiec_dzielona(int shmid)
{
    if (shmctl(shmid, IPC_RMID, 0) == -1) {perror("shmctl");}
}

//SEMAFORY
int alokujSemafor(key_t klucz, int number, int flagi)
{
    int semID;
    if ((semID = semget(klucz,number,flagi)) == -1)
    {
        perror("Blad semget(alokujSemfor): ");
        exit(1);
    }

    return semID;
}

void inicjalizujSemafor(int semID, int number, int val)
{
    if (semctl(semID, number, SETVAL, val) ==-1)
    {
        perror("Blad semctl(inicjalizujSemafor): ");
        exit(1);
    }
}

int zwolnijSemafor(int semID, int number)
{
    return semctl(semID, number, IPC_RMID);
}

int waitSemafor(int semID, int number, int flagi)
{
    struct sembuf operacje[1];
    operacje[0].sem_num = number;
    operacje[0].sem_op = -1;
    operacje[0].sem_flg = flagi;
    while (semop(semID, operacje, 1) == -1) {
        if (errno == EINTR) {
            continue;
        }
        // EIDRM - semafor został usunięty przez Kierownika
        // EINVAL - semafor nie istnieje (już usunięty)
        if (errno == EIDRM || errno == EINVAL) {
            exit(0);
        }

        perror("waitSemafor error");
        exit(1);
    }

    return 1;
}

void signalSemafor(int semID, int number)
{
    struct sembuf operacje[1];
    operacje[0].sem_num = number;
    operacje[0].sem_op = 1;
    operacje[0].sem_flg = 0;
    if (semop(semID, operacje, 1) == -1)
    {
        while (semop(semID, operacje, 1) == -1) {
            if (errno == EINTR) {
                continue;
            }

            // EIDRM - semafor został usunięty przez Kierownika
            // EINVAL - semafor nie istnieje (już usunięty)
            if (errno == EIDRM || errno == EINVAL) {
                exit(0);
            }

            perror("waitSemafor error");
            exit(1);
        }
    }
}

int valueSemafor(int semID, int number)
{
    return semctl(semID, number, GETVAL, NULL);
}

//KOLEJKI KOMUNIKATOW
int stworzKolejke()
{
    key_t klucz = utworz_klucz('S');
    int msgid = msgget(klucz, IPC_CREAT | 0600); // zmien potem na minimalne uprawnienia
    if (msgid == -1)
    {
        perror("msgget");
        exit(EXIT_FAILURE);
    }
    return msgid;
}

int WyslijDoKolejki(int msgid, struct messg_buffer *msg)
{
    size_t rozmiar = sizeof(struct messg_buffer) - sizeof(long);

    while (msgsnd(msgid, msg, rozmiar, 0) == -1) {
        if (errno == EINTR) continue;
        perror("msgsnd");
        exit(EXIT_FAILURE);
    }
    return 0;
}

int OdbierzZKolejki(int msgid, struct messg_buffer *msg,  long typ_adresata)
{
    size_t rozmiar = sizeof(struct messg_buffer) - sizeof(long);
    int odebrana;

    while ((odebrana = msgrcv(msgid, msg, rozmiar, typ_adresata, 0)) == -1) {
        if (errno == EINTR) {
            continue;
        }
        if (errno == EIDRM || errno == EINVAL) return -1;

        perror("msgrcv");
        return -1;
    }
    return odebrana;
}

void usun_kolejke(int msgid)
{
    if (msgctl(msgid, IPC_RMID, NULL) == -1)
    {
        perror("Błąd podczas usuwania kolejki komunikatów (msgctl)");
    }
}

int BezpieczneWyslanieKlienta(int msgid, struct messg_buffer *msg)
{
    size_t rozmiar_msg = sizeof(struct messg_buffer) - sizeof(long);
    struct msqid_ds buf;

    while(1) {
        if (msgctl(msgid, IPC_STAT, &buf) == -1) {
            perror("msgctl check");
            return -1;
        }

        if (buf.msg_cbytes + rozmiar_msg < buf.msg_qbytes - MARGINES_KOLEJKI_BAJTY) {
            if (msgsnd(msgid, msg, rozmiar_msg, IPC_NOWAIT) == -1) {
                if (errno == EAGAIN) {
                    SIM_SLEEP_US(1000);
                    continue;
                }
                if (errno == EINTR) continue;
                perror("Bezpieczne msgsnd");
                return -1;
            }
            return 0;
        } else {
            SIM_SLEEP_US(50000);
        }
    }
}

void inicjalizujKolejkeFIFO(KolejkaKlientow *k)
{
    k->poczatek = 0;
    k->koniec = 0;
    k->rozmiar = 0;
}

int dodajDoKolejkiFIFO(KolejkaKlientow *k, pid_t pid)
{
    if (k->rozmiar >= MAX_DLUGOSC_KOLEJKI)
    {
        return -1;
    }

    k->klienci[k->koniec].pid = pid;
    k->klienci[k->koniec].czas_wejscia = time(NULL);
    k->koniec = (k->koniec + 1) % MAX_DLUGOSC_KOLEJKI;
    k->rozmiar +=1;

    return 0;
}

pid_t zdejmijZKolejkiFIFO(KolejkaKlientow *k)
{
    if (k->rozmiar == 0) return 0;
    pid_t pid = k->klienci[k->poczatek].pid;
    k->poczatek = (k->poczatek +1) % MAX_DLUGOSC_KOLEJKI;
    k->rozmiar -=1;
    return pid;
}

pid_t podejrzyjPierwszegoFIFO(KolejkaKlientow *k)
{
    if (k->rozmiar == 0) return 0;
    return k->klienci[k->poczatek].pid;
}

int usunZSrodkaKolejkiFIFO(KolejkaKlientow *k, pid_t pid)
{
  if (k->rozmiar == 0) return 0;
    int idx = -1;
    int aktualny = k->poczatek;
    for (int i = 0; i < k->rozmiar; i++)
    {
        if (k->klienci[aktualny].pid == pid)
        {
            idx = aktualny;
            break;
        }
        aktualny = (aktualny + 1) % MAX_DLUGOSC_KOLEJKI;
    }
    if (idx == -1) return 0;

    if (idx == k->poczatek)
    {
        zdejmijZKolejkiFIFO(k);
        return 1;
    }

    int iterator = idx;

    while (iterator != (k->koniec - 1 + MAX_DLUGOSC_KOLEJKI) % MAX_DLUGOSC_KOLEJKI)
    {
        int next = (iterator + 1) % MAX_DLUGOSC_KOLEJKI;
        k->klienci[iterator] = k->klienci[next];
        iterator = next;
    }

    k->koniec = (k->koniec - 1 + MAX_DLUGOSC_KOLEJKI) % MAX_DLUGOSC_KOLEJKI;
    k->rozmiar -= 1;

    return 1;
}

//Handler wejścia standardowego
int inputExceptionHandler(const char * komunikat)
{
    int wartosc = 0;
    int c; // do czyszczenia bufora

    while (1) {
        printf(ANSI_BOLD ANSI_CYAN "%s" ANSI_RESET "\n > ", komunikat);
        if (scanf("%d", &wartosc) == 1) {
            if (wartosc > 0) {
                return wartosc;
            } else {
                printf(ANSI_RED "[BLAD] Liczba musi byc wieksza od 0!\n" ANSI_RESET);
            }
        } else {
            printf(ANSI_RED "[BLAD] To nie jest liczba!\n" ANSI_RESET);

            while ((c = getchar()) != '\n' && c != EOF);
        }
    }
}