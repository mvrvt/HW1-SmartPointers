#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>

#include "pointers/MemorySpan.hpp"
#include "pointers/MsPtr.hpp"
#include "pointers/ShrdPtr.hpp"
#include "pointers/UnqPtr.hpp"

struct InputEnded {};

// Читает строку целиком. Без проверки на конец ввода программа бесконечно
// спрашивала бы число у пустого потока
std::string ReadLine() {
    std::string line;
    if (!std::getline(std::cin, line))
        throw InputEnded();
    return line;
}

int ReadInt(const std::string& prompt,
            int min_value = std::numeric_limits<int>::min(),
            int max_value = std::numeric_limits<int>::max()) {
    while (true) {
        std::cout << prompt;
        const std::string line = ReadLine();
        if (line.empty())
            continue;
        try {
            size_t parsed = 0;
            const int value = std::stoi(line, &parsed);
            if (parsed != line.length())
                throw std::invalid_argument("not a number");
            if (value < min_value || value > max_value) {
                std::cout << " [!] Введите число от " << min_value << " до " << max_value << ".\n";
                continue;
            }
            return value;
        } catch (const std::exception&) {
            std::cout << " [!] Введите корректное целое число.\n";
        }
    }
}

// ============================================================================
// 1. UnqPtr - единоличное владение
// ============================================================================

void PrintUnqPtr(const std::string& name, const UnqPtr<int>& ptr) {
    std::cout << "   " << name << ": ";
    if (ptr)
        std::cout << *ptr << "   (объект по адресу " << ptr.Get() << ")\n";
    else
        std::cout << "пусто\n";
}

void UnqPtrMenu() {
    UnqPtr<int> first;
    UnqPtr<int> second;

    while (true) {
        std::cout << "\n";
        std::cout << " |============== UnqPtr<int>: единоличное владение ===============|\n";
        std::cout << " |  У объекта ровно один владелец. Копировать UnqPtr нельзя       |\n";
        std::cout << " |  (UnqPtr b = a; - ошибка компиляции), можно только переместить.|\n";
        std::cout << " |================================================================|\n";
        PrintUnqPtr("a", first);
        PrintUnqPtr("b", second);
        std::cout << " |----------------------------------------------------------------|\n";
        std::cout << " |  1. Создать объект в a          (a = MakeUnq<int>(x))          |\n";
        std::cout << " |  2. Переместить a -> b          (b = std::move(a))             |\n";
        std::cout << " |  3. Переместить b -> a          (a = std::move(b))             |\n";
        std::cout << " |  4. Изменить значение           (*a = x)                       |\n";
        std::cout << " |  5. Заменить объект в a         (a.Reset(new int(x)))          |\n";
        std::cout << " |  6. Забрать объект из a         (int* raw = a.Release())       |\n";
        std::cout << " |  7. Обменять a и b              (a.Swap(b))                    |\n";
        std::cout << " |  0. Назад                                                      |\n";
        std::cout << " |================================================================|\n";

        const int choice = ReadInt(" Выбор: ", 0, 7);
        if (choice == 0)
            break;

        if (choice == 1) {
            first = MakeUnq<int>(ReadInt(" Значение: "));
            std::cout << " Создан новый объект. Старый объект a (если был) удалён.\n";
        } else if (choice == 2) {
            second = std::move(first);
            std::cout << " Владение передано: a стал пустым, старый объект b (если был) удалён.\n";
        } else if (choice == 3) {
            first = std::move(second);
            std::cout << " Владение передано: b стал пустым.\n";
        } else if (choice == 4) {
            if (!first) {
                std::cout << " [!] a пуст - разыменовывать нечего.\n";
                continue;
            }
            *first = ReadInt(" Новое значение: ");
        } else if (choice == 5) {
            first.Reset(new int(ReadInt(" Значение нового объекта: ")));
            std::cout << " Старый объект удалён, a владеет новым.\n";
        } else if (choice == 6) {
            int* raw = first.Release();
            if (raw == nullptr) {
                std::cout << " a был пуст, Release() вернул nullptr.\n";
                continue;
            }
            std::cout << " Release() вернул сырой указатель на " << *raw
                      << ". a пуст, объект не удалён -\n"
                      << " теперь удалить его должны мы сами: delete raw;\n";
            delete raw;
        } else if (choice == 7) {
            first.Swap(second);
            std::cout << " Объекты поменялись местами (ничего не удалено и не скопировано).\n";
        }
    }
    // Здесь first и second выходят из области видимости и удаляют свои объекты
}

