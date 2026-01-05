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

/*
 *msgflg – flagi specyfikujące zachowanie się funkcji w warunkach nietypowych Wartość ta
 *może być ustawiona na 0 lub IPC_NOWAIT (jeśli kolejka komunikatów jest pełna wtedy wiadomość nie jest
 *zapisywana do kolejki, a sterowanie wraca do procesu. Gdyby flaga nie była ustawiona, proces jest wstrzymywany tak
 *długo, aż zapis wiadomości nie będzie możliwy)
 */

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
    int shmid = shmget(klucz, rozmiar, IPC_CREAT | 0666); // zmien potem uprawnienia na 0600 (własciciel R+W)
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
    //union semun arg;
    //arg.val = val;
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
    operacje[0].sem_flg = flagi; //sem_undo
    //printf(" -> [PID %d] ZABLOKOWALEM semafor nr %d\n z id %d", getpid(), number, semID);
    if (semop(semID, operacje, 1) == -1)
    {
        perror("semop(waitSemafor): ");
        exit(0);
    }

    return 1;
}
//operacja V (oddaj/podnies)
void signalSemafor(int semID, int number)
{
    struct sembuf operacje[1]; //kelner a bardziej formularz do niego
    operacje[0].sem_num = number; //numer stolika - czyli ktory semafor
    operacje[0].sem_op = 1; //co podac? -1 = zabloku klucz +1 = oddaj klucz
    operacje[0].sem_flg = 0; //wczesniej SEM_UNDO wykrzaczało klienta bo usuwalo ostatni signal (inreverse)
    if (semop(semID, operacje, 1) == -1)
    {
        perror("semop(postSemafor): ");
        exit(1);
    }
    //printf(" <- [PID %d] ZWALNIAM semafor nr %d\n", getpid(), number);
}

int valueSemafor(int semID, int number)
{
    return semctl(semID, number, GETVAL, NULL);
}

//KOLEJKI KOMUNIKATOW
int stworzKolejke()
{
    key_t klucz = utworz_klucz('S');
    int msgid = msgget(klucz, IPC_CREAT | 0666); // zmien potem na minimalne uprawnienia
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
    int wiadomosc = msgsnd(msgid, msg, rozmiar, 0);
    if (wiadomosc == -1)
    {
        perror("msgsnd");
        exit(EXIT_FAILURE);
    }
    return wiadomosc;
}

int OdbierzZKolejki(int msgid, struct messg_buffer *msg,  long typ_adresata)
{
    size_t rozmiar = sizeof(struct messg_buffer) - sizeof(long);
    int odebrana = msgrcv(msgid, msg, rozmiar, typ_adresata,0);
    if (odebrana == -1)
    {
        perror("msgrcv");
        exit(EXIT_FAILURE);
    }
    return odebrana;
}//

void usun_kolejke(int msgid)
{
    if (msgctl(msgid, IPC_RMID, NULL) == -1)
    {
        perror("Błąd podczas usuwania kolejki komunikatów (msgctl)");
        // 0 - sukces, -1 - blad
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

    k->klienci[idx].pid = 0;

    return 1;
}
///