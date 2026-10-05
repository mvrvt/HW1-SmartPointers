#include <gtest/gtest.h>

#include <cstddef>
#include <string>
#include <type_traits>
#include <utility>

#include "pointers/UnqPtr.hpp"
#include "test_helpers.hpp"

// Узел односвязного списка (для теста head = std::move(head->next))
struct ListNode {
    explicit ListNode(int value) : tracker(value) {}

    InstanceTracker tracker;
    UnqPtr<ListNode> next;
};

// Функция, которая возвращает пустой указатель через return nullptr
UnqPtr<int> MakeEmptyUnqPtr() {
    return nullptr;
}

// ============================================================================
// 0. Проверки во время компиляции
// is_constructible_v<A, B> — можно ли написать  A x(b);   (явно)
// is_convertible_v<B, A>   — можно ли написать  A x = b;  (неявно)
// UnqPtr<X>&&  — значит "передали через std::move"
// UnqPtr<X>&   — значит "передали обычную переменную" (это была бы копия)
// ============================================================================

// Копировать нельзя
static_assert(!std::is_copy_constructible_v<UnqPtr<int>>);
static_assert(!std::is_copy_assignable_v<UnqPtr<int>>);
static_assert(!std::is_copy_constructible_v<UnqPtr<int[]>>);

// Перемещать можно, и перемещение не бросает исключений (noexcept)
static_assert(std::is_nothrow_move_constructible_v<UnqPtr<int>>);
static_assert(std::is_nothrow_move_assignable_v<UnqPtr<int>>);
static_assert(std::is_nothrow_move_constructible_v<UnqPtr<int[]>>);

// Размер как у обычного указателя: никаких лишних затрат памяти
static_assert(sizeof(UnqPtr<int>) == sizeof(int*));
static_assert(sizeof(UnqPtr<int[]>) == sizeof(int*));

// explicit: UnqPtr<int> p(raw) можно, UnqPtr<int> p = raw нельзя
static_assert(std::is_constructible_v<UnqPtr<int>, int*>);
static_assert(!std::is_convertible_v<int*, UnqPtr<int>>);

// А из nullptr можно и через =
static_assert(std::is_convertible_v<std::nullptr_t, UnqPtr<int>>);

// explicit operator bool: if (p) можно, bool b = p нельзя
static_assert(std::is_constructible_v<bool, UnqPtr<int>>);
static_assert(!std::is_convertible_v<UnqPtr<int>, bool>);

// Подтипизация: Derived -> Base можно, всё остальное нельзя
static_assert(std::is_constructible_v<UnqPtr<BaseEntity>, UnqPtr<DerivedEntity>&&>);
static_assert(std::is_assignable_v<UnqPtr<BaseEntity>&, UnqPtr<DerivedEntity>&&>);
static_assert(!std::is_constructible_v<UnqPtr<DerivedEntity>, UnqPtr<BaseEntity>&&>);     // downcast
static_assert(!std::is_assignable_v<UnqPtr<DerivedEntity>&, UnqPtr<BaseEntity>&&>);
static_assert(!std::is_constructible_v<UnqPtr<UnrelatedEntity>, UnqPtr<BaseEntity>&&>);  // не родственники
static_assert(!std::is_constructible_v<UnqPtr<BaseEntity>, UnqPtr<PrivateDerivedEntity>&&>);
static_assert(!std::is_constructible_v<UnqPtr<BaseEntity>, UnqPtr<DerivedEntity>&>);      // без move

// const: добавить можно, убрать нельзя
static_assert(std::is_constructible_v<UnqPtr<const int>, UnqPtr<int>&&>);
static_assert(!std::is_constructible_v<UnqPtr<int>, UnqPtr<const int>&&>);

// Массивы: Derived* в UnqPtr<Base[]> нельзя, добавить const можно
static_assert(!std::is_constructible_v<UnqPtr<BaseEntity[]>, DerivedEntity*>);
static_assert(!std::is_constructible_v<UnqPtr<BaseEntity[]>, UnqPtr<DerivedEntity[]>&&>);
static_assert(std::is_constructible_v<UnqPtr<const int[]>, UnqPtr<int[]>&&>);
static_assert(!std::is_constructible_v<UnqPtr<int>, UnqPtr<int[]>&&>);  // массив -> не массив

