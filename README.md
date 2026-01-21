



  
# Projekt Systemy Operacyjne 2025/2026 - Temat 16: Dyskont  
  
    
  
**Autor:** Kamil Klimas  
  
**Numer albumu:** 155188  
  
**Grupa laboratoryjna:** GP02  
  
**Repozytorium GitHub:** https://github.com/KamilKlimas/Discount-store  
  
    
---  
  
    
## 1. Opis zadania, oraz uruchomienie:
  
 ### 1. Opis zadania:
Projekt stanowi symulację działania dyskontu spożywczego w systemie Linux, wykorzystującą mechanizmy **IPC Systemu** (*pamięć dzieloną, semafory, kolejki komunikatów*) oraz procesy potomne. Symulacja uwzględnia dynamiczne zarządzanie kasami, zachowanie klientów, weryfikacje wieku w przypadku zakupów zawierających alkohol oraz interwencje obsługi.

### 2. Uruchomienie dyskontu:
**- Scenariusz 1:**
* Dla symulacji klasycznej z użyciem funkcji `sleep` i `usleep`, oraz logami w terminalu:
```C
ipc.h
//zostaw zakomentowane #define TRYB_TURBO
//#define TRYB_TURBO 
```
**- Scenariusz 2:**
* Dla symulacji przyspieszonej, bez wyświetlanych logów usuń komentarz dla `#define TRYB_TURBO`:
```C
ipc.h
#define TRYB_TURBO
```
 **- Odpalenie symulacji:** 
```bash
make
chmod +x start_symulacji.sh

./start_symulacji.sh
```

---
## 2. Opis struktury kodu:
**- ```kierownik.c```:**  Główny proces zarządczy. Inicjalizuje środowisko IPC (*SHM, SEM, MSQ*) i powołuje personel. W pętli głównej realizuje algorytm skalowania kas samoobsługowych ( *K*(N-3*) ) oraz monitoruje długość kolejek stacjonarnych, decydując samodzielnie o otwarciu kasy 1. Obsługuje sygnały sterujące (*```SIGUSR1/2```,```SIGQUIT```, ```SIGRTMIN```,```SIGINT```, ```SIGQUIT```*) i odpowiada za sprzątanie zasobów (```watekCzyszczacy()```,), oraz generuje i zapisuje raport końcowy symulacji.

**-```generator.c```:** Symulator natężenia ruchu. W trybie klasycznym odpowiada za cykliczne tworzenie procesów dla ```klient.c``` (```fork()``` + ```execlp()```) w odstępach czasu stymulowanych przez "fale natężenia klientów". W trybie *"turbo"* natomiast tworzy on bursty klientów w kilka sekund, którzy czekają na wpuszczenie do sklepu. Posiada on wątek sprzątający *procesy zombie*.

**-```klient.c```:** Logika kupującego. Losuje koszyk produktów, wybiera kolejkę ( samoobsługa / kasa stacjonarna) i realizuje zakupy. Obsługuje mechanizm *Queue Jumping* (aktywne monitorowanie sąsiedniej kolejki), komunikacją z kasjerem (wymiana wiadomości) oraz procedurę zwrotu towaru w przypadku odmowy sprzedaży alkoholu.

**-```kasjer.c```:** Obsługa kasy stacjonarnej. Nasłuchuje na komunikaty od klientów (paragon), symuluje kasowanie (```SIM_SLEEP_S()```) i weryfikuje wiek (odsyła kod błędu *-2.0* dla klientów niepełnoletnich). Implementuje logikę oszczędzania zasobów (usypianie po 30 sekundach po obsłużeniu ostatniego klienta).

**-```pracownik.c```:** Obsługa mobilna strefy samoobsługowej. Monitoruje flagi w pamięci dzielonej: usuwa awarie techniczne blokujące kasy oraz weryfikuje wiek klientów kupujących alkohol (zatwierdzenie lub odrzucenie transakcji). 

**-```kasy_samo.c```:** Proces reprezentujący terminal samoobsługowy. Utrzymuje status urządzenia (*otwarta/zamknięta/zablokowana*) i przechowuje kwotę bieżącej transakcji w pamięci dzielonej.

