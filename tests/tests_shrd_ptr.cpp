#include <gtest/gtest.h>

#include <cstddef>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "pointers/ShrdPtr.hpp"
#include "pointers/UnqPtr.hpp"
#include "test_helpers.hpp"

// Узел односвязного списка на ShrdPtr (для тестов head = head->next).
// Имя отличается от ListNode из tests_unq_ptr.cpp: два разных класса
// с одним именем в одной программе — нарушение правила одного определения (ODR).
struct SharedListNode {
    explicit SharedListNode(int value) : tracker(value) {}

    InstanceTracker tracker;
    ShrdPtr<SharedListNode> next;
};

// Функция, которая возвращает пустой указатель через return nullptr
ShrdPtr<int> MakeEmptyShrdPtr() {
    return nullptr;
}

// ============================================================================
// 0. Проверки во время компиляции
// is_constructible_v<A, B> — можно ли написать  A x(b);   (явно)
// is_convertible_v<B, A>   — можно ли написать  A x = b;  (неявно)
// is_assignable_v<A&, B>   — можно ли написать  a = b;
// X&&        — "передали через std::move"
// const X&   — "передали обычную переменную" (копия) //TODO: разве копия? Копия это когда мы копируем элемент в память и работаем с копией, а тут у нас передача элемента по ссылке
// ============================================================================

// Копировать и перемещать можно, и ничего из этого не бросает исключений
static_assert(std::is_nothrow_copy_constructible_v<ShrdPtr<int>>);
static_assert(std::is_nothrow_move_constructible_v<ShrdPtr<int>>);
static_assert(std::is_nothrow_copy_assignable_v<ShrdPtr<int>>);
static_assert(std::is_nothrow_move_assignable_v<ShrdPtr<int>>);
static_assert(std::is_nothrow_copy_constructible_v<ShrdPtr<int[]>>);
static_assert(std::is_nothrow_move_assignable_v<ShrdPtr<int[]>>);

// Размер — два указателя: адрес объекта и адрес счётчика (как у std::shared_ptr)
static_assert(sizeof(ShrdPtr<int>) == 2 * sizeof(int*));
static_assert(sizeof(ShrdPtr<int[]>) == 2 * sizeof(int*));

// explicit: ShrdPtr<int> p(raw) можно, ShrdPtr<int> p = raw нельзя
static_assert(std::is_constructible_v<ShrdPtr<int>, int*>);
static_assert(!std::is_convertible_v<int*, ShrdPtr<int>>);
static_assert(std::is_constructible_v<ShrdPtr<int[]>, int*>);
static_assert(!std::is_convertible_v<int*, ShrdPtr<int[]>>);

// Из nullptr можно и через =
static_assert(std::is_convertible_v<std::nullptr_t, ShrdPtr<int>>);
static_assert(std::is_convertible_v<std::nullptr_t, ShrdPtr<int[]>>);

// Из UnqPtr: только через std::move (UnqPtr нельзя копировать)
static_assert(std::is_convertible_v<UnqPtr<int>&&, ShrdPtr<int>>);
static_assert(!std::is_constructible_v<ShrdPtr<int>, UnqPtr<int>&>);

// explicit operator bool: if (p) можно, bool b = p нельзя
static_assert(std::is_constructible_v<bool, ShrdPtr<int>>);
static_assert(!std::is_convertible_v<ShrdPtr<int>, bool>);

// Подтипизация: Derived -> Base можно (копией и перемещением)
static_assert(std::is_convertible_v<const ShrdPtr<DerivedEntity>&, ShrdPtr<BaseEntity>>);
static_assert(std::is_convertible_v<ShrdPtr<DerivedEntity>&&, ShrdPtr<BaseEntity>>);
static_assert(std::is_assignable_v<ShrdPtr<BaseEntity>&, const ShrdPtr<DerivedEntity>&>);
static_assert(std::is_assignable_v<ShrdPtr<BaseEntity>&, ShrdPtr<DerivedEntity>&&>);
static_assert(std::is_convertible_v<UnqPtr<DerivedEntity>&&, ShrdPtr<BaseEntity>>);
static_assert(std::is_constructible_v<ShrdPtr<BaseEntity>, DerivedEntity*>);

// ...а всё остальное нельзя
static_assert(!std::is_constructible_v<ShrdPtr<DerivedEntity>, const ShrdPtr<BaseEntity>&>);  // downcast
static_assert(!std::is_assignable_v<ShrdPtr<DerivedEntity>&, const ShrdPtr<BaseEntity>&>);
static_assert(!std::is_constructible_v<ShrdPtr<DerivedEntity>, UnqPtr<BaseEntity>&&>);
static_assert(!std::is_constructible_v<ShrdPtr<UnrelatedEntity>, const ShrdPtr<BaseEntity>&>);  // не родственники
static_assert(!std::is_constructible_v<ShrdPtr<BaseEntity>, const ShrdPtr<PrivateDerivedEntity>&>);

