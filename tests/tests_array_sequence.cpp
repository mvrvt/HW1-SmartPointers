#include <gtest/gtest.h>

#include <cstddef>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

#include "lab2/ArraySequence.hpp"
#include "lab2/DynamicArray.hpp"
#include "pointers/UnqPtr.hpp"
#include "test_helpers.hpp"

// Контейнеры из ЛР-2: DynamicArray хранит буфер в UnqPtr<T[]>,
// а Mutable/ImmutableArraySequence построены поверх DynamicArray.

// Строки длиннее ~22 символов хранятся в куче: если строка прочитана из
// удалённой памяти, ASan/Valgrind это заметят, а тест увидит «мусор»
std::string LongText(int number) {
    return "a long string that does not fit into SSO #" + std::to_string(number);
}

// Массив 10, 20, ..., count * 10
DynamicArray<int> MakeTens(std::size_t count) {
    DynamicArray<int> array;
    for (std::size_t i = 1; i <= count; ++i)
        array.Append(static_cast<int>(i * 10));
    return array;
}

int Square(const int& x) { return x * x; }
bool IsEven(const int& x) { return x % 2 == 0; }
int Sum(const int& acc, const int& x) { return acc + x; }

// ============================================================================
// 0. Проверки во время компиляции
// ============================================================================

static_assert(std::is_base_of_v<Sequence<int>, MutableArraySequence<int>>);
static_assert(std::is_base_of_v<Sequence<int>, ImmutableArraySequence<int>>);
static_assert(std::is_abstract_v<ArraySequence<int>>);                  // создавать нужно Mutable/Immutable
static_assert(std::is_nothrow_move_constructible_v<DynamicArray<int>>);
static_assert(std::is_nothrow_move_assignable_v<DynamicArray<int>>);
static_assert(!std::is_convertible_v<std::size_t, DynamicArray<int>>);  // explicit у конструктора из size_t

// ============================================================================
// 1. DynamicArray: создание и доступ
// ============================================================================

TEST(DynamicArrayTest, DefaultArrayIsEmpty) {
    DynamicArray<int> array;
    EXPECT_TRUE(array.IsEmpty());
    EXPECT_EQ(array.GetCount(), 0u);
    EXPECT_EQ(array.GetCapacity(), 0u);
    EXPECT_EQ(array.begin(), array.end());
}

TEST(DynamicArrayTest, SizedArrayIsFilledWithZeros) {
    DynamicArray<int> array(5);
    EXPECT_EQ(array.GetCount(), 5u);
    EXPECT_EQ(array.GetCapacity(), 5u);
    for (std::size_t i = 0; i < 5; ++i)
        EXPECT_EQ(array[i], 0);
}

TEST(DynamicArrayTest, FilledWithValue) {
    DynamicArray<std::string> array(3, LongText(1));
    EXPECT_EQ(array.GetCount(), 3u);
    for (const std::string& item : array)
        EXPECT_EQ(item, LongText(1));
}

TEST(DynamicArrayTest, CreateFromArray) {
    const int items[] = {1, 2, 3};
    DynamicArray<int> array(items, 3);
    EXPECT_EQ(array.GetCount(), 3u);
    EXPECT_EQ(array.GetFirst(), 1);
    EXPECT_EQ(array.GetLast(), 3);

    EXPECT_THROW(DynamicArray<int>(nullptr, 5), std::invalid_argument);
    DynamicArray<int> empty(nullptr, 0);  // ноль элементов из nullptr - допустимо
    EXPECT_TRUE(empty.IsEmpty());
}

TEST(DynamicArrayTest, OutOfRangeAndEmptyThrow) {
    DynamicArray<int> array = MakeTens(3);
    EXPECT_THROW(array.Get(3), IndexOutOfRange);
    EXPECT_THROW(array[100], IndexOutOfRange);
    EXPECT_THROW(array.Set(3, 1), IndexOutOfRange);
    EXPECT_THROW(array.RemoveAt(3), IndexOutOfRange);
    EXPECT_THROW(array.InsertAt(1, 4), IndexOutOfRange);
    EXPECT_EQ(array.GetCount(), 3u);  // массив не изменился

    DynamicArray<int> empty;
    EXPECT_THROW(empty.GetFirst(), EmptyCollection);
    EXPECT_THROW(empty.GetLast(), EmptyCollection);
    EXPECT_THROW(empty.RemoveLast(), EmptyCollection);
}

