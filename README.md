
  
# Projekt Systemy Operacyjne 2025/2026 - Temat 16: Dyskont  
  
    
  
**Autor:** Kamil Klimas  
  
**Numer albumu:** 155188  
  
**Grupa laboratoryjna:** GP02  
  
**Repozytorium GitHub:** https://github.com/KamilKlimas/Discount-store  
  
    
---  
  
    
## 1. Opis zadania, oraz uruchomienie:
  
 ### 1. Opis zadania:
Projekt stanowi symulację działania dyskontu spożywczego w systemie Linux, wykorzystującą mechanizmy **IPC Systemu** (*pamięć dzieloną, semafory, kolejki komunikatów*) oraz procesy potomne. Symulacja uwzględnia dynamiczne zarządzanie kasami, zachowanie klientów, weryfikacje wieku w przypadku zakópów zawierających alkohol oraz interwencje obsługi.

### 2. Uruchomienie dyskontu:
```bash
make
chmod +x start_symulacji.sh

./start_symulacji.sh
```

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
 * 1. Start/Sprawdzenie `msgrcv` (`IPC_NOWAIT`)
   * Odebrano wiadomość? -> Tak: Przejdź do płatności
   * Brak wiadomości? -> Zablokuj `SEM_KOLEJKI`
 * 2. Czy zmieniać kolejke? (`sąsiad == pusty && sąsiad == otwarty`)
   * Tak jeśli:
      * W bieżącej kolejce jest za dużo klientów
      * Bieżaca kasa jest zamknięta (*rozwiązanie problemu pułapki przedwczesnego otworzenia drugiej kasy, gdy 1 nie zdążyła się jeszcze uruchomić*)  
 * 3. Jeśli nie: -> `usunZSrodkaFIFO()` (obecna) -> `dodajDoFIFO` (sąsiednia) -> Odblokuj `SEM_KOLEJKI` 
 * 4. Jeśli nie -> Odblokuj `SEM_KOLEJKI`.
 *  5. Czekaj 200ms -> wróć do punktu 1.
<img src="https://raw.githubusercontent.com/KamilKlimas/Discount-store/refs/heads/main/img/schemat1logika.png" width="100%" height="110%">

**-Dynamiczne skalowanie kas:**
Kierownik w pętli głównej monitoruje liczbę klientów w sklepie i dostosowuje liczbę otwartych kas samoobsługowych zgodnie z ustalonymi założeniami.
* Zasady działania:
* 1. **Otwieranie:** Jeśli ```aktualne_kasy < (klienci / K_KLIENTOW_NA_KASE)```
* 2. **Zamykanie:** Jeśli `klienci < K_KLIENTOW_NA_KASE * (aktualne_kasy - 3)`
* 3. **Warunek brzegowy:** Zawsze muszą działać minimum 3 kasy.
<img src="https://raw.githubusercontent.com/KamilKlimas/Discount-store/refs/heads/main/img/schemat2.png" width="100%" height="110%">

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

## 6. Elementy wyróżniające:

**- Automatyczne uruchomienie symulacji:** Projekt wykorzystuje *skrypt Bash* aranżując sesję terminala `tmux`. Dzieli on automatycznie okno terminala na 4 niezależne panele uruchamiając w nich: proces `kierownik.c`, monitoring zasobów IPC w czasie rzeczywistym (`watch ipcs`) oraz dwie puste konsole (jedna do uruchomienia `generator.c`/`klient.c`, a druga do wysyłania sygnałów).

**- Wizualna segregacja procesów:** Wykorzystano funkcję `printf` do formatowania wyjścia standardowego (`stdout`) przy użyciu kodów ANSI.

## 7. Testy:
* **Test 1:**  Protokół odmowy sprzedaży (scenariusz dla osoby niepełnoletniej `wiek < 18`)
  * Oczekiwany rezultat: `kasjer.c`/`pracownik.c` loguje komunikat: `NIELETNI! Zabieram alkohol z kasy "X" i odkładam na półkę`. Proces sprawdzający nie kończy transakcji, lecz odsyła kod błędu (`-2.0`). Klient wtedy loguje: `Odmowa! Oddaje alkohol, kupuję resztę.`. Następna próba płatności z kwotą odliczoną o koszt alkoholu kończy się sukcesem.
  * Wynik: Sukces.