// const: добавить можно, убрать нельзя
static_assert(std::is_constructible_v<ShrdPtr<const int>, const ShrdPtr<int>&>);
static_assert(!std::is_constructible_v<ShrdPtr<int>, const ShrdPtr<const int>&>);

// Массивы: подтипизации нет, добавить const можно
static_assert(!std::is_constructible_v<ShrdPtr<BaseEntity[]>, DerivedEntity*>);
static_assert(!std::is_constructible_v<ShrdPtr<BaseEntity[]>, const ShrdPtr<DerivedEntity[]>&>);
static_assert(!std::is_constructible_v<ShrdPtr<BaseEntity[]>, UnqPtr<DerivedEntity[]>&&>);
static_assert(std::is_constructible_v<ShrdPtr<const int[]>, const ShrdPtr<int[]>&>);
static_assert(std::is_constructible_v<ShrdPtr<const int[]>, int*>);
static_assert(!std::is_constructible_v<ShrdPtr<int>, const ShrdPtr<int[]>&>);  // массив -> не массив
static_assert(!std::is_constructible_v<ShrdPtr<int[]>, const ShrdPtr<int>&>);  // не массив -> массив

// Псевдонимы типов: у массива ElementType — тип элемента, без []
static_assert(std::is_same_v<ShrdPtr<int>::ElementType, int>);
static_assert(std::is_same_v<ShrdPtr<int>::Pointer, int*>);
static_assert(std::is_same_v<ShrdPtr<int[]>::ElementType, int>);
static_assert(std::is_same_v<ShrdPtr<int[]>::Pointer, int*>);

// ============================================================================
// 1. Создание
// ============================================================================

TEST(ShrdPtrTest, DefaultConstructorIsEmpty) {
    ShrdPtr<int> ptr;
    EXPECT_EQ(ptr.Get(), nullptr);
    EXPECT_FALSE(ptr);
    EXPECT_EQ(ptr.UseCount(), 0u);  // у пустого указателя нет счётчика
    EXPECT_TRUE(ptr == nullptr);
    EXPECT_TRUE(nullptr == ptr);
}

TEST(ShrdPtrTest, CreateFromNullptr) {
    ShrdPtr<int> ptr = nullptr;
    EXPECT_FALSE(ptr);
    EXPECT_EQ(ptr.UseCount(), 0u);

    ShrdPtr<int> returned = MakeEmptyShrdPtr();
    EXPECT_FALSE(returned);
}

TEST(ShrdPtrTest, CreateFromUnqPtrTakesObject) {
    InstanceTracker::alive_count = 0;
    {
        UnqPtr<InstanceTracker> owner(new InstanceTracker(42));
        InstanceTracker* object = owner.Get();

        ShrdPtr<InstanceTracker> shared = std::move(owner);
        EXPECT_FALSE(owner);                // UnqPtr отдал объект
        EXPECT_EQ(shared.Get(), object);    // тот же объект, не копия
        EXPECT_EQ(shared.UseCount(), 1u);
        EXPECT_EQ(shared->value, 42);
        EXPECT_EQ(InstanceTracker::alive_count, 1);
    }
    EXPECT_EQ(InstanceTracker::alive_count, 0);
}

TEST(ShrdPtrTest, CreateFromEmptyUnqPtrGivesEmptyPointer) {
    UnqPtr<int> empty_owner;
    ShrdPtr<int> shared = std::move(empty_owner);
    EXPECT_FALSE(shared);
    EXPECT_EQ(shared.UseCount(), 0u);  // счётчик не выделялся
}

TEST(ShrdPtrTest, CreateFromRawPointer) {
    InstanceTracker::alive_count = 0;
    {
        ShrdPtr<InstanceTracker> shared(new InstanceTracker(7));
        EXPECT_EQ(shared->value, 7);
        EXPECT_EQ(shared.UseCount(), 1u);
    }
    EXPECT_EQ(InstanceTracker::alive_count, 0);
}

TEST(ShrdPtrTest, CreateFromNullRawPointerGivesEmptyPointer) {
    int* raw = nullptr;
    ShrdPtr<int> shared(raw);
    EXPECT_FALSE(shared);
    EXPECT_EQ(shared.UseCount(), 0u);
}