// ============================================================================
// 2. DynamicArray: изменение и рост буфера
// ============================================================================

TEST(DynamicArrayTest, CapacityGrowsTwofold) {
    DynamicArray<int> array;
    array.Append(1);
    EXPECT_EQ(array.GetCapacity(), 4u);  // 0 -> 4
    for (int i = 2; i <= 5; ++i)
        array.Append(i);
    EXPECT_EQ(array.GetCapacity(), 8u);  // 4 -> 8
    for (std::size_t i = 0; i < 5; ++i)
        EXPECT_EQ(array[i], static_cast<int>(i + 1));  // при росте элементы сохранились
}

TEST(DynamicArrayTest, ReserveKeepsElements) {
    DynamicArray<int> array = MakeTens(3);
    array.Reserve(50);
    EXPECT_EQ(array.GetCapacity(), 50u);
    EXPECT_EQ(array.GetCount(), 3u);
    EXPECT_EQ(array.GetLast(), 30);

    array.Reserve(10);  // меньше текущей ёмкости - ничего не делает
    EXPECT_EQ(array.GetCapacity(), 50u);
}

TEST(DynamicArrayTest, InsertPrependAndRemove) {
    DynamicArray<int> array = MakeTens(3);  // 10 20 30
    array.InsertAt(15, 1);                  // 10 15 20 30
    array.Prepend(5);                       // 5 10 15 20 30
    array.InsertAt(35, 5);                  // 5 10 15 20 30 35
    EXPECT_EQ(array.GetCount(), 6u);
    EXPECT_EQ(array[0], 5);
    EXPECT_EQ(array[2], 15);
    EXPECT_EQ(array.GetLast(), 35);

    array.RemoveAt(0);  // 10 15 20 30 35
    array.RemoveLast(); // 10 15 20 30
    EXPECT_EQ(array.GetCount(), 4u);
    EXPECT_EQ(array.GetFirst(), 10);
    EXPECT_EQ(array.GetLast(), 30);
}

TEST(DynamicArrayTest, AppendOwnElementWithReallocation) {
    DynamicArray<std::string> array;
    for (int i = 0; i < 4; ++i)
        array.Append(LongText(i));
    ASSERT_EQ(array.GetCount(), array.GetCapacity());  // следующий Append перевыделит буфер

    array.Append(array[0]);  // ссылка на элемент старого буфера
    EXPECT_EQ(array.GetLast(), LongText(0));
}

TEST(DynamicArrayTest, ResizeAndClear) {
    DynamicArray<int> array = MakeTens(3);
    array.Resize(5);
    EXPECT_EQ(array.GetCount(), 5u);
    EXPECT_EQ(array[3], 0);  // новые элементы равны T()
    EXPECT_EQ(array[4], 0);

    array.Resize(2);
    EXPECT_EQ(array.GetCount(), 2u);
    EXPECT_EQ(array.GetLast(), 20);

    std::size_t capacity = array.GetCapacity();
    array.Clear();
    EXPECT_TRUE(array.IsEmpty());
    EXPECT_EQ(array.GetCapacity(), capacity);  // ёмкость сохраняется, как у std::vector::clear
}

TEST(DynamicArrayTest, RangeForModifiesElements) {
    DynamicArray<int> array = MakeTens(3);
    for (int& item : array)
        item *= 2;
    EXPECT_EQ(array[0], 20);
    EXPECT_EQ(array[2], 60);
}

TEST(DynamicArrayTest, EnumeratorVisitsAllElements) {
    DynamicArray<int> array = MakeTens(3);
    UnqPtr<IEnumerator<int>> it(array.GetEnumerator());
    int sum = 0;
    while (it->MoveNext())
        sum += it->Current();
    EXPECT_EQ(sum, 60);
    EXPECT_FALSE(it->MoveNext());  // после конца обход заново не начинается
}

// ============================================================================
// 3. DynamicArray: копирование и перемещение
// ============================================================================

TEST(DynamicArrayTest, CopyIsDeep) {
    DynamicArray<int> original = MakeTens(2);
    DynamicArray<int> copy = original;
    EXPECT_NE(copy.begin(), original.begin());  // у копии свой буфер

    copy[0] = 999;
    copy.Append(30);
    EXPECT_EQ(original[0], 10);
    EXPECT_EQ(original.GetCount(), 2u);
}