<table>
  <tr>
    <td width="50%" align="center">
      <img src="https://raw.githubusercontent.com/KamilKlimas/Discount-store/refs/heads/main/img/test1klient.png" width="100%">
    </td>
    <td width="50%" align="center">
      <img src="https://raw.githubusercontent.com/KamilKlimas/Discount-store/refs/heads/main/img/test1kierownik.png" width="100%">
    </td>
  </tr>
</table>

* **Test 2:** Dynamiczne skalowanie (scenariusz dla nagłego skoku ilości w sklepie)
  * Oczekiwany rezultat: `kierownik.c` wykrywa obciążenie i loguje: `Liczba klientow > Prog. Otwieram Kase Samoobsługową nr "X"`. Nowa kasa zmienia status w pamięci dzielonej. Gdy liczba klientów spadnie, `kierownik.c` loguje: `Zamykam Kase Samoobsługową nr "X"`, tym samym oszczędzając zasoby.
  * Wynik: Sukces.
  
<table>
  <tr>
    <td align="center">
      <img src="https://raw.githubusercontent.com/KamilKlimas/Discount-store/refs/heads/main/img/test2otwieranieKas.png" width="100%">
    </td>
  </tr>
  <tr>
    <td align="center">
      <img src="https://raw.githubusercontent.com/KamilKlimas/Discount-store/refs/heads/main/img/test2zamykanieKas.png" width="100%">
    </td>
  </tr>
</table>

* **Test 3:** Aktywne oczekiwanie (scenariusz dla dyskontu po użyciu `SIGUSR1`)
  * Oczekiwany rezultat: Kierownik wysyłając sygnał otwarcia dodatkowej kasy (`kill -SIGUSR1 "PID"`), otwiera drugą kasę. Klienci wtedy stojący w kolejce do kasy X wykrywają zmiane statusu sąsiedniej kasy i jeśli zdecydują, że to słuszne (`kasaY.otwarta == 1 && kasaY.pusta == 1`) logują `Przechodzę do kasy Y`, a KasjerY ma ich poprawnie obsłużyć.
  * Wynik: Sukces.
<table>
  <tr>
    <td width="50%" align="center">
      <img src="https://raw.githubusercontent.com/KamilKlimas/Discount-store/refs/heads/main/img/test3SygnalOtwieranieKasy2.png" width="100%">
    </td>
    <td width="50%" align="center">
      <img src="https://raw.githubusercontent.com/KamilKlimas/Discount-store/refs/heads/main/img/test3KierownikOtwieranieKasy2.png" width="100%">
    </td>
  </tr>
  <tr>
    <td width="50%" align="center">
      <img src="https://raw.githubusercontent.com/KamilKlimas/Discount-store/refs/heads/main/img/test3zmianaKlienta4306.png" width="100%">
    </td>
    <td width="50%" align="center">
      <img src="https://raw.githubusercontent.com/KamilKlimas/Discount-store/refs/heads/main/img/test3KasjerdlaKlienta4306.png" width="100%">
    </td>
  </tr>
</table>

* **Test 4:** Bezpieczne zamknięcie (scenariusz dla sytuacji, gdy kasa dostaje polecenie `SIGUSR2` ale w kolejce czekają jeszcze klienci do obsłużenia)
  * Oczekiwany rezultat: Po ustawieniu sie kilku klientow w kasie stacjonarnej dostaje ona sygnał o zakończeniu swojego działania. Oczekiwanym od niej rozwiązaniem tej sytuacji jest obsłużenie wszystkich pozostałych klientów i dopiero przejście w stan uśpienia nie zostawiając przy tym procesów zombie.
  * Wynik: Sukces.
  <table>
  <tr>
    <td width="50%" align="center">
      <img src="https://raw.githubusercontent.com/KamilKlimas/Discount-store/refs/heads/main/img/test4otworzenieKasy1.png" width="100%">
    </td>
    <td width="50%" align="center">
      <img src="https://raw.githubusercontent.com/KamilKlimas/Discount-store/refs/heads/main/img/test4ZamkniecieKasy1.png" width="100%">
    </td>
  </tr>
  </table>