TEST(ShrdPtrTest, ObjectCanBeChangedThroughPointer) {
    ShrdPtr<InstanceTracker> shared = MakeShrd<InstanceTracker>(1);
    (*shared).value = 5;
    EXPECT_EQ(shared->value, 5);
    shared->value = 10;
    EXPECT_EQ((*shared).value, 10);
}

// ============================================================================
// 2. Копирование: общий объект и счётчик ссылок
// ============================================================================

TEST(ShrdPtrTest, CopyIncreasesCounter) {
    InstanceTracker::alive_count = 0;
    {
        ShrdPtr<InstanceTracker> first = MakeShrd<InstanceTracker>(1);
        ShrdPtr<InstanceTracker> second = first;

        EXPECT_EQ(first.UseCount(), 2u);
        EXPECT_EQ(second.UseCount(), 2u);
        EXPECT_EQ(first.Get(), second.Get());  // один объект на двоих
        EXPECT_EQ(InstanceTracker::alive_count, 1);
    }
    EXPECT_EQ(InstanceTracker::alive_count, 0);
}

TEST(ShrdPtrTest, CopiesSeeTheSameObject) {
    ShrdPtr<InstanceTracker> first = MakeShrd<InstanceTracker>(1);
    ShrdPtr<InstanceTracker> second = first;

    first->value = 99;
    EXPECT_EQ(second->value, 99);  // изменение через одну копию видно через другую
}

TEST(ShrdPtrTest, ObjectLivesUntilLastOwnerDies) {
    InstanceTracker::alive_count = 0;
    {
        ShrdPtr<InstanceTracker> outer = MakeShrd<InstanceTracker>(1);
        {
            ShrdPtr<InstanceTracker> inner1 = outer;
            ShrdPtr<InstanceTracker> inner2 = inner1;
            EXPECT_EQ(outer.UseCount(), 3u);
        }
        // inner1 и inner2 умерли, но объект жив: outer ещё владеет им
        EXPECT_EQ(outer.UseCount(), 1u);
        EXPECT_EQ(InstanceTracker::alive_count, 1);
        EXPECT_EQ(outer->value, 1);
    }
    EXPECT_EQ(InstanceTracker::alive_count, 0);  // последний владелец удалил объект
}

TEST(ShrdPtrTest, CopyOfEmptyPointerIsEmpty) {
    ShrdPtr<int> empty;
    ShrdPtr<int> copy = empty;
    EXPECT_FALSE(copy);
    EXPECT_EQ(copy.UseCount(), 0u);
}

TEST(ShrdPtrTest, CopiesInsideVector) {
    InstanceTracker::alive_count = 0;
    {
        ShrdPtr<InstanceTracker> original = MakeShrd<InstanceTracker>(3);
        std::vector<ShrdPtr<InstanceTracker>> copies(5, original);  // 5 копий
        EXPECT_EQ(original.UseCount(), 6u);

        copies.clear();
        EXPECT_EQ(original.UseCount(), 1u);
        EXPECT_EQ(InstanceTracker::alive_count, 1);
    }
    EXPECT_EQ(InstanceTracker::alive_count, 0);
}

// ============================================================================
// 3. Перемещение: владелец меняется, счётчик — нет
// ============================================================================

TEST(ShrdPtrTest, MoveConstructorDoesNotChangeCounter) {
    InstanceTracker::alive_count = 0;
    {
        ShrdPtr<InstanceTracker> source = MakeShrd<InstanceTracker>(5);
        ShrdPtr<InstanceTracker> other_owner = source;
        EXPECT_EQ(source.UseCount(), 2u);

        ShrdPtr<InstanceTracker> destination = std::move(source);
        EXPECT_FALSE(source);
        EXPECT_EQ(source.UseCount(), 0u);
        EXPECT_EQ(destination.UseCount(), 2u);  // всё ещё два владельца
        EXPECT_EQ(destination->value, 5);
    }
    EXPECT_EQ(InstanceTracker::alive_count, 0);
}

TEST(ShrdPtrTest, MoveAssignmentReleasesOldObject) {
    InstanceTracker::alive_count = 0;
    {
        ShrdPtr<InstanceTracker> first = MakeShrd<InstanceTracker>(1);
        ShrdPtr<InstanceTracker> second = MakeShrd<InstanceTracker>(2);

        first = std::move(second);
        EXPECT_EQ(InstanceTracker::alive_count, 1);  // объект 1 удалён
        EXPECT_EQ(first->value, 2);
        EXPECT_EQ(first.UseCount(), 1u);
        EXPECT_FALSE(second);
    }
    EXPECT_EQ(InstanceTracker::alive_count, 0);
}