TEST(DynamicArrayTest, CopyAssignment) {
    DynamicArray<int> source = MakeTens(3);
    DynamicArray<int> target = MakeTens(1);
    target = source;
    EXPECT_EQ(target.GetCount(), 3u);
    source[0] = -1;
    EXPECT_EQ(target[0], 10);

    DynamicArray<int>& same = target;  // через ссылку, иначе компилятор ругается
    target = same;
    EXPECT_EQ(target.GetCount(), 3u);
    EXPECT_EQ(target.GetLast(), 30);
}

TEST(DynamicArrayTest, MoveConstructorTakesBuffer) {
    DynamicArray<int> source = MakeTens(2);
    const int* buffer = source.begin();

    DynamicArray<int> destination = std::move(source);
    EXPECT_EQ(destination.begin(), buffer);  // буфер не копировался, его забрал UnqPtr
    EXPECT_EQ(destination[1], 20);
    EXPECT_TRUE(source.IsEmpty());
    EXPECT_EQ(source.GetCapacity(), 0u);
    EXPECT_EQ(source.begin(), nullptr);
}

TEST(DynamicArrayTest, MoveAssignmentFreesOldBuffer) {
    InstanceTracker::alive_count = 0;
    {
        DynamicArray<InstanceTracker> source(3);
        DynamicArray<InstanceTracker> target(5);
        EXPECT_EQ(InstanceTracker::alive_count, 8);

        target = std::move(source);
        EXPECT_EQ(InstanceTracker::alive_count, 3);  // старый буфер target удалил UnqPtr
        EXPECT_EQ(target.GetCount(), 3u);
        EXPECT_TRUE(source.IsEmpty());

        source.Append(InstanceTracker(7));  // перемещённым объектом можно пользоваться дальше
        EXPECT_EQ(source.GetFirst().value, 7);

        DynamicArray<InstanceTracker>& same = target;
        target = std::move(same);  // самоприсваивание ничего не ломает
        EXPECT_EQ(target.GetCount(), 3u);
    }
    EXPECT_EQ(InstanceTracker::alive_count, 0);
}

TEST(DynamicArrayTest, SwapExchangesBuffers) {
    DynamicArray<int> first = MakeTens(2);
    DynamicArray<int> second = MakeTens(5);
    const int* first_buffer = first.begin();

    first.Swap(second);
    EXPECT_EQ(first.GetCount(), 5u);
    EXPECT_EQ(second.GetCount(), 2u);
    EXPECT_EQ(second.begin(), first_buffer);
    EXPECT_EQ(first.GetLast(), 50);
    EXPECT_EQ(second.GetLast(), 20);
}

// ============================================================================
// 4. DynamicArray: время жизни элементов
// ============================================================================

TEST(DynamicArrayTest, StoresNonCopyableUnqPtr) {
    InstanceTracker::alive_count = 0;
    {
        DynamicArray<UnqPtr<InstanceTracker>> array;
        for (int i = 0; i < 10; ++i)
            array.Append(MakeUnq<InstanceTracker>(i));  // несколько перевыделений: элементы перемещаются
        EXPECT_EQ(InstanceTracker::alive_count, 10);
        for (int i = 0; i < 10; ++i)
            EXPECT_EQ(array[static_cast<std::size_t>(i)]->value, i);

        array.InsertAt(MakeUnq<InstanceTracker>(100), 0);
        EXPECT_EQ(array.GetFirst()->value, 100);

        array.RemoveAt(0);  // удалённый элемент сразу освобождает объект
        EXPECT_EQ(InstanceTracker::alive_count, 10);
        array.RemoveLast();
        EXPECT_EQ(InstanceTracker::alive_count, 9);
    }
    EXPECT_EQ(InstanceTracker::alive_count, 0);
}

TEST(DynamicArrayTest, AllObjectsAreDestroyed) {
    InstanceTracker::alive_count = 0;
    {
        DynamicArray<InstanceTracker> array;
        for (int i = 0; i < 10; ++i)
            array.Append(InstanceTracker(i));
        DynamicArray<InstanceTracker> copy = array;
        copy.Reserve(100);
        EXPECT_EQ(copy[9].value, 9);
    }
    EXPECT_EQ(InstanceTracker::alive_count, 0);  // ни утечек, ни двойных удалений
}

// ============================================================================
// 5. MutableArraySequence / ImmutableArraySequence
// ============================================================================