// ============================================================================
// 2. ShrdPtr - разделяемое владение
// ============================================================================

const int kShrdSlots = 3;

void PrintShrdSlots(const ShrdPtr<int> slots[]) {
    for (int i = 0; i < kShrdSlots; ++i) {
        std::cout << "   [" << i << "] ";
        if (slots[i]) {
            // Одинаковый адрес у разных ячеек = копии указывают на один объект
            std::cout << *slots[i] << "   владельцев: " << slots[i].UseCount()
                      << "   адрес: " << slots[i].Get() << "\n";
        } else {
            std::cout << "пусто\n";
        }
    }
}

void ShrdPtrMenu() {
    ShrdPtr<int> slots[kShrdSlots];

    while (true) {
        std::cout << "\n";
        std::cout << " |============ ShrdPtr<int>: разделяемое владение ================|\n";
        std::cout << " |  Копии ShrdPtr делят один объект и один счётчик ссылок.        |\n";
        std::cout << " |  Объект удаляется, когда умирает последний владелец.           |\n";
        std::cout << " |================================================================|\n";
        PrintShrdSlots(slots);
        std::cout << " |----------------------------------------------------------------|\n";
        std::cout << " |  1. Создать объект в ячейке     (s[i] = MakeShrd<int>(x))      |\n";
        std::cout << " |  2. Скопировать ячейку i -> j   (s[j] = s[i], счётчик + 1)     |\n";
        std::cout << " |  3. Переместить ячейку i -> j   (s[j] = std::move(s[i]))       |\n";
        std::cout << " |  4. Изменить значение           (*s[i] = x - видно всем копиям)|\n";
        std::cout << " |  5. Отпустить ячейку            (s[i].Reset(), счётчик - 1)    |\n";
        std::cout << " |  0. Назад                                                      |\n";
        std::cout << " |================================================================|\n";

        const int choice = ReadInt(" Выбор: ", 0, 5);
        if (choice == 0)
            break;

        const int last = kShrdSlots - 1;
        if (choice == 1) {
            const int i = ReadInt(" Ячейка (0-2): ", 0, last);
            slots[i] = MakeShrd<int>(ReadInt(" Значение: "));
        } else if (choice == 2 || choice == 3) {
            const int from = ReadInt(" Из ячейки (0-2): ", 0, last);
            const int to = ReadInt(" В ячейку (0-2): ", 0, last);
            if (choice == 2)
                slots[to] = slots[from];
            else
                slots[to] = std::move(slots[from]);
        } else if (choice == 4) {
            const int i = ReadInt(" Ячейка (0-2): ", 0, last);
            if (!slots[i]) {
                std::cout << " [!] Ячейка пуста.\n";
                continue;
            }
            *slots[i] = ReadInt(" Новое значение: ");
        } else if (choice == 5) {
            const int i = ReadInt(" Ячейка (0-2): ", 0, last);
            const size_t before = slots[i].UseCount();
            slots[i].Reset();
            if (before == 1)
                std::cout << " Это был последний владелец - объект удалён.\n";
        }
    }
}

// ============================================================================
// 3. MemorySpan и MsPtr - арифметика указателей с проверкой границ
// ============================================================================

// Выводит массив, текущий элемент MsPtr - в скобках > <
void PrintSpan(MemorySpan<int>& span, const MsPtr<int>& current) {
    std::cout << "   ";
    for (MsPtr<int> ptr = span.Begin(); ptr != span.End(); ++ptr) {
        if (ptr == current)
            std::cout << ">" << *ptr << "< ";
        else
            std::cout << "[" << *ptr << "] ";
    }
    std::cout << "\n   MsPtr: ";
    if (current.IsEnd())
        std::cout << "позиция End (за последним, читать нельзя)\n";
    else
        std::cout << "индекс " << current.GetIndex() << "\n";
}