// ============================================================================
// 4. Присваивание (copy-and-swap)
// ============================================================================

TEST(ShrdPtrTest, CopyAssignmentReleasesOldObject) {
    InstanceTracker::alive_count = 0;
    {
        ShrdPtr<InstanceTracker> first = MakeShrd<InstanceTracker>(1);
        ShrdPtr<InstanceTracker> second = MakeShrd<InstanceTracker>(2);

        first = second;
        EXPECT_EQ(InstanceTracker::alive_count, 1);  // объект 1 удалён: first был его единственным владельцем
        EXPECT_EQ(first->value, 2);
        EXPECT_EQ(second.UseCount(), 2u);
    }
    EXPECT_EQ(InstanceTracker::alive_count, 0);
}

TEST(ShrdPtrTest, CopyAssignmentKeepsOldObjectWithOtherOwner) {
    InstanceTracker::alive_count = 0;
    {
        ShrdPtr<InstanceTracker> first = MakeShrd<InstanceTracker>(1);
        ShrdPtr<InstanceTracker> keeper = first;  // второй владелец объекта 1
        ShrdPtr<InstanceTracker> second = MakeShrd<InstanceTracker>(2);

        first = second;
        EXPECT_EQ(InstanceTracker::alive_count, 2);  // объект 1 жив: им владеет keeper
        EXPECT_EQ(keeper.UseCount(), 1u);
        EXPECT_EQ(keeper->value, 1);
    }
    EXPECT_EQ(InstanceTracker::alive_count, 0);
}

TEST(ShrdPtrTest, AssignmentBetweenOwnersOfSameObject) {
    ShrdPtr<int> first = MakeShrd<int>(1);
    ShrdPtr<int> second = first;

    first = second;  // уже указывают на один объект
    EXPECT_EQ(first.UseCount(), 2u);
    EXPECT_EQ(*first, 1);
}

TEST(ShrdPtrTest, SelfAssignmentKeepsObject) {
    InstanceTracker::alive_count = 0;
    {
        ShrdPtr<InstanceTracker> ptr = MakeShrd<InstanceTracker>(9);
        ShrdPtr<InstanceTracker>& same_ptr = ptr;  // через ссылку, иначе компилятор ругается

        ptr = same_ptr;
        EXPECT_EQ(ptr.UseCount(), 1u);
        EXPECT_EQ(ptr->value, 9);

        ptr = std::move(same_ptr);
        EXPECT_TRUE(ptr);
        EXPECT_EQ(ptr.UseCount(), 1u);
        EXPECT_EQ(InstanceTracker::alive_count, 1);
    }
    EXPECT_EQ(InstanceTracker::alive_count, 0);
}

TEST(ShrdPtrTest, ChainedAssignment) {
    ShrdPtr<int> first;
    ShrdPtr<int> second;
    ShrdPtr<int> third = MakeShrd<int>(3);

    first = second = third;  // operator= возвращает ссылку, поэтому цепочка работает
    EXPECT_EQ(third.UseCount(), 3u);
    EXPECT_EQ(*first, 3);
}

// head->next лежит ВНУТРИ первого узла, которым владеет head.
// Наивное "сначала отпустить своё, потом взять чужое" здесь читает удалённую память.
TEST(ShrdPtrTest, RemoveFirstNodeOfListByCopy) {
    InstanceTracker::alive_count = 0;
    {
        ShrdPtr<SharedListNode> head = MakeShrd<SharedListNode>(1);
        head->next = MakeShrd<SharedListNode>(2);
        head->next->next = MakeShrd<SharedListNode>(3);
        EXPECT_EQ(InstanceTracker::alive_count, 3);

        head = head->next;  // копия

        EXPECT_EQ(InstanceTracker::alive_count, 2);
        EXPECT_EQ(head->tracker.value, 2);
        EXPECT_EQ(head.UseCount(), 1u);
        EXPECT_EQ(head->next->tracker.value, 3);
    }
    EXPECT_EQ(InstanceTracker::alive_count, 0);
}

TEST(ShrdPtrTest, RemoveFirstNodeOfListByMove) {
    InstanceTracker::alive_count = 0;
    {
        ShrdPtr<SharedListNode> head = MakeShrd<SharedListNode>(1);
        head->next = MakeShrd<SharedListNode>(2);

        head = std::move(head->next);  // перемещение

        EXPECT_EQ(InstanceTracker::alive_count, 1);
        EXPECT_EQ(head->tracker.value, 2);
        EXPECT_FALSE(head->next);
    }
    EXPECT_EQ(InstanceTracker::alive_count, 0);
}