**-```ipc.c``` / ```ipc.h```:** Biblioteka pomocnicza IPC. Zawiera definicje podstawowych struktur (`Produkt`, `KasaSamoobslugowa, etc...`), oraz  struktury (`PamiecDzielona`, `messg_buffer`). W plikach są zawarte również stałe systemowe, makra do zmiany trybu symulacji, oraz funkcje obsługi `pamieci dzielonej`, `semaforów`, `kolejki komunikatów`, `kolejki FIFO`, oraz `walidacji wejścia z klawiatury`,

---
## 3. Opis implementacji i wybranych algorytmów:
### 3.1 Mechanizmy synchronizacji i IPC
**- Pamieć dzielona (```shmget```/ ```shmat```):**
	Obiekt `PamiecDzielona` jest mapowany przez każdy proces. Zawiera:
* Tablice struktur `KasaSamo` (statusy, blokady, kwoty).
* Struktury `KolekaFIFO` (cykliczne bufory PID-ów oczekujących klientów).
* Statystyki globalne (liczba klientów w sklepie, utarg).

**- Semafory (`semop`):**
Zestaw 4 semaforów kontroluje dostęp do zasobów, zapobiegając wyścigom (race Conditions):
* `SEM_KASY` (0): Chroni modyfikację statusu kas (otwieranie/zamykanie/blokada), statystyk globalnych, oraz produktów.
* `SEM_UTARG` (1): Chroni utargu sklepu, oraz statystykę liczby klientów.
* `SEM_KOLEJKI` (2): Chroni struktury FIFO podczas dodawania/usuwania klientów.
* `SEM_WEJSCIE` (3): Chroni i kontroluje liczbę klientów znajdujących się aktualnie w sklepie (`MAX_MIEJSC_W_SKLEPIE`).

**- Kolejki komunikatów (`msgrcv`/`msgsnd`):**
Slużą do adresowania wiadomości do konkretnych procesów:
* **Typ wiadomości = PID Klienta:** Kasjer wysyła zaproszenie do konkretnej osoby.
*  **Typ wiadomosci = ID Kasjera + OFFSET:** Klient odsyła listę zakupów do konkretnego kasjera.

**- Wątki i Sygnały (`pthread` / `sigwait`):** 
Zastosowany został model wielowątkowy do asynchronicznego zarządzania zasobami i sygnałami, aby nie blokować głównej logiki symulacji:

 **- Osobny wątek do łapania sygnałów:** 
 W procesie Generatora główny wątek blokuje sygnał `SIGCHLD`, a dedykowany wątek pomocniczy odbiera go funkcją `sigwait()`, co pozwala na natychmiastowe usuwanie procesów Zombie bez przerywania pętli generującej klientów (`waitpid`).
    
**- Wątek Sprzątający:** 
W procesie Kierownika osobny wątek oczekuje na zakończenie pracy, gwarantując usunięcie semaforów i pamięci dzielonej (`IPC_RMID`) przed wyjściem z programu.

### 3.2 Algorytm Klienta:
**- Aktywne oczekiwanie i zmiana kolejki:**
Klient w kasie stacjonarnej nie blokuje się na stałe (`msgrcv` bez flagi), lecz stosuje *aktywne oczekiwanie* dzięki fladze `IPC_NOWAIT`. Pozwala to na monitorowanie otoczenia i zmianę kolejki, jeśli sąsiednia kasa jest  pusta.
 * Logika algorytmu:
 * 1. Start/Sprawdzenie `msgrcv` (`IPC_NOWAIT`)
   * Odebrano wiadomość? -> Tak: Przejdź do płatności
   * Brak wiadomości? -> Zablokuj `SEM_KOLEJKI`
 * 2. Czy zmieniać kolejke? (`sąsiad == pusty && sąsiad_krótszy`)
   * Tak jeśli:
      * Różnica długości kolejek wynosi min. 2 osoby.
      * Bieżąca kasa jest zamknięta (*rozwiązanie problemu pułapki przedwczesnego otworzenia drugiej kasy, gdy 1 nie zdążyła się jeszcze uruchomić*)  
 * 3. Jeśli nie: -> `usunZSrodkaFIFO()` (obecna) -> `dodajDoFIFO` (sąsiednia) -> Odblokuj `SEM_KOLEJKI` 
 * 4. Jeśli nie -> Odblokuj `SEM_KOLEJKI`.
 *  5. Czekaj 200ms -> wróć do punktu 1.
<img src="https://raw.githubusercontent.com/KamilKlimas/Discount-store/refs/heads/main/testy/schemat1-update2.png" width="100%" height="110%">