void MemorySpanMenu() {
    const int size = ReadInt(" Размер MemorySpan (1-20): ", 1, 20);
    MemorySpan<int> span(static_cast<size_t>(size));
    for (int i = 0; i < size; ++i)
        *span.Locate(static_cast<size_t>(i)) = (i + 1) * 10;  // 10, 20, 30, ...
    MsPtr<int> ptr = span.Begin();

    while (true) {
        std::cout << "\n";
        std::cout << " |============ MemorySpan<int> и MsPtr<int> ======================|\n";
        std::cout << " |  MsPtr ходит только внутри своего MemorySpan: выход за границы |\n";
        std::cout << " |  бросает исключение, а не портит чужую память.                 |\n";
        std::cout << " |================================================================|\n";
        PrintSpan(span, ptr);
        std::cout << " |----------------------------------------------------------------|\n";
        std::cout << " |  1. ++ptr                       5. *ptr = x                    |\n";
        std::cout << " |  2. --ptr                       6. Прочитать ptr[k]            |\n";
        std::cout << " |  3. ptr += k                    7. ptr = span.Locate(i)        |\n";
        std::cout << " |  4. Прочитать *ptr              8. span.Get(i)  -> UnqPtr копия|\n";
        std::cout << " |  0. Назад                       9. span.Copy(i) -> ShrdPtr     |\n";
        std::cout << " |================================================================|\n";

        const int choice = ReadInt(" Выбор: ", 0, 9);
        if (choice == 0)
            break;

        // Исключения MsPtr и MemorySpan ловим и показываем: это и есть защита
        try {
            if (choice == 1) {
                ++ptr;
            } else if (choice == 2) {
                --ptr;
            } else if (choice == 3) {
                ptr += ReadInt(" Смещение k (может быть отрицательным): ");
            } else if (choice == 4) {
                const int value = *ptr;  // сначала читаем: если бросит, строка не выведется наполовину
                std::cout << " *ptr = " << value << "\n";
            } else if (choice == 5) {
                const int value = ReadInt(" Новое значение: ");
                *ptr = value;
            } else if (choice == 6) {
                const int offset = ReadInt(" k: ");
                const int value = ptr[offset];
                std::cout << " ptr[" << offset << "] = " << value << "\n";
            } else if (choice == 7) {
                ptr = span.Locate(static_cast<size_t>(ReadInt(" Индекс i: ", 0)));
            } else if (choice == 8) {
                UnqPtr<int> copy = span.Get(static_cast<size_t>(ReadInt(" Индекс i: ", 0)));
                *copy += 1000;
                std::cout << " Get вернул UnqPtr на копию. Изменили копию: " << *copy
                          << " - в массиве значение прежнее.\n";
            } else if (choice == 9) {
                ShrdPtr<int> copy = span.Copy(static_cast<size_t>(ReadInt(" Индекс i: ", 0)));
                ShrdPtr<int> another = copy;
                std::cout << " Copy вернул ShrdPtr на копию: " << *copy
                          << ", после ещё одного копирования владельцев: " << another.UseCount() << "\n";
            }
        } catch (const std::exception& error) {
            std::cout << " [!] Исключение: " << error.what() << "\n";
        }
    }
}

// ============================================================================
// Главное меню
// ============================================================================

int main() {
    std::cout << "\n |================================================================|\n";
    std::cout << " |        Лабораторная работа №1: умные указатели                 |\n";
    std::cout << " |        UnqPtr, ShrdPtr, MemorySpan, MsPtr                      |\n";
    std::cout << " |================================================================|\n";

    while (true) {
        std::cout << "\n";
        std::cout << " |=========================== МЕНЮ ===============================|\n";
        std::cout << " |  1. UnqPtr - единоличное владение                              |\n";
        std::cout << " |  2. ShrdPtr - разделяемое владение, счётчик ссылок             |\n";
        std::cout << " |  3. MemorySpan и MsPtr  безопасная арифметика указателей      |\n";
        std::cout << " |  0. Выход                                                      |\n";
        std::cout << " |================================================================|\n";

        try {
            const int choice = ReadInt(" Выбор: ", 0, 3);
            if (choice == 0)
                break;
            else if (choice == 1)
                UnqPtrMenu();
            else if (choice == 2)
                ShrdPtrMenu();
            else if (choice == 3)
                MemorySpanMenu();
        } catch (const InputEnded&) {
            std::cout << "\n Ввод завершён.\n";
            break;
        }
    }

    std::cout << "\n Конец программы\n";
    return 0;
}