TEST(ShrdPtrTest, AssignNullptrReleasesObject) {
    InstanceTracker::alive_count = 0;
    ShrdPtr<InstanceTracker> ptr = MakeShrd<InstanceTracker>(1);

    ptr = nullptr;
    EXPECT_FALSE(ptr);
    EXPECT_EQ(ptr.UseCount(), 0u);
    EXPECT_EQ(InstanceTracker::alive_count, 0);
}

TEST(ShrdPtrTest, AssignUnqPtr) {
    InstanceTracker::alive_count = 0;
    {
        ShrdPtr<InstanceTracker> shared = MakeShrd<InstanceTracker>(1);
        UnqPtr<InstanceTracker> owner(new InstanceTracker(2));

        shared = std::move(owner);
        EXPECT_FALSE(owner);
        EXPECT_EQ(shared->value, 2);
        EXPECT_EQ(shared.UseCount(), 1u);
        EXPECT_EQ(InstanceTracker::alive_count, 1);  // объект 1 удалён
    }
    EXPECT_EQ(InstanceTracker::alive_count, 0);
}

// ============================================================================
// 5. Reset, Swap, сравнения
// ============================================================================

TEST(ShrdPtrTest, ResetOfLastOwnerDeletesObject) {
    InstanceTracker::alive_count = 0;
    ShrdPtr<InstanceTracker> ptr = MakeShrd<InstanceTracker>(1);

    ptr.Reset();
    EXPECT_FALSE(ptr);
    EXPECT_EQ(ptr.Get(), nullptr);
    EXPECT_EQ(ptr.UseCount(), 0u);
    EXPECT_EQ(InstanceTracker::alive_count, 0);
}

TEST(ShrdPtrTest, ResetKeepsObjectForOtherOwners) {
    InstanceTracker::alive_count = 0;
    ShrdPtr<InstanceTracker> first = MakeShrd<InstanceTracker>(1);
    ShrdPtr<InstanceTracker> second = first;

    first.Reset();
    EXPECT_FALSE(first);
    EXPECT_EQ(second.UseCount(), 1u);
    EXPECT_EQ(second->value, 1);
    EXPECT_EQ(InstanceTracker::alive_count, 1);

    second.Reset();
    EXPECT_EQ(InstanceTracker::alive_count, 0);
}

TEST(ShrdPtrTest, ResetOfEmptyPointer) {
    ShrdPtr<int> ptr;
    ptr.Reset();
    EXPECT_FALSE(ptr);
    EXPECT_EQ(ptr.UseCount(), 0u);
}

TEST(ShrdPtrTest, ResetWithNewObjectChangesOnlyThisPointer) {
    InstanceTracker::alive_count = 0;
    ShrdPtr<InstanceTracker> first = MakeShrd<InstanceTracker>(1);
    ShrdPtr<InstanceTracker> second = first;

    first.Reset(new InstanceTracker(2));
    EXPECT_EQ(first->value, 2);
    EXPECT_EQ(first.UseCount(), 1u);   // у нового объекта свой счётчик
    EXPECT_EQ(second->value, 1);       // second по-прежнему владеет старым
    EXPECT_EQ(second.UseCount(), 1u);
    EXPECT_EQ(InstanceTracker::alive_count, 2);

    first.Reset(nullptr);
    second.Reset();
    EXPECT_EQ(InstanceTracker::alive_count, 0);
}

TEST(ShrdPtrTest, ResetWithSamePointerKeepsObject) {
    InstanceTracker::alive_count = 0;
    ShrdPtr<InstanceTracker> ptr = MakeShrd<InstanceTracker>(5);

    ptr.Reset(ptr.Get());  // не должен создать второй счётчик для того же объекта
    EXPECT_EQ(ptr.UseCount(), 1u);
    EXPECT_EQ(ptr->value, 5);
    EXPECT_EQ(InstanceTracker::alive_count, 1);
}

TEST(ShrdPtrTest, SwapExchangesObjectsAndCounters) {
    InstanceTracker::alive_count = 0;
    {
        ShrdPtr<InstanceTracker> first = MakeShrd<InstanceTracker>(1);
        ShrdPtr<InstanceTracker> first_copy = first;    // у объекта 1 два владельца
        ShrdPtr<InstanceTracker> second = MakeShrd<InstanceTracker>(2);

        first.Swap(second);
        EXPECT_EQ(first->value, 2);
        EXPECT_EQ(first.UseCount(), 1u);
        EXPECT_EQ(second->value, 1);
        EXPECT_EQ(second.UseCount(), 2u);

        using std::swap;
        swap(first, second);  // ADL найдёт наш friend swap
        EXPECT_EQ(first->value, 1);
        EXPECT_EQ(second->value, 2);

        EXPECT_EQ(InstanceTracker::alive_count, 2);  // ничего не удалено
    }
    EXPECT_EQ(InstanceTracker::alive_count, 0);
}

