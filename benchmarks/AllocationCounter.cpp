#include "AllocationCounter.hpp"

#include <cstdlib>  // std::malloc, std::free
#include <new>      // std::bad_alloc, std::nothrow_t

static AllocationStats stats;

void ResetAllocationStats() noexcept {
    stats = AllocationStats();
}

AllocationStats GetAllocationStats() noexcept {
    return stats;
}

// Выделение памяти со счётом. malloc(0) может вернуть nullptr, поэтому минимум 1 байт.
static void* CountedAllocate(std::size_t size) noexcept {
    ++stats.count;
    stats.bytes += size;
    return std::malloc(size == 0 ? 1 : size);
}

// Заменяем ВСЕ варианты new и delete: иначе память, выделенная стандартной
// версией, освобождалась бы нашей (или наоборот), а это ошибка.
void* operator new(std::size_t size) {
    void* memory = CountedAllocate(size);
    if (memory == nullptr)
        throw std::bad_alloc();
    return memory;
}

void* operator new[](std::size_t size) {
    void* memory = CountedAllocate(size);
    if (memory == nullptr)
        throw std::bad_alloc();
    return memory;
}

void* operator new(std::size_t size, const std::nothrow_t&) noexcept {
    return CountedAllocate(size);
}

void* operator new[](std::size_t size, const std::nothrow_t&) noexcept {
    return CountedAllocate(size);
}

void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete[](void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }
void operator delete[](void* memory, std::size_t) noexcept { std::free(memory); }
void operator delete(void* memory, const std::nothrow_t&) noexcept { std::free(memory); }
void operator delete[](void* memory, const std::nothrow_t&) noexcept { std::free(memory); }
