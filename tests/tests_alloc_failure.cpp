#include <gtest/gtest.h>

#include <cstddef>
#include <cstdlib>
#include <new>
#include <utility>

#include "pointers/ShrdPtr.hpp"
#include "pointers/UnqPtr.hpp"
#include "test_helpers.hpp"

// ============================================================================
// Тесты поведения ShrdPtr при нехватке памяти.
//
// Единственное место в ShrdPtr, где может вылететь исключение, — new size_t(1)
// для счётчика. Проверяем, что в этот момент объект не теряется (нет утечки)
// и ничего не удаляется дважды.
//
// Чтобы заставить new бросить std::bad_alloc, подменяем глобальные
// operator new / operator delete: если поднят флаг fail_next_allocation,
// СЛЕДУЮЩЕЕ выделение памяти бросит исключение.
//
// Подмена действует на всю программу, поэтому эти тесты собраны в ОТДЕЛЬНУЮ
// программу alloc_failure_tests: в основной unit_tests санитайзеры и Valgrind
// продолжают ловить путаницу delete / delete[].
// ============================================================================

static bool fail_next_allocation = false;

static void* Allocate(std::size_t size) {
    if (fail_next_allocation) {
        fail_next_allocation = false;
        throw std::bad_alloc();
    }
    void* memory = std::malloc(size == 0 ? 1 : size);
    if (memory == nullptr)
        throw std::bad_alloc();
    return memory;
}

// Все варианты new берут память через malloc, все варианты delete отдают через free —
// иначе память, выделенная одной парой функций, освобождалась бы другой.
void* operator new(std::size_t size) { return Allocate(size); }
void* operator new[](std::size_t size) { return Allocate(size); }
void* operator new(std::size_t size, const std::nothrow_t&) noexcept { return std::malloc(size == 0 ? 1 : size); }
void* operator new[](std::size_t size, const std::nothrow_t&) noexcept { return std::malloc(size == 0 ? 1 : size); }

void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete[](void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }
void operator delete[](void* memory, std::size_t) noexcept { std::free(memory); }
void operator delete(void* memory, const std::nothrow_t&) noexcept { std::free(memory); }
void operator delete[](void* memory, const std::nothrow_t&) noexcept { std::free(memory); }


TEST(ShrdPtrTest, FailedCounterAllocationLeavesObjectInUnqPtr) {
    InstanceTracker::alive_count = 0;
    UnqPtr<InstanceTracker> owner(new InstanceTracker(1));

    bool thrown = false;
    fail_next_allocation = true;  // следующий new — это new size_t для счётчика
    try {
        ShrdPtr<InstanceTracker> shared(std::move(owner));
    } catch (const std::bad_alloc&) {
        thrown = true;
    }

    EXPECT_TRUE(thrown);
    EXPECT_TRUE(owner);  // объект не потерян: им всё ещё владеет UnqPtr
    EXPECT_EQ(owner->value, 1);
    EXPECT_EQ(InstanceTracker::alive_count, 1);
}

TEST(ShrdPtrTest, FailedCounterAllocationFromRawPointerDoesNotLeak) {
    InstanceTracker::alive_count = 0;
    InstanceTracker* raw = new InstanceTracker(1);

    bool thrown = false;
    fail_next_allocation = true;
    try {
        ShrdPtr<InstanceTracker> shared(raw);
    } catch (const std::bad_alloc&) {
        thrown = true;
    }

    EXPECT_TRUE(thrown);
    EXPECT_EQ(InstanceTracker::alive_count, 0);  // временный UnqPtr удалил объект
}

TEST(ShrdPtrTest, FailedResetKeepsOldObject) {
    InstanceTracker::alive_count = 0;
    ShrdPtr<InstanceTracker> shared = MakeShrd<InstanceTracker>(1);
    InstanceTracker* raw = new InstanceTracker(2);

    bool thrown = false;
    fail_next_allocation = true;
    try {
        shared.Reset(raw);
    } catch (const std::bad_alloc&) {
        thrown = true;
    }

    EXPECT_TRUE(thrown);
    EXPECT_EQ(shared->value, 1);  // shared не изменился
    EXPECT_EQ(shared.UseCount(), 1u);
    EXPECT_EQ(InstanceTracker::alive_count, 1);  // новый объект удалён, старый жив
}

TEST(ShrdPtrArrayTest, FailedCounterAllocationDoesNotLeak) {
    InstanceTracker::alive_count = 0;
    InstanceTracker* raw = new InstanceTracker[3]();

    bool thrown = false;
    fail_next_allocation = true;
    try {
        ShrdPtr<InstanceTracker[]> shared(raw);
    } catch (const std::bad_alloc&) {
        thrown = true;
    }

    EXPECT_TRUE(thrown);
    EXPECT_EQ(InstanceTracker::alive_count, 0);
}