**- Dynamiczne skalowanie kas:**
Kierownik w pętli głównej monitoruje liczbę klientów w sklepie i dostosowuje liczbę otwartych kas samoobsługowych zgodnie z ustalonymi założeniami.
* Zasady działania:
* 1. **Otwieranie:** Jeśli ```aktualne_kasy < (klienci / K_KLIENTOW_NA_KASE)```
* 2. **Zamykanie:** Jeśli `klienci < K_KLIENTOW_NA_KASE * (aktualne_kasy - 3)`
* 3. **Warunek brzegowy:** Zawsze muszą działać minimum 3 kasy.
<img src="https://raw.githubusercontent.com/KamilKlimas/Discount-store/refs/heads/main/testy/schemat2.png" width="100%" height="110%">

### 3.3 Protokół weryfikacji wieku:
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
## 4. Rzeczy, które okazały się problematyczne i ich rozwiązania:
**- Wyścig danych przy zmianie kolejki:**
* *Problem*: Klienci mogą w dowolnym momencie zdecydować o przejściu do innej kolejki. Jeśli dwóch klientów jednocześnie próbowałoby zmodyfikować tablicę kolejki (np. jeden dochodzi, drugi odchodzi ze środka), doszłoby do uszkodzenia struktur danych (nadpisania PID-ów).

* *Rozwiązanie*: Wprowadziłem dedykowany semafor `SEM_KOLEJKI`. Każda operacja na tablicach FIFO odbywa się w sekcji krytycznej. Dodatkowo klient używa `msgrcv` z flagą `IPC_NOWAIT`, co pozwala mu aktywnie czekać i sprawdzać stan sąsiednich kolejek bez blokowania procesu.

**- Celowane dostarczanie wiadomości:**
* *Problem*: W systemie istnieje tylko jedna główna kolejka komunikatów, a klientów jest wielu. Pojawia się więc pytanie "Jak sprawić, by paragon trafił do właściwego klienta, a nie został odebrany przez kogoś innego"?

* *Rozwiązanie*: Wykorzystano pole `mtype` w strukturze komunikatu.
Zaproszenie: Kasjer wysyła wiadomość o typie równym PID klienta.
Paragon: Klient wysyła listę zakupów o typie ID_Kasjera + OFFSET. Dzięki temu każdy proces wyciąga z kolejki tylko te komunikaty, które są przeznaczone konkretnie dla niego.

## 5. Elementy wyróżniające:

**- Automatyczne uruchomienie symulacji:** Projekt wykorzystuje *skrypt Bash* aranżując sesję terminala `tmux`. Dzieli on automatycznie okno terminala na 4 niezależne panele uruchamiając w nich: proces `kierownik.c`, monitoring zasobów IPC w czasie rzeczywistym (`watch ipcs`) oraz dwie puste konsole (jedna do uruchomienia `generator.c`/`klient.c`, a druga do wysyłania sygnałów).

**- Wizualna segregacja procesów:** Wykorzystano funkcję `printf` do formatowania wyjścia standardowego (`stdout`) przy użyciu kodów ANSI.

## 6. Testy:


<details>
  <summary><h3>Test 1 – Obsługa klientów, alkohol i reakcje awaryjne</h3></summary>
  <br>
  <p>
  <em>Opis testu:</em> Sprawdza, jak system zachowuje się przy standardowym ruchu klientów oraz w sytuacjach szczególnych (każdy klient "posiada" alkohol w koszyku, kasa samoobsługowa każdorazowo ma awarie). Weryfikuje poprawne otwieranie/zamykanie kas, obsługę kolejki, mechanizmy awaryjne i integralność IPC/logów. Test jest podzielony na dwa scenariusze.
</p>
  <br>
  
  <table>
    <tr>
      <th align="center">Zmiana: awaria</th>
      <th align="center">Zmiana: Alkohol</th>
    </tr>
    <tr>
      <td width="50%">
        <img src="https://raw.githubusercontent.com/KamilKlimas/Discount-store/refs/heads/main/testy/test1/awaria99_zmiana_klient.png" width="100%" alt="Zmiana kodu awaria">
      </td>
      <td width="50%">
        <img src="https://raw.githubusercontent.com/KamilKlimas/Discount-store/refs/heads/main/testy/test1/alko1_zmiana_klient.png" width="100%" alt="Zmiana kodu alkohol">
      </td>
    </tr>
  </table>

  <br>

  <details>
    <summary><strong>Scenariusz 1: Klasyczny ruch i zakończenie SIGINT</strong></summary>
    <br>
	  <p>
	    Sklep podczas klasycznego ruchu ma za zadanie poradzić sobie z obsługą, aż do
	    końca symulacji bez zakleszczeń. Po ukończeniu obsługi kierownik otrzymuje <code>SIGINT</code>
	    oraz loguje raport potwierdzający, że żaden z klientów nie zaginął po drodze.
	   </p> 
   <br>
		<p>
	   <em>Wyniki testu:</em> 
	   </p>
	   
