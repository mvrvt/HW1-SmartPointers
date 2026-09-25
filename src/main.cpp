#include <gtest/gtest.h>

#include <iostream>
#include <limits>
#include <string>

#include "ArraySequence.hpp"
#include "pointers/MemorySpan.hpp"
#include "pointers/MsPtr.hpp"

namespace {

void RunAllGoogleTests() {
  std::cout << "\n========================================\n";
  std::cout << "       ЗАПУСК ВСЕХ ЮНИТ-ТЕСТОВ          \n";
  std::cout << "========================================\n";

  // Инициализируем аргументы для движка Google Test
  int fake_argc = 1;
  char app_name[] = "smart_pointers_app";
  char* fake_argv[] = {app_name, nullptr};

  ::testing::InitGoogleTest(&fake_argc, fake_argv);

  // RUN_ALL_TESTS() выполнит все макросы TEST(...) из test_all.cpp
  int test_result = RUN_ALL_TESTS();
  if (test_result == 0) {
    std::cout << "\n>>> ВСЕ ПРОВЕРКИ ПРОЙДЕНЫ УСПЕШНО! <<<\n";
  } else {
    std::cout << "\n>>> ОБНАРУЖЕНЫ ОШИБКИ В ТЕСТАХ! <<<\n";
  }
}

void RunInteractiveSandbox() {
  std::cout << "\n--- Интерактивная песочница MemorySpan & MsPtr ---\n";
  constexpr std::size_t kSpanSize = 5;
  MemorySpan<int> span(kSpanSize);

  // Инициализируем значениями 10, 20, 30, 40, 50
  for (std::size_t i = 0; i < kSpanSize; ++i) {
    span[i] = static_cast<int>((i + 1) * 10);
  }

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
      std::cin.clear();
      std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
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
          std::cin >> offset;
          ptr += offset;
          std::cout << "Сдвиг выполнен. Новый индекс: " << ptr.GetIndex() << "\n";
          break;
        }
        case 4:
          std::cout << "Элемент по индексу [" << ptr.GetIndex()
                    << "] = " << *ptr << "\n";
          break;
        case 0:
          in_sandbox = false;
          break;
        default:
          std::cout << "Неизвестная команда.\n";
          break;
      }
    } catch (const std::exception& e) {
      std::cout << "⚠️ Перехвачено исключение безопасности: " << e.what() << "\n";
    }
  }
}

void PrintMainMenu() {
  std::cout << "\n========================================\n"
            << "   ЛАБОРАТОРНАЯ РАБОТА: SMART POINTERS  \n"
            << "========================================\n"
            << " 1. Запустить юнит-тесты (Google Test)\n"
            << " 2. Интерактивная песочница (MemorySpan & MsPtr)\n"
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
      std::cin.clear();
      std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
      std::cout << "Пожалуйста, введите корректное число.\n";
      continue;
    }

    switch (choice) {
      case 1:
        RunAllGoogleTests();
        break;
      case 2:
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