TEST(ShrdPtrTest, ComparisonComparesAddresses) {
    ShrdPtr<int> first = MakeShrd<int>(1);
    ShrdPtr<int> first_copy = first;
    ShrdPtr<int> second = MakeShrd<int>(1);
    ShrdPtr<int> empty1;
    ShrdPtr<int> empty2;

    EXPECT_TRUE(first == first_copy);  // один объект
    EXPECT_TRUE(first != second);      // значения равны, но объекты разные
    EXPECT_TRUE(empty1 == empty2);
    EXPECT_TRUE(first != nullptr);
    EXPECT_TRUE(empty1 == nullptr);
}

// ============================================================================
// 6. Подтипизация
// ============================================================================

TEST(ShrdPtrTest, DerivedCopiesIntoBase) {
    BaseEntity::alive_entities = 0;
    {
        ShrdPtr<DerivedEntity> derived_ptr = MakeShrd<DerivedEntity>();
        ShrdPtr<BaseEntity> base_ptr = derived_ptr;  // копия: SmrtPtr<C2> ptr2 = ptr1 из условия

        EXPECT_EQ(derived_ptr.UseCount(), 2u);  // общий счётчик
        EXPECT_EQ(base_ptr.Get(), derived_ptr.Get());
        EXPECT_EQ(base_ptr->GetName(), "DerivedEntity");  // virtual работает
        EXPECT_EQ(BaseEntity::alive_entities, 1);
    }
    EXPECT_EQ(BaseEntity::alive_entities, 0);
}

TEST(ShrdPtrTest, DerivedMovesIntoBase) {
    BaseEntity::alive_entities = 0;
    {
        ShrdPtr<DerivedEntity> derived_ptr = MakeShrd<DerivedEntity>();
        ShrdPtr<BaseEntity> base_ptr = std::move(derived_ptr);

        EXPECT_FALSE(derived_ptr);
        EXPECT_EQ(base_ptr.UseCount(), 1u);
        EXPECT_EQ(base_ptr->GetName(), "DerivedEntity");
    }
    EXPECT_EQ(BaseEntity::alive_entities, 0);
}

TEST(ShrdPtrTest, DerivedAssignedToBaseReleasesOldObject) {
    BaseEntity::alive_entities = 0;
    {
        ShrdPtr<BaseEntity> base_ptr = MakeShrd<BaseEntity>();
        ShrdPtr<DerivedEntity> derived_ptr = MakeShrd<DerivedEntity>();
        EXPECT_EQ(BaseEntity::alive_entities, 2);

        base_ptr = derived_ptr;
        EXPECT_EQ(BaseEntity::alive_entities, 1);  // старый BaseEntity удалён
        EXPECT_EQ(base_ptr->GetName(), "DerivedEntity");
        EXPECT_EQ(derived_ptr.UseCount(), 2u);
    }
    EXPECT_EQ(BaseEntity::alive_entities, 0);
}

TEST(ShrdPtrTest, LastBaseOwnerDeletesDerivedObject) {
    BaseEntity::alive_entities = 0;
    {
        ShrdPtr<BaseEntity> base_ptr;
        {
            ShrdPtr<DerivedEntity> derived_ptr = MakeShrd<DerivedEntity>();
            base_ptr = derived_ptr;
        }
        // derived_ptr умер, объект жив через base_ptr
        EXPECT_EQ(BaseEntity::alive_entities, 1);
        EXPECT_EQ(base_ptr.UseCount(), 1u);
    }
    EXPECT_EQ(BaseEntity::alive_entities, 0);  // удалён через Base* — нужен virtual ~BaseEntity
}

TEST(ShrdPtrTest, BasePointerFromUnqPtrAndRawDerived) {
    BaseEntity::alive_entities = 0;
    {
        ShrdPtr<BaseEntity> from_unq = MakeUnq<DerivedEntity>();
        ShrdPtr<BaseEntity> from_raw(new DerivedEntity());
        EXPECT_EQ(from_unq->GetName(), "DerivedEntity");
        EXPECT_EQ(from_raw->GetName(), "DerivedEntity");
        EXPECT_EQ(BaseEntity::alive_entities, 2);
    }
    EXPECT_EQ(BaseEntity::alive_entities, 0);
}

