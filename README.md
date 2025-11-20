# Projekt Systemy Operacyjne 2025/2026 - Temat 16: Dyskont

**Autor:** Kamil Klimas
**Numer albumu:** 155188
**Grupa laboratoryjna:** GP02
**Repozytorium GitHub:** https://github.com/KamilKlimas/Discount-store

---

## 1. Opis zadania (Temat 16 - Dyskont)

Celem projektu jest symulacja działania dyskontu spożywczego przy użyciu mechanizmów systemów operacyjnych (procesy, IPC, sygnały). Symulacja uwzględnia obsługę klientów, kas samoobsługowych, kas stacjonarnych oraz zarządzanie sklepem przez kierownika.

### Główne założenia symulacji:
* **Architektura:** Rozwiązanie niescentralizowane oparte na procesach (`fork`, `exec`).
* **Zasoby sklepu:**
    * 8 kas łącznie: 2 stacjonarne i 6 samoobsługowych.
    * Personel: Kierownik, Kasjerzy, Pracownik obsługi.
* **Logika Klienta:**
    * Klienci pojawiają się losowo i robią zakupy (3-10 produktów) przez losowy czas.
    * Preferencje kas: 95% wybiera kasy samoobsługowe, 5% kasy stacjonarne.
    * Klienci kupujący alkohol muszą przejść weryfikację wieku (Pracownik obsługi).
* **Logika Kas Samoobsługowych:**
    * Wspólna kolejka, zawsze min. 3 aktywne.
    * Skalowanie: liczba czynnych kas zależy od liczby klientów w sklepie (reguła $K \cdot (N-3)$).
    * Obsługa błędów: losowe zablokowanie kasy (wymaga interwencji obsługi).
* **Logika Kas Stacjonarnych:**
    * Kasa 1: Otwierana automatycznie, gdy w kolejce > 3 osoby. Zamykana po 30s bezczynności.
    * Kasa 2: Otwierana tylko na sygnał Kierownika.
* **Interwencje Kierownika (Sygnały):**
    * `Sygnał 1`: Otwarcie Kasy 2.
    * `Sygnał 2`: Zamknięcie wskazanej kasy (1 lub 2).
    * `Sygnał 3`: Ewakuacja (klienci wychodzą natychmiast bez zakupów, kasy są zamykane).

---

## 2. Założenia projektowe i implementacja

### Struktura procesów
Symulacja zostanie zrealizowana w języku C/C++ w środowisku Linux.
* **Proces Główny (Manager):** Inicjalizuje środowisko, tworzy zasoby IPC i procesy potomne.
* **Proces Kierownik:** Wysyła sygnały sterujące do kasjerów i klientów.
* **Procesy Klientów:** Tworzone dynamicznie/cyklicznie. Każdy klient to osobny proces wykonujący symulację zakupów i płatności.
* **Procesy/Wątki Kas:** Obsługują logikę kasowania produktów i kolejkowania.

### Mechanizmy IPC (Komunikacja międzyprocesowa)
W projekcie wykorzystane zostaną następujące mechanizmy (wymagane min. 2 różne):
1.  **Pamięć dzielona (Shared Memory):** Przechowywanie stanu kolejek, liczby aktywnych kas oraz flag stanu sklepu.
2.  **Semafory (Semaphores):** Synchronizacja dostępu do pamięci dzielonej (sekcje krytyczne) oraz blokowanie klientów w kolejkach.
3.  **Kolejki komunikatów (Message Queues):** Komunikacja między kasami samoobsługowymi a pracownikiem obsługi (np. wezwanie do alkoholu lub błędu wagi).

---
## 3. Plan testów

### Test 1: Podstawowa obsługa i skalowanie kas samoobsługowych
* **Scenariusz:** Uruchomienie symulacji z małą liczbą klientów, następnie gwałtowne zwiększenie liczby klientów.
* **Oczekiwany rezultat:** Początkowo działają 3 kasy samoobsługowe. Po przekroczeniu progu klientów, system automatycznie otwiera kolejne kasy (do 6). Po obsłużeniu "fali", kasy są zamykane do poziomu 3.
* **Weryfikacja:** Logi systemowe wskazujące moment otwarcia/zamknięcia kas.

### Test 2: Obsługa kas stacjonarnych i limitów czasu
* **Scenariusz:** Generowanie klientów preferujących kasy stacjonarne. Kolejka przekracza 3 osoby. Następnie brak nowych klientów przez >30s.
* **Oczekiwany rezultat:** Kasa 1 otwiera się automatycznie po przekroczeniu limitu kolejki. Po 30 sekundach bezczynności proces Kasy 1 kończy pracę (zamyka się).
* **Weryfikacja:** Obserwacja komunikatów wyjścia (stdout/plik raportu).

### Test 3: Obsługa wyjątków (Alkohol i Błąd wagi)
* **Scenariusz:** Klient "kupuje" alkohol przy kasie samoobsługowej oraz losowe wystąpienie błędu wagi.
* **Oczekiwany rezultat:** Proces klienta zostaje zablokowany (wstrzymany) do momentu interwencji Pracownika Obsługi. Po "zatwierdzeniu", klient kończy transakcję.
* **Weryfikacja:** Brak zakleszczenia (deadlock) przy zablokowanej kasie; inne kasy działają płynnie.

### Test 4: Interwencja Kierownika i Ewakuacja (Sygnały)
* **Scenariusz:**
    1. Wysłanie `Sygnału 1`: Wymuszenie otwarcia Kasy 2.
    2. Wysłanie `Sygnału 3`: Ogłoszenie ewakuacji.
* **Oczekiwany rezultat:**
    1. Kasa 2 otwiera się niezależnie od długości kolejki.
    2. Po sygnale ewakuacji wszyscy klienci natychmiast kończą procesy (bez finalizacji zakupów), a pracownicy kończą pracę i zwalniają zasoby IPC.
* **Weryfikacja:** Czyste zakończenie wszystkich procesów, brak "sierot", usunięcie zasobów IPC (ipcs).