* **Test 5:** Wyczyszczenie zasobów po ewakuacji (scenariusz dla sytuacji, gdy przy największym obciążeniu zostanie użyty sygnał `SIGQUIT`)
  * Oczekiwany rezultat: Po otrzymaniu przez kierownika sygnału `SIGQUIT` wszystkie procesy potomne zostają zakończone, lista semaforów (`ipcs -s`) i pamięci dzielonej (`ipcs -m`) zostaje wyczyszczona.
  * Wynik: Sukces.
<table>
  <tr>
    <td align="center">
      <img src="https://raw.githubusercontent.com/KamilKlimas/Discount-store/refs/heads/main/img/test5_sygnal.png" width="100%">
    </td>
  </tr>
  <tr>
    <td align="center">
      <img src="https://raw.githubusercontent.com/KamilKlimas/Discount-store/refs/heads/main/img/test5_kierownik_generator_reakcja.png" width="100%">
    </td>
  </tr>
  <tr>
    <td align="center">
      <img src="https://raw.githubusercontent.com/KamilKlimas/Discount-store/refs/heads/main/img/test5_wynik.png" width="100%">
    </td>
  </tr>
</table>

## 8. Funkcje systemowe i linki do kodu:

**- Tworzenie i obsługa plików:**
  * `fopen()` : [Zobacz w kodzie (`Src/kierownik.c`)](https://github.com/KamilKlimas/Discount-store/blob/15be105aad2c796cc197e192a1b637c903e7aa97/Src/kierownik.c#L153) – Otwieranie pliku raportu.
  * `fprint()`:  [Zobacz w kodzie (`Src/kierownik.c`)](https://github.com/KamilKlimas/Discount-store/blob/15be105aad2c796cc197e192a1b637c903e7aa97/Src/kierownik.c#L426) – Zapis danych do raportu.
 
 **- Tworzenie procesów:**
  * `fork()`:  [Zobacz w kodzie (`Src/generator.c`)](https://github.com/KamilKlimas/Discount-store/blob/15be105aad2c796cc197e192a1b637c903e7aa97/Src/generator.c#L107) – Tworzenie procesu klienta.
  * `execlp()`: [Zobacz w kodzie (`Src/generator.c`)](https://github.com/KamilKlimas/Discount-store/blob/15be105aad2c796cc197e192a1b637c903e7aa97/Src/generator.c#L111) – Podmiana procesu na program klienta.
   
**- Obsługa procesów i sygnały:** 
  * `wait()`: [Zobacz w kodzie (`Src/generator.c`)](https://github.com/KamilKlimas/Discount-store/blob/15be105aad2c796cc197e192a1b637c903e7aa97/Src/generator.c#L140) – Czekanie na zakończenie procesów potomnych.
  * `kill()`: [Zobacz w kodzie (`Src/kierownik.c`)](https://github.com/KamilKlimas/Discount-store/blob/15be105aad2c796cc197e192a1b637c903e7aa97/Src/kierownik.c#L105) – Wysyłanie sygnałów (np. `SIGUSR1`, `SIGQUIT`).
  * `signal()`: [Zobacz w kodzie (`Src/pracownik.c`)](https://github.com/KamilKlimas/Discount-store/blob/15be105aad2c796cc197e192a1b637c903e7aa97/Src/pracownik.c#L39C7-L39C32) – Rejestracja obsługi sygnałów.
  * `getpid()`: [Zobacz w kodzie (`Src/klient.c`)](https://github.com/KamilKlimas/Discount-store/blob/15be105aad2c796cc197e192a1b637c903e7aa97/Src/klient.c#L49) – Pobranie Pid'u procesu.

**- Synchronizacja (semafory):**
   * `semget()`: (`alokujSemafor()`) : [Zobacz w kodzie (`Src/ipc.c`)](https://github.com/KamilKlimas/Discount-store/blob/15be105aad2c796cc197e192a1b637c903e7aa97/Src/ipc.c#L80) – Tworzenie semaforów.
   * `semctl()`: (`InicjalizujSemafor()`) : [Zobacz w kodzie (`Src/ipc.c`)](https://github.com/KamilKlimas/Discount-store/blob/15be105aad2c796cc197e192a1b637c903e7aa97/Src/ipc.c#L91) – Ustawianie wartości początkowej. 
   * `semop()`: (`waitSemafor()`) : [Zobacz w kodzie (`Src/ipc.c`)](https://github.com/KamilKlimas/Discount-store/blob/15be105aad2c796cc197e192a1b637c903e7aa97/Src/ipc.c#L110) – Operacja P (zablokuj). 
   * `semop()`: (`signalSemafor()`) : [Zobacz w kodzie (`Src/ipc.c`)](https://github.com/KamilKlimas/Discount-store/blob/15be105aad2c796cc197e192a1b637c903e7aa97/Src/ipc.c#L126) – Operacja V (odblokuj). 
  
**- Pamięć dzielona:**
  * `ftok()`:  [Zobacz w kodzie (`Src/ipc.c`)](https://github.com/KamilKlimas/Discount-store/blob/15be105aad2c796cc197e192a1b637c903e7aa97/Src/ipc.c#L17) – Generowanie klucza IPC. 
  * `shmget()` : [Zobacz w kodzie (`Src/ipc.c`)](https://github.com/KamilKlimas/Discount-store/blob/15be105aad2c796cc197e192a1b637c903e7aa97/Src/ipc.c#L31) – Alokacja pamięci dzielonej. 
  * `shmat()`: [Zobacz w kodzie (`Src/ipc.c`)](https://github.com/KamilKlimas/Discount-store/blob/15be105aad2c796cc197e192a1b637c903e7aa97/Src/ipc.c#L54) – Dołączenie pamięci. 
  * `shmdt()`: [Zobacz w kodzie (`Src/ipc.c`)](https://github.com/KamilKlimas/Discount-store/blob/15be105aad2c796cc197e192a1b637c903e7aa97/Src/ipc.c#L65) – Odłączenie pamięci. 
  * `shmctl()`: [Zobacz w kodzie (`Src/ipc.c`)](https://github.com/KamilKlimas/Discount-store/blob/15be105aad2c796cc197e192a1b637c903e7aa97/Src/ipc.c#L73) – Usunięcie segmentu pamięci. 

**- Kolejki komunikatów:**
  * `msgget()`: [Zobacz w kodzie (`Src/ipc.c`)](https://github.com/KamilKlimas/Discount-store/blob/15be105aad2c796cc197e192a1b637c903e7aa97/Src/ipc.c#L143) – Tworzenie kolejki. 
  * `msgsnd()`: [Zobacz w kodzie (`Src/ipc.c`)](https://github.com/KamilKlimas/Discount-store/blob/15be105aad2c796cc197e192a1b637c903e7aa97/Src/ipc.c#L155) – Wysłanie komunikatu. 
  * `msgrcv()` : [Zobacz w kodzie (`Src/ipc.c`)](https://github.com/KamilKlimas/Discount-store/blob/15be105aad2c796cc197e192a1b637c903e7aa97/Src/ipc.c#L167) – Odbiór komunikatu. 
  * `msgctl()` : [Zobacz w kodzie (`Src/ipc.c`)](https://github.com/KamilKlimas/Discount-store/blob/15be105aad2c796cc197e192a1b637c903e7aa97/Src/ipc.c#L178) – Usunięcie kolejki komunikatów. 
  
 **- Kolejki FIFO:**
  * `dodajDoKolejkiFIFO()` : [Zobacz w kodzie (`Src/ipc.c`)](https://github.com/KamilKlimas/Discount-store/blob/15be105aad2c796cc197e192a1b637c903e7aa97/Src/ipc.c#L192) – Dodanie klienta. 
  * `zdejmijZKolejkiFIFO()` :  [Zobacz w kodzie (`Src/ipc.c`)](https://github.com/KamilKlimas/Discount-store/blob/15be105aad2c796cc197e192a1b637c903e7aa97/Src/ipc.c#L207) – Pobranie klienta. 

 **- Obsługa błędów:**
  * `perror()`: [Zobacz w kodzie (`Src/ipc.c`)](https://github.com/KamilKlimas/Discount-store/blob/15be105aad2c796cc197e192a1b637c903e7aa97/Src/ipc.c#L20) – Wypisywanie błędów systemowych. 
