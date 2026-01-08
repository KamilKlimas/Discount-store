
  
# Projekt Systemy Operacyjne 2025/2026 - Temat 16: Dyskont  
  
    
  
**Autor:** Kamil Klimas  
  
**Numer albumu:** 155188  
  
**Grupa laboratoryjna:** GP02  
  
**Repozytorium GitHub:** https://github.com/KamilKlimas/Discount-store  
  
    
---  
  
    
## 1. Opis zadania (Temat 16 - Dyskont)
  
    
Projekt stanowi symulację działania dyskontu spożywczego w systemie Linux, wykorzystującą mechanizmy **IPC Systemu** (*pamięć dzieloną, semafory, kolejki komunikatów*) oraz procesy potomne. Symulacja uwzględnia dynamiczne zarządzanie kasami, zachowanie klientów, weryfikacje wieku w przypadku zakópów zawierających alkohol oraz interwencje obsługi.

---
## 2. Zalożenia projektu:
**- Architektura wieloprocesowa:**  Każdy element symulacji (Klient, Kasjer, Kierownik, Pracownik) jest niezależnym procesem potomnym tworzonym przez ```fork()```.
**- IPC Systemu:** Komunikacja odbywa się poprzez *pamięć dzieloną* (wspólny stan sklepu) oraz *kolejki komunikatów* (bezpośrednia wymiana danych między procesami).
**-Synchronizacja:** Dostęp do zasobów wspóldzielonych i sekcji krytycznych jest chroniony zestawem *semaforów*, co zapobiega błędom oraz wyscigom.
**-Dynamika systemu:** Liczba otwartych kas jest dynamicznie skalowana względem natężenia ruchu, a klienci podejmują autonomiczne decyzje (np. zmiana kolejki).

---
## 3.Opis struktury kodu:
**- ```kierownik.c```:**  Główny proces zarządczy. Inicjalizuje środowisko IPC (*SHM, SEM, MSQ*) i powołuje personel. W pętli głównej realizuje algorytm skalowania kas samoobsługowych ( *K*(N-3*) ) oraz monitoruje długość kolejek stacjonarnych, decydując samodzielnie o otwarciu kasy 1. Obsługuje sygnały sterujące (*```SIGUSR1/2```,```SIGQUIT```*) i odpowiada za sprzątanie zasobów (```cleanUp()```).

**-```generator.c```:** Symulator natężenia ruchu. Odpowiada za cykliczne tworzenie procesów dla ```klient.c``` (```fork()``` + ```execlp()```) w losowych odstępach stymulowanych przez "fale natężenia klientów".

**-```klient.c```:** Logika kupującego. Losuje koszyk produktów, wybiera kolejkę (FIFO lub Stacjonarna) i realizuje zakupy. Obsługuje mechanizm *Queue Jumping* (aktywne monitorowanie sąsiedniej kolejki), komunikacją z kasjerem (wymiana wiadomości) oraz procedurę zwrotu towaru w przypadku odmowy sprzedaży alkoholu.

**-```kasjer.c```:** Obsługa kasy stacjonarnej. Nasłuchuje na komunikaty od klientów (paragon), symuluje kasowanie (```sleep()```) i weryfikuje wiek (odsyła kod błędu *-2.0* dla klientów niepełnoletnich). Implementuje logike oszczędzania zasobów (usypianie po 30 sekundach po obsłużeniu ostatniego klienta).

**-```pracownik.c```:** Obsługa mobilna strefy samoobsługowej. Monitoruje flagi w pamięci dzielonej: usuwa awarie techniczne blokujące kasy oraz weryfikuje wiek klientów kupujących alkohol (zatwierdzenie lub odrzucenie transakcji). 

**-```kasy_samo.c```:** Proces reprezentujący terminal samoobsługowy. Utrzymuje status urządzenia (*otwarta/zamknięta/zablokowana*) i przechowuje kwotę bieżącej transakcji w pamięci dzielonej.

