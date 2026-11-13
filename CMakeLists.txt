# Erfordert mindestens CMake Version 3.28 für die Verwendung
cmake_minimum_required(VERSION 3.28)

# Definiert das Projekt mit dem Namen "projekt" und C++-Sprache
project(projekt LANGUAGES CXX)

# Setzt das C++-Standard auf C++20 (erforderlich für moderne C++-Features)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Aktiviert die Erzeugung von compile_commands.json für Tools wie clang-tidy und Editoren
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)   # für clang-tidy und Editoren

# Setzt den Build-Typ auf Debug, wenn keiner angegeben wurde
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

# Fügt Compile-Optionen hinzu für bessere Fehlererkennung
add_compile_options(-Wall -Wextra -Wpedantic)

# Option zur Aktivierung von Sanitizern (AddressSanitizer und UBSan)
option(ENABLE_SANITIZERS "AddressSanitizer und UBSan" OFF)
if(ENABLE_SANITIZERS)
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer -g)
    add_link_options(-fsanitize=address,undefined)
endif()

# Programmlogik als Bibliothek: Programm und Tests nutzen denselben Code.
# Neue .cpp-Dateien hier eintragen.
add_library(projekt_lib
    src/statistik.cpp
    src/kursdaten.cpp
    src/trendindikatoren.cpp
    src/risikokennzahlen.cpp
    src/supportresistance.cpp
    src/zinsentscheidung.cpp
    src/ereignisstudie.cpp
    src/http_client.cpp
    src/umgebung.cpp
    src/alphavantage_quelle.cpp
    src/katalog.cpp
    src/kursabruf.cpp
    src/http_cache.cpp
    src/anfragebehandlung.cpp
    src/dateiquelle.cpp
    src/threadpool.cpp
    src/ergebnis_cache.cpp
    src/gecachte_anfragebehandlung.cpp
)
# Stellt sicher, dass Header-Dateien im src-Verzeichnis gefunden werden können
target_include_directories(projekt_lib PUBLIC src)
# Header-only-Bibliotheken aus third_party/ (z. B. nlohmann/json). SYSTEM unterdrückt deren Warnungen,
# PRIVATE: nur die .cpp-Dateien der Bibliothek brauchen sie, die eigenen Header nicht.
target_include_directories(projekt_lib SYSTEM PRIVATE third_party)

# Netzwerkschicht als eigene kleine Bibliothek (E8/E9): reine Übersetzung zwischen cpp-httplib und
# IAnfrageBehandler. Getrennt von projekt_lib, damit nur diese Datei die große httplib.h übersetzt.
# Die Tests binden sie ein, um den Server über echte lokale Sockets zu prüfen (Parallelität, Beenden).
add_library(projekt_server src/http_server.cpp)
target_link_libraries(projekt_server PUBLIC projekt_lib)
# cpp-httplib (third_party/httplib.h); SYSTEM unterdrückt Warnungen aus fremdem Code
target_include_directories(projekt_server SYSTEM PRIVATE third_party)
# Threads (pthread) für cpp-httplib und unseren ThreadPool
find_package(Threads REQUIRED)
target_link_libraries(projekt_server PUBLIC Threads::Threads)

# Erstellt das Hauptprogramm (Server) aus main.cpp
add_executable(projekt src/main.cpp)
# Verlinkt das Hauptprogramm mit der Netzwerkschicht (und darüber mit projekt_lib)
target_link_libraries(projekt PRIVATE projekt_server)

# Tests
enable_testing()
# Findet die Google Test Bibliothek
find_package(GTest REQUIRED)
# Fügt Google Test Funktionen ein
include(GoogleTest)

# Neue Testdateien hier eintragen.
add_executable(tests
    tests/test_statistik.cpp
    tests/test_kursdaten.cpp
    tests/test_trendindikatoren.cpp
    tests/test_risikokennzahlen.cpp
    tests/test_supportresistance.cpp
    tests/test_ereignisstudie.cpp
    tests/test_quellen.cpp
    tests/test_alphavantage.cpp
    tests/test_katalog.cpp
    tests/test_http_cache.cpp
    tests/test_anfragebehandlung.cpp
    tests/test_dateiquelle.cpp
    tests/test_nebenlaeufigkeit.cpp
    tests/test_http_server.cpp
)
# Verlinkt die Tests mit der Bibliothek und Google Test
target_link_libraries(tests PRIVATE projekt_lib projekt_server GTest::gtest_main)
# Die Tests lesen JSON-Antworten der Schnittstelle (E8) mit nlohmann/json
target_include_directories(tests SYSTEM PRIVATE third_party)
# Pfad zu den Testdaten (aufgezeichnete und handgeschriebene Antworten), als Konstante FIXTURES_DIR im Code
target_compile_definitions(tests PRIVATE FIXTURES_DIR="${CMAKE_SOURCE_DIR}/tests/fixtures")
# Pfad zum Konfigurationsordner (config/katalog.json), damit ein Test den echten Katalog prüfen kann
target_compile_definitions(tests PRIVATE KONFIG_DIR="${CMAKE_SOURCE_DIR}/config")
# Erkennt automatisch alle TEST() Makros als CTest-Tests
# jeder TEST() wird ein CTest-Test; TIMEOUT: ein hängender Test (z. B. Deadlock) bricht nach 60 s ab
gtest_discover_tests(tests PROPERTIES TIMEOUT 60)