// Псевдонимы типов: у массива ElementType — тип элемента, без []
static_assert(std::is_same_v<UnqPtr<int>::ElementType, int>);
static_assert(std::is_same_v<UnqPtr<int[]>::ElementType, int>);
static_assert(std::is_same_v<UnqPtr<int[]>::Pointer, int*>);

// ============================================================================
// 1. Создание и пустой указатель
// ============================================================================

TEST(UnqPtrTest, DefaultConstructorIsEmpty) {
    UnqPtr<int> ptr;
    EXPECT_EQ(ptr.Get(), nullptr);
    EXPECT_FALSE(ptr);
    EXPECT_TRUE(ptr == nullptr);
    EXPECT_TRUE(nullptr == ptr);
}

TEST(UnqPtrTest, CreateFromNullptr) {
    UnqPtr<int> ptr = nullptr;
    EXPECT_FALSE(ptr);

    UnqPtr<int> returned = MakeEmptyUnqPtr();
    EXPECT_FALSE(returned);
}

TEST(UnqPtrTest, ObjectIsDeletedWhenPointerDies) {
    InstanceTracker::alive_count = 0;
    {
        UnqPtr<InstanceTracker> ptr(new InstanceTracker(42));
        EXPECT_TRUE(ptr);
        EXPECT_EQ(ptr->value, 42);
        EXPECT_EQ((*ptr).value, 42);
        EXPECT_EQ(InstanceTracker::alive_count, 1);
    }
    EXPECT_EQ(InstanceTracker::alive_count, 0);
}

TEST(UnqPtrTest, ObjectCanBeChangedThroughPointer) {
    UnqPtr<int> ptr(new int(1));
    *ptr = 10;
    EXPECT_EQ(*ptr, 10);

    // const UnqPtr запрещает менять сам указатель, но не объект (как int* const)
    const UnqPtr<InstanceTracker> const_ptr(new InstanceTracker(1));
    const_ptr->value = 7;
    EXPECT_EQ(const_ptr->value, 7);
}

// ============================================================================
// 2. Перемещение
// ============================================================================

TEST(UnqPtrTest, MoveConstructorTakesObject) {
    InstanceTracker::alive_count = 0;
    {
        UnqPtr<InstanceTracker> source(new InstanceTracker(100));
        InstanceTracker* object = source.Get();

        UnqPtr<InstanceTracker> destination = std::move(source);
        EXPECT_EQ(source.Get(), nullptr);
        EXPECT_EQ(destination.Get(), object);  // тот же самый объект, а не копия
        EXPECT_EQ(InstanceTracker::alive_count, 1);
    }
    EXPECT_EQ(InstanceTracker::alive_count, 0);
}

TEST(UnqPtrTest, MoveAssignmentDeletesOldObject) {
    InstanceTracker::alive_count = 0;
    {
        UnqPtr<InstanceTracker> first(new InstanceTracker(1));
        UnqPtr<InstanceTracker> second(new InstanceTracker(2));
        EXPECT_EQ(InstanceTracker::alive_count, 2);

        first = std::move(second);
        EXPECT_EQ(InstanceTracker::alive_count, 1);  // объект 1 удалён
        EXPECT_EQ(first->value, 2);
        EXPECT_FALSE(second);
    }
    EXPECT_EQ(InstanceTracker::alive_count, 0);
}

TEST(UnqPtrTest, MoveAssignmentIntoEmptyPointer) {
    InstanceTracker::alive_count = 0;
    {
        UnqPtr<InstanceTracker> empty;
        UnqPtr<InstanceTracker> full(new InstanceTracker(5));

        empty = std::move(full);
        EXPECT_EQ(empty->value, 5);
        EXPECT_FALSE(full);
        EXPECT_EQ(InstanceTracker::alive_count, 1);
    }
    EXPECT_EQ(InstanceTracker::alive_count, 0);
}

TEST(UnqPtrTest, SelfMoveAssignmentKeepsObject) {
    InstanceTracker::alive_count = 0;
    {
        UnqPtr<InstanceTracker> ptr(new InstanceTracker(9));
        UnqPtr<InstanceTracker>& same_ptr = ptr;  // через ссылку, иначе компилятор ругается

        ptr = std::move(same_ptr);
        EXPECT_TRUE(ptr);
        EXPECT_EQ(ptr->value, 9);
        EXPECT_EQ(InstanceTracker::alive_count, 1);
    }
    EXPECT_EQ(InstanceTracker::alive_count, 0);
}