**-```ipc.c``` / ```ipc.h```:** Biblioteka pomocnicza IPC. Zawiera definicje kluczowych struktur (```PamiecDzielona```, ```KolejkaFIFO```, ```messg_buffer```) oraz bezpieczne wrappery na funkcje systemowe (```semop```, ```shmget```, ```msgrcv```). 

---
## 4. Opis implementacji i wybranych algorytmów:
### 4.1 Mechanizmy synchronizacji i IPC
**- Pamieć dzielona (```shmget```/ ```shmat```):**
	Obiekt `PamiecDzielona` jest mapowany przez każdy proces. Zawiera:
* Tablice struktur `KasaSamo` (statusy, blokady, kwoty).
* Struktury `KolekaFIFO` (cykliczne bufory PID-ów oczekujących klientów).
* Statystyki globalne (liczba klientów w sklepie, utarg).

**- Semafory (`semop`):**
Zestaw 3 semaforów kontroluje dostęp do zasobów, zapobiegając wyścigom (race Conditions):
* `SEM_KASY` (0): Chroni modyfikację statusu kas (otwieranie/zamykanie/blokada).
* `SEM_UTARG` (1): Chroni zmienną dziennego utargu sklepu.
* `SEM_KOLEJKI` (2): Chroni intergralność tablic FIFO podczas dodawania/usuwania klientów.

**- Kolejki komunikatów (`msgrcv`/`msgsnd`):**
Slużą do adresowania wiadomości do konkretnych procesów:
* **Typ wiadomości = PID Klienta:** Kasjer wysyła zaproszenie do konkretnej osoby.
*  **Typ wiadomosci = ID Kasjera + OFFSET:** Klient odsyła listę zakupów do konkretnego kasjera.

### 4.2 Algorytm Klienta:
**- Aktywne oczekiwanie i zmiana kolejki:**
Klient w kasie stacjonarnej nie blokuje się na stałe (`msgrcv` bez flagi), lecz stosuje *aktywne oczekiwanie* dzięki fladze `IPC_NOWAIT`. Pozwala to na monitorowanie otoczenia i zmianę kolejki, jeśli sąsiednia kasa jest  pusta.
 * Logika algorytmu:
 * 1. Sprawdź czy kasjer mnie woła.
 * 2. Jeśli nie -> sprawdź czy sąsiad jest otwarty i pusty.
 * 3. Jeśli tak -> przepisz się .
 * 4. Jeśli nie -> zaśnij na 200ms i powtórz.
<img src="https://i.imgur.com/GftgEcC.png" height="110%">

**-Dynamiczne skalowanie kas:**
Kierownik w pętli głównej monitoruje liczbę klientów w sklepie i dostosowuje liczbę otwartych kas samoobsługowych zgodnie z ustalonymi założeniami.
* Zasady działania:
* 1. **Otwieranie:** Jeśli ```aktualne_kasy < (klienci / K_KLIENTOW_NA_KASE)```
* 2. **Zamykanie:** Jeśli `klienci < K_KLIENTOW_NA_KASE * (aktualne_kasy - 3)`
* 3. **Warunek brzegowy:** Zawsze muszą działać minimum 3 kasy.
<img src="https://i.imgur.com/GftgEcC.png" width="100%" height="110%">

