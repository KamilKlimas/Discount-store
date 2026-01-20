cmake_minimum_required(VERSION 3.20)
project(SystemKasowy C)

set(CMAKE_C_STANDARD 11)

# 1. Wskazujemy folder z plikami nagłówkowymi (.h)
include_directories(Include)

# 2. Definiujemy osobne programy (pliki wykonywalne)
# Składnia: add_executable(nazwa_programu plik_z_main.c pliki_pomocnicze.c)

# Generator
add_executable(generator Src/generator.c Src/ipc.c)

# Kasa samoobsługowa
add_executable(kasa_samo Src/kasa_samo.c Src/ipc.c)

# Kasjer
add_executable(kasjer Src/kasjer.c Src/ipc.c)

# Kierownik
add_executable(kierownik Src/kierownik.c Src/ipc.c)

# Klient
add_executable(klient Src/klient.c Src/ipc.c)

# Pracownik
add_executable(pracownik Src/pracownik.c Src/ipc.c)