TEST(ShrdPtrTest, ConversionToConstPointer) {
    ShrdPtr<int> ptr = MakeShrd<int>(8);
    ShrdPtr<const int> const_ptr = ptr;
    EXPECT_EQ(ptr.UseCount(), 2u);
    EXPECT_EQ(*const_ptr, 8);

    *ptr = 9;  // меняем через неконстантный — видно через константный
    EXPECT_EQ(*const_ptr, 9);
}

// ============================================================================
// 7. MakeShrd
// ============================================================================

TEST(ShrdPtrTest, MakeShrdCreatesObject) {
    InstanceTracker::alive_count = 0;
    {
        ShrdPtr<InstanceTracker> ptr = MakeShrd<InstanceTracker>(42);
        EXPECT_EQ(ptr->value, 42);
        EXPECT_EQ(ptr.UseCount(), 1u);
        EXPECT_EQ(InstanceTracker::alive_count, 1);
    }
    EXPECT_EQ(InstanceTracker::alive_count, 0);
}

TEST(ShrdPtrTest, MakeShrdPassesSeveralArguments) {
    ShrdPtr<std::string> ptr = MakeShrd<std::string>(3, 'a');  // как std::string(3, 'a')
    EXPECT_EQ(*ptr, "aaa");
}

TEST(ShrdPtrTest, MakeShrdCopiesVariable) {
    std::string text = "hello";
    ShrdPtr<std::string> ptr = MakeShrd<std::string>(text);
    EXPECT_EQ(*ptr, "hello");
    EXPECT_EQ(text, "hello");  // переменная не тронута
}

TEST(ShrdPtrTest, MakeShrdWithoutArgumentsGivesZero) {
    ShrdPtr<int> ptr = MakeShrd<int>();
    EXPECT_EQ(*ptr, 0);
}

// ============================================================================
// 8. Циклические ссылки — известное ограничение подсчёта ссылок
// ============================================================================

TEST(ShrdPtrTest, CycleKeepsObjectsAliveUntilBroken) {
    InstanceTracker::alive_count = 0;
    {
        ShrdPtr<SharedListNode> first = MakeShrd<SharedListNode>(1);
        ShrdPtr<SharedListNode> second = MakeShrd<SharedListNode>(2);
        first->next = second;
        second->next = first;  // цикл: каждый узел владеет другим

        EXPECT_EQ(first.UseCount(), 2u);
        EXPECT_EQ(second.UseCount(), 2u);

        // Без этой строки после выхода из блока счётчики станут 1, а не 0,
        // и оба узла утекут. В STL цикл разрывают через std::weak_ptr.
        second->next = nullptr;
        EXPECT_EQ(first.UseCount(), 1u);
    }
    EXPECT_EQ(InstanceTracker::alive_count, 0);
}

// ============================================================================
// 9. ShrdPtr<T[]> — массивы
// ============================================================================

TEST(ShrdPtrArrayTest, DefaultAndNullptrAreEmpty) {
    ShrdPtr<int[]> by_default;
    ShrdPtr<int[]> from_null = nullptr;
    EXPECT_FALSE(by_default);
    EXPECT_FALSE(from_null);
    EXPECT_EQ(by_default.UseCount(), 0u);
    EXPECT_TRUE(by_default == nullptr);
}

TEST(ShrdPtrArrayTest, CreateFromUnqPtrAndRawPointer) {
    InstanceTracker::alive_count = 0;
    {
        UnqPtr<InstanceTracker[]> owner(new InstanceTracker[3]());
        ShrdPtr<InstanceTracker[]> from_unq = std::move(owner);
        ShrdPtr<InstanceTracker[]> from_raw(new InstanceTracker[2]());

        EXPECT_FALSE(owner);
        EXPECT_EQ(from_unq.UseCount(), 1u);
        EXPECT_EQ(from_raw.UseCount(), 1u);
        EXPECT_EQ(InstanceTracker::alive_count, 5);
    }
    EXPECT_EQ(InstanceTracker::alive_count, 0);  // delete[] вызвал все 5 деструкторов
}

TEST(ShrdPtrArrayTest, CreateFromEmptyUnqPtrGivesEmptyPointer) {
    UnqPtr<int[]> empty_owner;
    ShrdPtr<int[]> shared = std::move(empty_owner);
    EXPECT_FALSE(shared);
    EXPECT_EQ(shared.UseCount(), 0u);
}

TEST(ShrdPtrArrayTest, CopiesShareElements) {
    ShrdPtr<int[]> first(new int[3]());
    ShrdPtr<int[]> second = first;

    first[1] = 20;
    EXPECT_EQ(second[1], 20);
    EXPECT_EQ(first.UseCount(), 2u);
}