### 4.3 Protokół weryfikacji wieku:
W przypadku zakupu alkoholu, proces klienta i pracownika/kasjera muszą sie zsynchronizować aby podjąć decyzję o sprzedaży.
* Strona Klienta:
```C
IF (koszyk zawiera alkohol) TO:
    SEKCJA_KRYTYCZNA(SEM_KASY):
        kasa[nr].zablokowana = 1   // Blokada kasy
        kasa[nr].alkohol = 1       // Flaga: "Proszę o weryfikację"
        kasa[nr].wiek = moj_wiek   // Okazanie dowodu
    KONIEC_SEKCJI

    // Oczekiwanie na Pracownika
    WHILE (kasa[nr].zablokowana == 1):
        Sleep(100ms)

    // Odczyt decyzji (już po odblokowaniu)
    status = kasa[nr].alkohol
    
    IF (status == -1) TO: // Decyzja: ODMOWA
        DLA KAŻDEGO produktu w koszyku:
            IF (produkt to alkohol) TO:
                Magazyn[produkt] += 1      // Fizyczny zwrot na półkę
                Rachunek -= cena_produktu  // Korekta ceny
        Zapłać(Rachunek)
    ELSE IF(status == 2): // Decyzja: ZGODA
        Zapłać(Pelna_Kwota)
 ```
 * Strona pracownika:
 ```C
 DLA KAŻDEJ kasy samoobslugowej J:
    SEKCJA_KRYTYCZNA(SEM_KASY):
        // Sprawdź czy kasa wymaga interwencji
        IF (kasa[J].zablokowana == 1 ORAZ kasa[J].alkohol == 1) TO:
            wiek_klienta = kasa[J].wiek
            
            IF (wiek_klienta < 18) TO:
                LOG("Klient nieletni! Odrzucam transakcję.")
                kasa[J].alkohol = -1 // Kod błędu (Odrzucenie)
            ELSE:
                LOG("Klient pełnoletni. Zatwierdzam.")
                kasa[J].alkohol = 2  // Kod sukcesu (Zatwierdzenie)
            
            //oddanie sterowania klientowi
            kasa[J].zablokowana = 0 
    KONIEC_SEKCJI
 ```
 * Strona kasjera:
 ```c
// Kasjer czeka na klienta
OdbierzWiadomosc(TYP = KANAL_KASJERA)

// Weryfikacja po otrzymaniu danych
IF (msg.ma_alkohol == 1 ORAZ msg.wiek < 18) TO:
    LOG("Klient nieletni! Odmowa sprzedaży.")
    
    // Krok 1: Odesłanie kodu błędu -2.0 (Nakaz zwrotu)
    msg.kwota = -2.0
    WyslijWiadomosc(DO = PID_Klienta, msg)

    // Krok 2: Oczekiwanie na skorygowany rachunek (bez alkoholu)
    OdbierzWiadomosc(TYP = KANAL_KASJERA)
    LOG("Przyjęto poprawioną kwotę: " + msg.kwota)

ELSE IF (msg.ma_alkohol == 1):
    LOG("Weryfikacja pomyślna. Sprzedaję alkohol.")

// Finalizacja transakcji
SymulujSkanowanie(2 sekundy)
ZaktualizujUtarg(msg.kwota)
WyslijPotwierdzenie(DO = PID_Klienta, Kod = -1.0)
```
## 5. Rzeczy, które okazały się problematyczne i ich rozwiązania:
**- Wyścig danych przy zmianie kolejki:**
* *Problem*: Klienci mogą w dowolnym momencie zdecydować o przejściu do innej kolejki. Jeśli dwóch klientów jednocześnie próbowałoby zmodyfikować tablicę kolejki (np. jeden dochodzi, drugi odchodzi ze środka), doszłoby do uszkodzenia struktur danych (nadpisania PID-ów).

* *Rozwiązanie*: Wprowadziłem dedykowany semafor `SEM_KOLEJKI`. Każda operacja na tablicach FIFO odbywa się w sekcji krytycznej. Dodatkowo klient używa `msgrcv` z flagą `IPC_NOWAIT`, co pozwala mu aktywnie czekać i sprawdzać stan sąsiednich kolejek bez blokowania procesu.

**-Celowane dostarczanie wiadomości:**
* *Problem*: W systemie istnieje tylko jedna główna kolejka komunikatów, a klientów jest wielu. Pojawia się więc pytanie "Jak sprawić, by paragon trafił do właściwego klienta, a nie został odebrany przez kogoś innego"?

* *Rozwiązanie*: Wykorzystano pole mtype w strukturze komunikatu.
Zaproszenie: Kasjer wysyła wiadomość o typie równym PID klienta.
Paragon: Klient wysyła listę zakupów o typie ID_Kasjera + OFFSET. Dzięki temu każdy proces wyciąga z kolejki tylko te komunikaty, które są przeznaczone konkretnie dla niego.