// head->next лежит ВНУТРИ первого узла, которым владеет head
TEST(UnqPtrTest, RemoveFirstNodeOfList) {
    InstanceTracker::alive_count = 0;
    {
        UnqPtr<ListNode> head(new ListNode(1));
        head->next = UnqPtr<ListNode>(new ListNode(2));
        head->next->next = UnqPtr<ListNode>(new ListNode(3));
        EXPECT_EQ(InstanceTracker::alive_count, 3);

        head = std::move(head->next);  // удаляем первый узел

        EXPECT_EQ(InstanceTracker::alive_count, 2);
        EXPECT_EQ(head->tracker.value, 2);
        EXPECT_EQ(head->next->tracker.value, 3);
    }
    EXPECT_EQ(InstanceTracker::alive_count, 0);
}

TEST(UnqPtrTest, AssignNullptrDeletesObject) {
    InstanceTracker::alive_count = 0;
    UnqPtr<InstanceTracker> ptr(new InstanceTracker(1));

    ptr = nullptr;
    EXPECT_FALSE(ptr);
    EXPECT_EQ(InstanceTracker::alive_count, 0);
}

// ============================================================================
// 3. Release, Reset, Swap, сравнения
// ============================================================================

TEST(UnqPtrTest, ReleaseDoesNotDeleteObject) {
    InstanceTracker::alive_count = 0;
    UnqPtr<InstanceTracker> ptr(new InstanceTracker(10));
    InstanceTracker* object = ptr.Get();

    InstanceTracker* raw = ptr.Release();
    EXPECT_EQ(raw, object);
    EXPECT_FALSE(ptr);
    EXPECT_EQ(InstanceTracker::alive_count, 1);  // объект жив

    delete raw;  // теперь удалять должны мы сами
    EXPECT_EQ(InstanceTracker::alive_count, 0);
}

TEST(UnqPtrTest, ReleaseOfEmptyPointerReturnsNullptr) {
    UnqPtr<int> ptr;
    EXPECT_EQ(ptr.Release(), nullptr);
}

TEST(UnqPtrTest, ResetReplacesAndDeletesObject) {
    InstanceTracker::alive_count = 0;
    UnqPtr<InstanceTracker> ptr(new InstanceTracker(1));

    ptr.Reset(new InstanceTracker(2));
    EXPECT_EQ(ptr->value, 2);
    EXPECT_EQ(InstanceTracker::alive_count, 1);  // старый объект удалён

    ptr.Reset();
    EXPECT_FALSE(ptr);
    EXPECT_EQ(InstanceTracker::alive_count, 0);
}

TEST(UnqPtrTest, ResetWithSamePointerKeepsObject) {
    InstanceTracker::alive_count = 0;
    UnqPtr<InstanceTracker> ptr(new InstanceTracker(5));

    ptr.Reset(ptr.Get());
    EXPECT_TRUE(ptr);
    EXPECT_EQ(ptr->value, 5);
    EXPECT_EQ(InstanceTracker::alive_count, 1);
}

TEST(UnqPtrTest, ResetOfEmptyPointer) {
    UnqPtr<int> ptr;
    ptr.Reset();
    EXPECT_FALSE(ptr);

    ptr.Reset(new int(3));
    EXPECT_EQ(*ptr, 3);
}

TEST(UnqPtrTest, SwapExchangesObjects) {
    InstanceTracker::alive_count = 0;
    {
        UnqPtr<InstanceTracker> first(new InstanceTracker(1));
        UnqPtr<InstanceTracker> second(new InstanceTracker(2));

        first.Swap(second);
        EXPECT_EQ(first->value, 2);
        EXPECT_EQ(second->value, 1);

        using std::swap;
        swap(first, second);  // ADL найдёт наш friend swap
        EXPECT_EQ(first->value, 1);
        EXPECT_EQ(second->value, 2);

        EXPECT_EQ(InstanceTracker::alive_count, 2);  // ничего не удалено
    }
    EXPECT_EQ(InstanceTracker::alive_count, 0);
}