<div align="center">
      <table>
        <tr>
          <td width="33%" align="center">
            <a href="https://github.com/KamilKlimas/Discount-store/blob/main/testy/test1/scenariusz1/logi_generator.txt">Logi: Generator</a>
          </td>
          <td width="33%" align="center">
            <a href="https://github.com/KamilKlimas/Discount-store/blob/main/testy/test1/scenariusz1/logi_kasjer.txt">Logi: Kasjer</a>
          </td>
          <td width="33%" align="center">
            <a href="https://github.com/KamilKlimas/Discount-store/blob/main/testy/test1/scenariusz1/logi_kierownik.txt">Logi: Kierownik</a>
          </td>
        </tr>
        <tr>
          <td align="center">
            <a href="https://github.com/KamilKlimas/Discount-store/blob/main/testy/test1/scenariusz1/logi_klient.txt">Logi: Klient</a>
          </td>
          <td align="center">
            <a href="https://github.com/KamilKlimas/Discount-store/blob/main/testy/test1/scenariusz1/logi_kasa_samo.txt">Logi: Kasa Samo.</a>
          </td>
          <td align="center">
            <a href="https://github.com/KamilKlimas/Discount-store/blob/main/testy/test1/scenariusz1/logi_pracownik.txt">Logi: Pracownik</a>
          </td>
        </tr>
      </table>
    </div>
<p align="center"> <img src="https://raw.githubusercontent.com/KamilKlimas/Discount-store/refs/heads/main/testy/test1/scenariusz1/Pomyslne_zakonczenie_symulacji.png" width="100%"> </p> <br>
  </details>

  <details>
    <summary><strong>Scenariusz 2: Ewakuacja (SIGQUIT)</strong></summary>
    <br>
    <p>
    Symulacja rozpoczyna się w taki sam sposób jak w scenariuszu 1.
    Zmiana polega na wysłaniu do kierownika sygnału o ewakuacji <code>SIGQUIT</code> w trakcie
    trwania największego ruchu. Symulacja ma za zadanie się natychmiastowo zakończyć,
    sprzątając za sobą wszystkie zasoby.
    </p>

<p>
	   <em>Wyniki testu:</em> 
	   </p>

<div align="center">
      <table>
        <tr>
          <td width="33%" align="center">
            <a href="https://github.com/KamilKlimas/Discount-store/blob/main/testy/test1/scenariusz2/logi_generator.txt">Logi: Generator</a>
          </td>
          <td width="33%" align="center">
            <a href="https://github.com/KamilKlimas/Discount-store/blob/main/testy/test1/scenariusz2/logi_kasjer.txt">Logi: Kasjer</a>
          </td>
          <td width="33%" align="center">
            <a href="https://github.com/KamilKlimas/Discount-store/blob/main/testy/test1/scenariusz2/logi_kierownik.txt">Logi: Kierownik</a>
          </td>
        </tr>
        <tr>
          <td align="center">
            <a href="https://github.com/KamilKlimas/Discount-store/blob/main/testy/test1/scenariusz2/logi_klient.txt">Logi: Klient</a>
          </td>
          <td align="center">
            <a href="https://github.com/KamilKlimas/Discount-store/blob/main/testy/test1/scenariusz2/logi_kasa_samo.txt">Logi: Kasa Samo.</a>
          </td>
          <td align="center">
            <a href="https://github.com/KamilKlimas/Discount-store/blob/main/testy/test1/scenariusz2/logi_pracownik.txt">Logi: Pracownik</a>
          </td>
        </tr>
      </table>
    </div>
<p align="center"> <img src="https://raw.githubusercontent.com/KamilKlimas/Discount-store/refs/heads/main/testy/test1/scenariusz2/SIGQUIT_pomyslnei.png" width="100%"> </p> <br>
  </details>
  </details>

</details>

## 7. Funkcje systemowe i linki do kodu:

**- Tworzenie i obsługa plików:**
  * `fopen()` : [Zobacz w kodzie (`Src/kierownik.c`)](https://github.com/KamilKlimas/Discount-store/blob/f3dcb1c08de4677d4b1d582d16bd1f61076cc38a/Src/kierownik.c#L53) – Otwieranie pliku raportu.
  * `fprint()`:  [Zobacz w kodzie (`Src/kierownik.c`)](https://github.com/KamilKlimas/Discount-store/blob/f3dcb1c08de4677d4b1d582d16bd1f61076cc38a/Src/kierownik.c#L55) – Zapis danych do raportu.
 
 **- Tworzenie procesów:**
  * `fork()`:  [Zobacz w kodzie (`Src/generator.c`)](https://github.com/KamilKlimas/Discount-store/blob/f3dcb1c08de4677d4b1d582d16bd1f61076cc38a/Src/generator.c#L138) – Tworzenie procesu klienta.
  * `execlp()`: [Zobacz w kodzie (`Src/generator.c`)](https://github.com/KamilKlimas/Discount-store/blob/f3dcb1c08de4677d4b1d582d16bd1f61076cc38a/Src/generator.c#L144) – Podmiana procesu na program klienta.
   
**- Obsługa procesów i sygnały:** 
  * `kill()`: [Zobacz w kodzie (`Src/kierownik.c`)](https://github.com/KamilKlimas/Discount-store/blob/f3dcb1c08de4677d4b1d582d16bd1f61076cc38a/Src/kierownik.c#L210) – Wysyłanie sygnałów (np. `SIGUSR1`, `SIGQUIT`).
  * `signal()`: [Zobacz w kodzie (`Src/pracownik.c`)](https://github.com/KamilKlimas/Discount-store/blob/f3dcb1c08de4677d4b1d582d16bd1f61076cc38a/Src/pracownik.c#L46) – Rejestracja obsługi sygnałów.
  * `getpid()`: [Zobacz w kodzie (`Src/klient.c`)](https://github.com/KamilKlimas/Discount-store/blob/f3dcb1c08de4677d4b1d582d16bd1f61076cc38a/Src/klient.c#L119) – Pobranie Pid'u procesu.

**- Synchronizacja (semafory):**
   * `semget()`: (`alokujSemafor()`) : [Zobacz w kodzie (`Src/ipc.c`)](https://github.com/KamilKlimas/Discount-store/blob/f3dcb1c08de4677d4b1d582d16bd1f61076cc38a/Src/ipc.c#L82) – Tworzenie semaforów.
   * `semctl()`: (`InicjalizujSemafor()`) : [Zobacz w kodzie (`Src/ipc.c`)](https://github.com/KamilKlimas/Discount-store/blob/f3dcb1c08de4677d4b1d582d16bd1f61076cc38a/Src/ipc.c#L93) – Ustawianie wartości początkowej. 
   * `semop()`: (`waitSemafor()`) : [Zobacz w kodzie (`Src/ipc.c`)](https://github.com/KamilKlimas/Discount-store/blob/f3dcb1c08de4677d4b1d582d16bd1f61076cc38a/Src/ipc.c#L115) – Operacja P (zablokuj). 
   * `semop()`: (`signalSemafor()`) : [Zobacz w kodzie (`Src/ipc.c`)](https://github.com/KamilKlimas/Discount-store/blob/f3dcb1c08de4677d4b1d582d16bd1f61076cc38a/Src/ipc.c#L138) – Operacja V (odblokuj). 
  
**- Pamięć dzielona:**
  * `ftok()`:  [Zobacz w kodzie (`Src/ipc.c`)](https://github.com/KamilKlimas/Discount-store/blob/f3dcb1c08de4677d4b1d582d16bd1f61076cc38a/Src/ipc.c#L19) – Generowanie klucza IPC. 
  * `shmget()` : [Zobacz w kodzie (`Src/ipc.c`)](https://github.com/KamilKlimas/Discount-store/blob/f3dcb1c08de4677d4b1d582d16bd1f61076cc38a/Src/ipc.c#L33) – Alokacja pamięci dzielonej. 
  * `shmat()`: [Zobacz w kodzie (`Src/ipc.c`)](https://github.com/KamilKlimas/Discount-store/blob/f3dcb1c08de4677d4b1d582d16bd1f61076cc38a/Src/ipc.c#L56) – Dołączenie pamięci. 
  * `shmdt()`: [Zobacz w kodzie (`Src/ipc.c`)](https://github.com/KamilKlimas/Discount-store/blob/f3dcb1c08de4677d4b1d582d16bd1f61076cc38a/Src/ipc.c#L67) – Odłączenie pamięci. 
  * `shmctl()`: [Zobacz w kodzie (`Src/ipc.c`)](https://github.com/KamilKlimas/Discount-store/blob/f3dcb1c08de4677d4b1d582d16bd1f61076cc38a/Src/ipc.c#L75) – Usunięcie segmentu pamięci. 

**- Kolejki komunikatów:**
  * `msgget()`: [Zobacz w kodzie (`Src/ipc.c`)](https://github.com/KamilKlimas/Discount-store/blob/f3dcb1c08de4677d4b1d582d16bd1f61076cc38a/Src/ipc.c#L171) – Tworzenie kolejki. 
  * `msgsnd()`: [Zobacz w kodzie (`Src/ipc.c`)](https://github.com/KamilKlimas/Discount-store/blob/145f40c8d199a75ca471d9be4124868d9bd1e41a/Src/ipc.c#L176) – Wysłanie komunikatu. 
  * `msgrcv()` : [Zobacz w kodzie (`Src/ipc.c`)](https://github.com/KamilKlimas/Discount-store/blob/f3dcb1c08de4677d4b1d582d16bd1f61076cc38a/Src/ipc.c#L197) – Odbiór komunikatu. 
  * `msgctl()` : [Zobacz w kodzie (`Src/ipc.c`)](https://github.com/KamilKlimas/Discount-store/blob/f3dcb1c08de4677d4b1d582d16bd1f61076cc38a/Src/ipc.c#L211) – Usunięcie kolejki komunikatów. 
  
 **- Kolejki FIFO:**
  * `dodajDoKolejkiFIFO()` : [Zobacz w kodzie (`Src/ipc.c`)](https://github.com/KamilKlimas/Discount-store/blob/f3dcb1c08de4677d4b1d582d16bd1f61076cc38a/Src/ipc.c#L252) – Dodanie klienta. 
  * `zdejmijZKolejkiFIFO()` :  [Zobacz w kodzie (`Src/ipc.c`)](https://github.com/KamilKlimas/Discount-store/blob/f3dcb1c08de4677d4b1d582d16bd1f61076cc38a/Src/ipc.c#L267) – Pobranie klienta. 
 
**- Wątki i zaawansowane sygnały:**
 * `pthread_create()`:  [Zobacz w kodzie (`Src/generator.c`)](https://github.com/KamilKlimas/Discount-store/blob/f3dcb1c08de4677d4b1d582d16bd1f61076cc38a/Src/generator.c#L67) – Tworzenie wątków pomocniczych.
 *   `sigwait()`:  [Zobacz w kodzie (`Src/generator.c`)](https://github.com/KamilKlimas/Discount-store/blob/f3dcb1c08de4677d4b1d582d16bd1f61076cc38a/Src/generator.c#L35) – Oczekiwanie na sygnały bez przerywania pracy (`ASync-Signal-Safe`).
 *  `pthread_sigmask()`:  [Zobacz w kodzie (`Src/generator.c`)](https://github.com/KamilKlimas/Discount-store/blob/f3dcb1c08de4677d4b1d582d16bd1f61076cc38a/Src/generator.c#L61) - Blokowanie sygnałów dla wątków.
 * `pthread_join()`: [Zobacz w kodzie (`Src/generator.c`)](https://github.com/KamilKlimas/Discount-store/blob/f3dcb1c08de4677d4b1d582d16bd1f61076cc38a/Src/generator.c#L164) – Oczekiwanie na zakończenie wątku sprzątającego.


 **- Obsługa błędów:**
  * `perror()`: [Zobacz w kodzie (`Src/ipc.c`)](https://github.com/KamilKlimas/Discount-store/blob/f3dcb1c08de4677d4b1d582d16bd1f61076cc38a/Src/ipc.c#L22) – Wypisywanie błędów systemowych. 
  * `inputExceptionHandler()`: [Zobacz w kodzie (`Src/ipc.c`)](https://github.com/KamilKlimas/Discount-store/blob/f3dcb1c08de4677d4b1d582d16bd1f61076cc38a/Src/ipc.c#L320) – Walidacja danych wejścia standardowego.
