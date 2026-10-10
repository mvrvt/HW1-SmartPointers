#pragma once

#include <cstddef>  // size_t

// ============================================================================
// Счётчик выделений динамической памяти.
// В AllocationCounter.cpp глобальные operator new / operator delete заменены
// своими версиями: каждое выделение памяти (new, new[], make_shared, push_back
// в vector — всё, что идёт через operator new) увеличивает счётчики.
// Так мы точно знаем, сколько раз и сколько байт выделил каждый вариант указателя.
// ============================================================================

struct AllocationStats {
    size_t count = 0;  // сколько раз выделялась память
    size_t bytes = 0;  // сколько байт запрошено в сумме
};

// Обнулить счётчики (перед замером)
void ResetAllocationStats() noexcept;

// Сколько выделено с последнего обнуления
AllocationStats GetAllocationStats() noexcept;
