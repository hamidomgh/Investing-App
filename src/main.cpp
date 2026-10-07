#include "statistik.hpp"

#include <iostream>

// Liest Zahlen von der Standardeingabe und gibt eine Statistik aus.
//   echo "3 1 4 1 5" | ./build/projekt
int main() {
    Statistik statistik;
    double wert = 0.0;
    while (std::cin >> wert) {
        statistik.hinzufuegen(wert);
    }
    if (statistik.anzahl() == 0) {
        std::cerr << "Keine Werte gelesen.\n";
        return 1;
    }
    std::cout << "Anzahl:     " << statistik.anzahl() << '\n'
              << "Mittelwert: " << statistik.mittelwert() << '\n'
              << "Minimum:    " << statistik.minimum() << '\n'
              << "Maximum:    " << statistik.maximum() << '\n';
    return 0;
}