TEST(UnqPtrTest, ComparisonComparesAddresses) {
    UnqPtr<int> first(new int(1));
    UnqPtr<int> second(new int(1));
    UnqPtr<int> empty1;
    UnqPtr<int> empty2;

    EXPECT_TRUE(first == first);
    EXPECT_TRUE(first != second);  // значения равны, но объекты разные
    EXPECT_TRUE(empty1 == empty2);
    EXPECT_TRUE(first != nullptr);
}

// ============================================================================
// 4. Подтипизация
// ============================================================================

TEST(UnqPtrTest, DerivedMovesIntoBase) {
    BaseEntity::alive_entities = 0;
    {
        UnqPtr<DerivedEntity> derived_ptr(new DerivedEntity());
        UnqPtr<BaseEntity> base_ptr = std::move(derived_ptr);

        EXPECT_FALSE(derived_ptr);
        EXPECT_EQ(base_ptr->GetName(), "DerivedEntity");  // virtual работает
        EXPECT_EQ(BaseEntity::alive_entities, 1);
    }
    EXPECT_EQ(BaseEntity::alive_entities, 0);  // виртуальный деструктор
}

TEST(UnqPtrTest, DerivedAssignedToBaseDeletesOldObject) {
    BaseEntity::alive_entities = 0;
    {
        UnqPtr<BaseEntity> base_ptr(new BaseEntity());
        UnqPtr<DerivedEntity> derived_ptr(new DerivedEntity());
        EXPECT_EQ(BaseEntity::alive_entities, 2);

        base_ptr = std::move(derived_ptr);
        EXPECT_EQ(BaseEntity::alive_entities, 1);
        EXPECT_EQ(base_ptr->GetName(), "DerivedEntity");
        EXPECT_FALSE(derived_ptr);
    }
    EXPECT_EQ(BaseEntity::alive_entities, 0);
}

TEST(UnqPtrTest, BasePointerFromRawDerived) {
    BaseEntity::alive_entities = 0;
    {
        UnqPtr<BaseEntity> ptr(new DerivedEntity());
        EXPECT_EQ(ptr->GetName(), "DerivedEntity");
    }
    EXPECT_EQ(BaseEntity::alive_entities, 0);
}

TEST(UnqPtrTest, MoveIntoConstPointer) {
    UnqPtr<int> ptr(new int(8));
    UnqPtr<const int> const_ptr = std::move(ptr);
    EXPECT_FALSE(ptr);
    EXPECT_EQ(*const_ptr, 8);
}

// ============================================================================
// 5. MakeUnq
// ============================================================================

TEST(UnqPtrTest, MakeUnqCreatesObject) {
    InstanceTracker::alive_count = 0;
    {
        UnqPtr<InstanceTracker> ptr = MakeUnq<InstanceTracker>(42);
        EXPECT_EQ(ptr->value, 42);
        EXPECT_EQ(InstanceTracker::alive_count, 1);
    }
    EXPECT_EQ(InstanceTracker::alive_count, 0);
}

TEST(UnqPtrTest, MakeUnqPassesSeveralArguments) {
    UnqPtr<std::string> ptr = MakeUnq<std::string>(3, 'a');  // как std::string(3, 'a')
    EXPECT_EQ(*ptr, "aaa");
}

TEST(UnqPtrTest, MakeUnqCopiesVariable) {
    std::string text = "hello";
    UnqPtr<std::string> ptr = MakeUnq<std::string>(text);
    EXPECT_EQ(*ptr, "hello");
    EXPECT_EQ(text, "hello");  // переменная не тронута
}

TEST(UnqPtrTest, MakeUnqWithoutArgumentsGivesZero) {
    UnqPtr<int> ptr = MakeUnq<int>();
    EXPECT_EQ(*ptr, 0);
}

// ============================================================================
// 6. UnqPtr<T[]> — массивы
// ============================================================================

TEST(UnqPtrArrayTest, DefaultAndNullptrAreEmpty) {
    UnqPtr<int[]> by_default;
    UnqPtr<int[]> from_null = nullptr;
    EXPECT_FALSE(by_default);
    EXPECT_FALSE(from_null);
    EXPECT_TRUE(by_default == nullptr);
}

