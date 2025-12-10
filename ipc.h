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

//Global constants - stałe systemu
#define MAX_KLIENTOW 100
#define MAX_PRODUKTOW 50
#define KASY_SAMOOBSLUGOWE 6
#define KASY_STACJONARNE 2
#define MIN_PRODUKTOW_KOSZYK 3
#define MAX_PRODUKTOW_KOSZYK 10
#define SZANSA_SAMOOBSLUGA 95

//Dynamic checkout opening menagment - dynamiczne otwieranie kas
#define K_KLIENTOW_NA_KASE 10 //For K customers 1 checkout - co K klientów 1 kasa
#define MIN_CZYNNYCH_KAS_SAMO 3

//Staffed checkout - kasa stacjonarna
#define OTWORZ_KASE_1_PRZY 3 // >=3 customers in queue - >=3 klientow w kolejce
#define ZAMKNIJ_KASE_PO 30 // 30 sec waittime without customers - 30 sekund oczekiwania brak klientow

//strukt na typ produktu

//strukt zakupy klienta

//strukt klient w kolejce

//FIFO

//strukt kasa samoobslugowa

//strukt kasa stacjonarna

//FUNKCJE IPC!!!