TEST(ShrdPtrArrayTest, ArrayLivesUntilLastOwnerDies) {
    InstanceTracker::alive_count = 0;
    {
        ShrdPtr<InstanceTracker[]> outer = MakeShrd<InstanceTracker[]>(4);
        {
            ShrdPtr<InstanceTracker[]> inner = outer;
            EXPECT_EQ(outer.UseCount(), 2u);
        }
        EXPECT_EQ(outer.UseCount(), 1u);
        EXPECT_EQ(InstanceTracker::alive_count, 4);
    }
    EXPECT_EQ(InstanceTracker::alive_count, 0);
}

TEST(ShrdPtrArrayTest, MoveAndAssignment) {
    InstanceTracker::alive_count = 0;
    {
        ShrdPtr<InstanceTracker[]> source = MakeShrd<InstanceTracker[]>(3);
        ShrdPtr<InstanceTracker[]> destination = std::move(source);
        EXPECT_FALSE(source);
        EXPECT_EQ(destination.UseCount(), 1u);

        ShrdPtr<InstanceTracker[]> other = MakeShrd<InstanceTracker[]>(2);
        destination = other;  // старый массив из 3 элементов удалён
        EXPECT_EQ(InstanceTracker::alive_count, 2);
        EXPECT_EQ(other.UseCount(), 2u);

        destination = nullptr;
        EXPECT_EQ(other.UseCount(), 1u);
    }
    EXPECT_EQ(InstanceTracker::alive_count, 0);
}

TEST(ShrdPtrArrayTest, ResetVariants) {
    InstanceTracker::alive_count = 0;
    ShrdPtr<InstanceTracker[]> array(new InstanceTracker[3]());

    array.Reset(new InstanceTracker[2]());
    EXPECT_EQ(InstanceTracker::alive_count, 2);
    EXPECT_EQ(array.UseCount(), 1u);

    array.Reset(array.Get());  // тот же массив — ничего не меняется
    EXPECT_EQ(array.UseCount(), 1u);
    EXPECT_EQ(InstanceTracker::alive_count, 2);

    array.Reset(nullptr);
    EXPECT_FALSE(array);
    EXPECT_EQ(InstanceTracker::alive_count, 0);

    array.Reset(new InstanceTracker[1]());
    array.Reset();
    EXPECT_EQ(InstanceTracker::alive_count, 0);
}

TEST(ShrdPtrArrayTest, SwapExchangesArrays) {
    ShrdPtr<int[]> first(new int[1]{1});
    ShrdPtr<int[]> second(new int[1]{2});

    using std::swap;
    swap(first, second);
    EXPECT_EQ(first[0], 2);
    EXPECT_EQ(second[0], 1);

    first.Swap(second);
    EXPECT_EQ(first[0], 1);
}

TEST(ShrdPtrArrayTest, ConversionToConstArray) {
    ShrdPtr<int[]> array(new int[2]{4, 5});
    ShrdPtr<const int[]> const_array = array;
    EXPECT_EQ(array.UseCount(), 2u);
    EXPECT_EQ(const_array[1], 5);

    ShrdPtr<const int[]> moved = std::move(array);
    EXPECT_FALSE(array);
    EXPECT_EQ(const_array.UseCount(), 2u);
}

TEST(ShrdPtrArrayTest, MakeShrdArrayIsFilledWithZeros) {
    ShrdPtr<int[]> numbers = MakeShrd<int[]>(5);
    for (size_t i = 0; i < 5; ++i)
        EXPECT_EQ(numbers[i], 0);
    EXPECT_EQ(numbers.UseCount(), 1u);
}

// ============================================================================
// 10. Death-тесты: assert на пустом указателе.
// Работают только в Debug: в Release (NDEBUG) assert'ов нет.
// ============================================================================

#ifndef NDEBUG
TEST(ShrdPtrDeathTest, DereferencingEmptyPointerCrashes) {
    ShrdPtr<int> ptr;
    EXPECT_DEATH((void)*ptr, "Dereferencing empty ShrdPtr");
}

TEST(ShrdPtrDeathTest, MemberAccessThroughEmptyPointerCrashes) {
    ShrdPtr<InstanceTracker> ptr;
    EXPECT_DEATH((void)ptr->value, "Member access through empty ShrdPtr");
}

TEST(ShrdPtrDeathTest, IndexingEmptyArrayCrashes) {
    ShrdPtr<int[]> array;
    EXPECT_DEATH((void)array[0], "Indexing empty ShrdPtr");
}
#endif