TEST(UnqPtrArrayTest, IndexingReadAndWrite) {
    UnqPtr<int[]> array(new int[5]());
    for (size_t i = 0; i < 5; ++i)
        array[i] = static_cast<int>((i + 1) * 10);

    EXPECT_EQ(array[0], 10);
    EXPECT_EQ(array[2], 30);
    EXPECT_EQ(array[4], 50);
}

TEST(UnqPtrArrayTest, DestructorDeletesEveryElement) {
    InstanceTracker::alive_count = 0;
    {
        UnqPtr<InstanceTracker[]> array(new InstanceTracker[4]());
        EXPECT_EQ(InstanceTracker::alive_count, 4);
    }
    EXPECT_EQ(InstanceTracker::alive_count, 0);  // delete[] вызвал все 4 деструктора
}

TEST(UnqPtrArrayTest, MoveTakesArrayAndDeletesOld) {
    InstanceTracker::alive_count = 0;
    {
        UnqPtr<InstanceTracker[]> source(new InstanceTracker[3]());
        UnqPtr<InstanceTracker[]> destination = std::move(source);
        EXPECT_FALSE(source);
        EXPECT_TRUE(destination);
        EXPECT_EQ(InstanceTracker::alive_count, 3);

        UnqPtr<InstanceTracker[]> other(new InstanceTracker[2]());
        destination = std::move(other);  // старый массив из 3 элементов удалён
        EXPECT_EQ(InstanceTracker::alive_count, 2);
    }
    EXPECT_EQ(InstanceTracker::alive_count, 0);
}

TEST(UnqPtrArrayTest, ReleaseAndReset) {
    InstanceTracker::alive_count = 0;
    UnqPtr<InstanceTracker[]> array(new InstanceTracker[3]());

    array.Reset(new InstanceTracker[2]());
    EXPECT_EQ(InstanceTracker::alive_count, 2);

    InstanceTracker* raw = array.Release();
    EXPECT_FALSE(array);
    EXPECT_EQ(InstanceTracker::alive_count, 2);
    delete[] raw;
    EXPECT_EQ(InstanceTracker::alive_count, 0);

    array.Reset(new InstanceTracker[1]());
    array.Reset();
    EXPECT_EQ(InstanceTracker::alive_count, 0);
}

TEST(UnqPtrArrayTest, AssignNullptrDeletesArray) {
    InstanceTracker::alive_count = 0;
    UnqPtr<InstanceTracker[]> array(new InstanceTracker[3]());

    array = nullptr;
    EXPECT_FALSE(array);
    EXPECT_EQ(InstanceTracker::alive_count, 0);
}

TEST(UnqPtrArrayTest, SwapExchangesArrays) {
    UnqPtr<int[]> first(new int[1]{1});
    UnqPtr<int[]> second(new int[1]{2});

    using std::swap;
    swap(first, second);
    EXPECT_EQ(first[0], 2);
    EXPECT_EQ(second[0], 1);
}

TEST(UnqPtrArrayTest, MoveIntoConstArray) {
    UnqPtr<int[]> array(new int[2]{4, 5});
    UnqPtr<const int[]> const_array = std::move(array);
    EXPECT_FALSE(array);
    EXPECT_EQ(const_array[1], 5);
}

TEST(UnqPtrArrayTest, MakeUnqArrayIsFilledWithZeros) {
    UnqPtr<int[]> numbers = MakeUnq<int[]>(5);
    for (size_t i = 0; i < 5; ++i)
        EXPECT_EQ(numbers[i], 0);

    InstanceTracker::alive_count = 0;
    {
        UnqPtr<InstanceTracker[]> objects = MakeUnq<InstanceTracker[]>(3);
        EXPECT_EQ(InstanceTracker::alive_count, 3);
    }
    EXPECT_EQ(InstanceTracker::alive_count, 0);
}

// ============================================================================
// 7. Death-тесты: assert на пустом указателе.
// Работают только в Debug: в Release (NDEBUG) assert'ов нет.
// ============================================================================

#ifndef NDEBUG
TEST(UnqPtrDeathTest, DereferencingEmptyPointerCrashes) {
    UnqPtr<int> ptr;
    EXPECT_DEATH((void)*ptr, "Dereferencing empty UnqPtr");
}

TEST(UnqPtrDeathTest, IndexingEmptyArrayCrashes) {
    UnqPtr<int[]> array;
    EXPECT_DEATH((void)array[0], "Indexing empty UnqPtr");
}
#endif
