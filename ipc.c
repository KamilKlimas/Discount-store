//
// Created by kamil-klimas on 11.12.2025.
//

#include "ipc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


key_t utworz_klucz(char id)
{
    key_T klucz = ftok(FTOK_PATH, id);
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
        perror("shmget");e
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
    if (shmctl(shmid, IPC_RMID, 0) == -1) : perror("shmctl");
}