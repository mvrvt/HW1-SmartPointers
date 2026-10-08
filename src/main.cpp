#include <cstddef>
#include <iostream>
#include <limits>
#include <stdexcept>

#include "pointers/MemorySpan.hpp"
#include "pointers/MsPtr.hpp"

namespace {

void ClearInput() {
    std::cin.clear();
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
}

void RunInteractiveSandbox() {
    std::cout << "\n--- Интерактивная песочница MemorySpan & MsPtr ---\n";
    constexpr size_t kSpanSize = 5;
    MemorySpan<int> span(kSpanSize);

    // Инициализируем значениями 10, 20, 30, 40, 50 (через MsPtr: span[i] снаружи недоступен)
    for (size_t i = 0; i < kSpanSize; ++i)
        *span.Locate(i) = static_cast<int>((i + 1) * 10);

    MsPtr<int> ptr = span.Locate(0);
    std::cout << "Создан MemorySpan размера " << kSpanSize << ".\n";
    std::cout << "MsPtr указывает на индекс: " << ptr.GetIndex()
              << ", значение: " << *ptr << "\n";

    bool in_sandbox = true;
    while (in_sandbox) {
        std::cout << "\nКоманды песочницы:\n"
                  << " 1. Инкремент (++ptr)\n"
                  << " 2. Декремент (--ptr)\n"
                  << " 3. Сдвиг на смещение (ptr += offset)\n"
                  << " 4. Прочитать текущее значение (*ptr)\n"
                  << " 0. Вернуться в главное меню\n"
                  << "Выберите действие: ";

        int choice = 0;
        if (!(std::cin >> choice)) {
            ClearInput();
            continue;
        }

        try {
            switch (choice) {
                case 1:
                    ++ptr;
                    std::cout << "Сдвиг выполнен. Индекс: " << ptr.GetIndex() << "\n";
                    break;
                case 2:
                    --ptr;
                    std::cout << "Сдвиг выполнен. Индекс: " << ptr.GetIndex() << "\n";
                    break;
                case 3: {
                    std::cout << "Введите целочисленное смещение (например, 2 или -1): ";
                    ptrdiff_t offset = 0;
                    if (!(std::cin >> offset)) {
                        ClearInput();
                        std::cout << "Некорректное число.\n";
                        break;
                    }
                    ptr += offset;
                    std::cout << "Сдвиг выполнен. Новый индекс: " << ptr.GetIndex() << "\n";
                    break;
                }
                case 4:
                    std::cout << "Элемент по индексу [" << ptr.GetIndex() << "] = " << *ptr << "\n";
                    break;
                case 0:
                    in_sandbox = false;
                    break;
                default:
                    std::cout << "Неизвестная команда.\n";
                    break;
            }
        } catch (const std::exception& e) {
            std::cout << "Перехвачено исключение: " << e.what() << "\n";
        }
    }
}

void PrintMainMenu() {
    std::cout << "\n========================================\n"
              << "   ЛАБОРАТОРНАЯ РАБОТА: SMART POINTERS  \n"
              << "========================================\n"
              << " 1. Интерактивная песочница (MemorySpan & MsPtr)\n"
              << " 0. Выход\n"
              << "----------------------------------------\n"
              << "Выберите пункт меню: ";
}

}  // namespace

int main() {
    bool running = true;
    while (running) {
        PrintMainMenu();
        int choice = 0;
        if (!(std::cin >> choice)) {
            ClearInput();
            std::cout << "Пожалуйста, введите корректное число.\n";
            continue;
        }

        switch (choice) {
            case 1:
                RunInteractiveSandbox();
                break;
            case 0:
                std::cout << "Завершение программы.\n";
                running = false;
                break;
            default:
                std::cout << "Неверный пункт меню. Попробуйте снова.\n";
                break;
        }
    }
    return 0;
}