TEST(ArraySequenceTest, MutableAppendChangesItself) {
    MutableArraySequence<int> seq;
    Sequence<int>* result = seq.Append(1)->Append(2)->Prepend(0);
    EXPECT_EQ(result, &seq);  // Mutable возвращает this
    EXPECT_EQ(seq.GetLength(), 3u);
    EXPECT_EQ(seq.GetFirst(), 0);
    EXPECT_EQ(seq.GetLast(), 2);
}

TEST(ArraySequenceTest, MutableAccessByReference) {
    const int items[] = {1, 2, 3};
    MutableArraySequence<int> seq(items, 3);
    seq[0] = 10;
    for (int& item : seq)
        item *= 2;
    EXPECT_EQ(seq.Get(0), 20);
    EXPECT_EQ(seq.Get(2), 6);
    EXPECT_THROW(seq.Get(3), IndexOutOfRange);
}

TEST(ArraySequenceTest, MutableReserve) {
    MutableArraySequence<int> seq;
    seq.Reserve(32);
    EXPECT_EQ(seq.GetCapacity(), 32u);
    EXPECT_EQ(seq.GetLength(), 0u);
}

TEST(ArraySequenceTest, ImmutableReturnsNewObject) {
    const int items[] = {1, 2};
    ImmutableArraySequence<int> seq(items, 2);

    UnqPtr<Sequence<int>> appended(seq.Append(3));
    EXPECT_NE(appended.Get(), &seq);
    EXPECT_EQ(seq.GetLength(), 2u);  // оригинал не изменился
    EXPECT_EQ(appended->GetLength(), 3u);
    EXPECT_EQ(appended->GetLast(), 3);
}

TEST(ArraySequenceTest, ImmutableErrorDoesNotLeak) {
    InstanceTracker::alive_count = 0;
    {
        ImmutableArraySequence<InstanceTracker> seq;
        UnqPtr<Sequence<InstanceTracker>> one(seq.Append(InstanceTracker(1)));
        const int alive_before = InstanceTracker::alive_count;  // все ячейки буфера (new T[capacity])

        EXPECT_THROW(one->InsertAt(InstanceTracker(2), 5), IndexOutOfRange);
        EXPECT_THROW(one->RemoveAt(1), IndexOutOfRange);
        EXPECT_EQ(InstanceTracker::alive_count, alive_before);  // копия при ошибке не создавалась
    }
    EXPECT_EQ(InstanceTracker::alive_count, 0);
}

TEST(ArraySequenceTest, CreateFromDynamicArray) {
    DynamicArray<int> array = MakeTens(3);
    MutableArraySequence<int> seq(array);
    array[0] = -1;
    EXPECT_EQ(seq.GetLength(), 3u);
    EXPECT_EQ(seq[0], 10);  // последовательность хранит свою копию
}

TEST(ArraySequenceTest, MapWhereReduceConcat) {
    const int items[] = {1, 2, 3, 4};
    MutableArraySequence<int> seq(items, 4);

    UnqPtr<Sequence<int>> squares(seq.Map(Square));
    EXPECT_EQ(squares->GetLast(), 16);

    UnqPtr<Sequence<int>> even(seq.Where(IsEven));
    EXPECT_EQ(even->GetLength(), 2u);
    EXPECT_EQ(even->GetFirst(), 2);

    EXPECT_EQ(seq.Reduce(Sum, 0), 10);

    UnqPtr<Sequence<int>> joined(seq.Concat(*even));
    EXPECT_EQ(joined->GetLength(), 6u);
    EXPECT_EQ(joined->GetLast(), 4);

    UnqPtr<Sequence<int>> middle(seq.GetSubsequence(1, 2));
    EXPECT_EQ(middle->GetLength(), 2u);
    EXPECT_EQ(middle->GetFirst(), 2);
}

TEST(ArraySequenceTest, CopyIsIndependentAndNoLeaks) {
    InstanceTracker::alive_count = 0;
    {
        MutableArraySequence<InstanceTracker> original;
        for (int i = 0; i < 10; ++i)
            original.Append(InstanceTracker(i));
        MutableArraySequence<InstanceTracker> copy = original;
        copy.RemoveAt(0);
        copy[0].value = 999;
        EXPECT_EQ(original.GetLength(), 10u);
        EXPECT_EQ(original[1].value, 1);
        EXPECT_EQ(copy.GetLength(), 9u);
    }
    EXPECT_EQ(InstanceTracker::alive_count, 0);
}
