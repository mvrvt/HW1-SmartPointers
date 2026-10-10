#include <gtest/gtest.h>

#include <cstddef>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

#include "ArraySequence.hpp"
#include "Sequence.hpp"
#include "pointers/UnqPtr.hpp"
#include "test_helpers.hpp"

// Строки длиннее ~22 символов хранятся в куче: если строка прочитана из
// удалённой памяти, ASan/Valgrind это заметят, а тест увидит «мусор»
std::string LongText(int number) {
    return "a long string that does not fit into SSO #" + std::to_string(number);
}

// Последовательность 10, 20, ..., count * 10
ArraySequence<int> MakeTens(size_t count) {
    ArraySequence<int> seq;
    for (size_t i = 1; i <= count; ++i)
        seq.Append(static_cast<int>(i * 10));
    return seq;
}

// ============================================================================
// 0. Проверки во время компиляции
// ============================================================================

static_assert(std::is_base_of_v<Sequence<int>, ArraySequence<int>>);  // реализует интерфейс Sequence
static_assert(std::is_copy_constructible_v<ArraySequence<int>>);      // глубокое копирование есть
static_assert(std::is_nothrow_move_constructible_v<ArraySequence<int>>);
static_assert(std::is_nothrow_move_assignable_v<ArraySequence<int>>);
static_assert(!std::is_convertible_v<size_t, ArraySequence<int>>);    // explicit у конструктора из size_t

// ============================================================================
// 1. Создание
// ============================================================================

TEST(ArraySequenceTest, DefaultSequenceIsEmpty) {
    ArraySequence<int> seq;
    EXPECT_TRUE(seq.IsEmpty());
    EXPECT_EQ(seq.GetLength(), 0u);
    EXPECT_EQ(seq.GetCapacity(), 0u);
    EXPECT_THROW(seq.Get(0), std::out_of_range);
    EXPECT_THROW(seq.GetFirst(), std::out_of_range);
    EXPECT_THROW(seq.GetLast(), std::out_of_range);
}

TEST(ArraySequenceTest, SizedSequenceIsFilledWithZeros) {
    ArraySequence<int> seq(5);
    EXPECT_EQ(seq.GetLength(), 5u);
    for (size_t i = 0; i < 5; ++i)
        EXPECT_EQ(seq.Get(i), 0);
}

TEST(ArraySequenceTest, CreateFromArray) {
    int items[] = {1, 2, 3};
    ArraySequence<int> seq(items, 3);
    EXPECT_EQ(seq.GetLength(), 3u);
    EXPECT_EQ(seq.GetFirst(), 1);
    EXPECT_EQ(seq.GetLast(), 3);

    items[0] = 100;  // исходный массив и последовательность независимы
    EXPECT_EQ(seq.GetFirst(), 1);
}

TEST(ArraySequenceTest, CreateFromNullArray) {
    EXPECT_THROW(ArraySequence<int>(nullptr, 5), std::invalid_argument);

    ArraySequence<int> empty(nullptr, 0);  // ноль элементов из nullptr — допустимо
    EXPECT_TRUE(empty.IsEmpty());
}

// ============================================================================
// 2. Доступ к элементам
// ============================================================================

TEST(ArraySequenceTest, GetFirstLastAndIndex) {
    ArraySequence<int> seq = MakeTens(4);
    EXPECT_EQ(seq.GetFirst(), 10);
    EXPECT_EQ(seq.GetLast(), 40);
    EXPECT_EQ(seq.Get(2), 30);
    EXPECT_EQ(seq[1], 20);
}

TEST(ArraySequenceTest, AccessAllowsModification) {
    ArraySequence<int> seq = MakeTens(3);
    seq.Get(0) = 1;
    seq.GetLast() = 3;
    seq[1] = 2;
    EXPECT_EQ(seq.GetFirst(), 1);
    EXPECT_EQ(seq.Get(1), 2);
    EXPECT_EQ(seq.Get(2), 3);
}

TEST(ArraySequenceTest, ConstSequenceCanBeRead) {
    const ArraySequence<int> seq = MakeTens(3);
    EXPECT_EQ(seq.Get(1), 20);
    EXPECT_EQ(seq.GetFirst(), 10);
    EXPECT_EQ(seq.GetLast(), 30);
    EXPECT_EQ(seq[2], 30);
    EXPECT_THROW(seq.Get(3), std::out_of_range);
}

TEST(ArraySequenceTest, GetOutOfRangeThrows) {
    ArraySequence<int> seq = MakeTens(3);
    EXPECT_THROW(seq.Get(3), std::out_of_range);  // индекс == длине
    EXPECT_THROW(seq.Get(100), std::out_of_range);
}

// ============================================================================
// 3. Добавление
// ============================================================================

TEST(ArraySequenceTest, AppendAddsToEnd) {
    ArraySequence<int> seq;
    seq.Append(1);
    seq.Append(2);
    EXPECT_EQ(seq.GetLength(), 2u);
    EXPECT_EQ(seq.GetFirst(), 1);
    EXPECT_EQ(seq.GetLast(), 2);
}

TEST(ArraySequenceTest, AppendGrowsCapacityAndKeepsElements) {
    ArraySequence<int> seq;
    for (int i = 0; i < 100; ++i)
        seq.Append(i);

    EXPECT_EQ(seq.GetLength(), 100u);
    EXPECT_GE(seq.GetCapacity(), 100u);
    for (int i = 0; i < 100; ++i)
        EXPECT_EQ(seq.Get(static_cast<size_t>(i)), i);  // после всех перевыделений ничего не потерялось
}

TEST(ArraySequenceTest, CapacityDoublesWhenFull) {
    ArraySequence<int> seq;
    seq.Append(1);
    EXPECT_EQ(seq.GetCapacity(), 4u);  // первое выделение — 4 элемента
    for (int i = 0; i < 4; ++i)
        seq.Append(i);
    EXPECT_EQ(seq.GetCapacity(), 8u);  // 5-й элемент не влез — ёмкость удвоилась
}

TEST(ArraySequenceTest, PrependAddsToBeginning) {
    ArraySequence<int> seq = MakeTens(2);
    seq.Prepend(5);
    EXPECT_EQ(seq.GetLength(), 3u);
    EXPECT_EQ(seq.Get(0), 5);
    EXPECT_EQ(seq.Get(1), 10);
    EXPECT_EQ(seq.Get(2), 20);
}

TEST(ArraySequenceTest, InsertAtBeginningMiddleAndEnd) {
    ArraySequence<std::string> seq;
    seq.Append("B");
    seq.Append("D");

    seq.InsertAt("A", 0);  // в начало
    seq.InsertAt("C", 2);  // в середину
    seq.InsertAt("E", 4);  // в конец (индекс == длине — допустимо)

    EXPECT_EQ(seq.GetLength(), 5u);
    EXPECT_EQ(seq[0], "A");
    EXPECT_EQ(seq[1], "B");
    EXPECT_EQ(seq[2], "C");
    EXPECT_EQ(seq[3], "D");
    EXPECT_EQ(seq[4], "E");
}

TEST(ArraySequenceTest, InsertOutOfRangeThrowsAndKeepsSequence) {
    ArraySequence<int> seq = MakeTens(2);
    EXPECT_THROW(seq.InsertAt(1, 3), std::out_of_range);
    EXPECT_EQ(seq.GetLength(), 2u);  // ничего не изменилось
}

// ============================================================================
// 4. Добавление элемента этой же последовательности (seq.Append(seq[0]))
// item — ссылка внутрь буфера; перевыделение или сдвиг не должны его испортить
// ============================================================================

TEST(ArraySequenceTest, AppendOwnElementWithReallocation) {
    ArraySequence<std::string> seq;
    for (int i = 0; i < 4; ++i)
        seq.Append(LongText(i));
    ASSERT_EQ(seq.GetLength(), seq.GetCapacity());  // буфер полон: Append перевыделит его

    seq.Append(seq[0]);
    EXPECT_EQ(seq.GetLast(), LongText(0));
    EXPECT_EQ(seq.GetFirst(), LongText(0));
}

TEST(ArraySequenceTest, InsertOwnElementWithShift) {
    ArraySequence<std::string> seq;
    seq.Append(LongText(0));
    seq.Append(LongText(1));
    seq.Append(LongText(2));  // ёмкость 4: перевыделения не будет, только сдвиг

    seq.InsertAt(seq[1], 0);
    EXPECT_EQ(seq[0], LongText(1));
    EXPECT_EQ(seq[1], LongText(0));
    EXPECT_EQ(seq[2], LongText(1));
    EXPECT_EQ(seq[3], LongText(2));
}

TEST(ArraySequenceTest, PrependOwnLastElement) {
    ArraySequence<std::string> seq;
    for (int i = 0; i < 4; ++i)
        seq.Append(LongText(i));

    seq.Prepend(seq.GetLast());
    EXPECT_EQ(seq.GetFirst(), LongText(3));
    EXPECT_EQ(seq.GetLast(), LongText(3));
    EXPECT_EQ(seq.GetLength(), 5u);
}

// ============================================================================
// 5. Удаление
// ============================================================================

TEST(ArraySequenceTest, RemoveAtBeginningMiddleAndEnd) {
    ArraySequence<int> seq = MakeTens(5);  // 10 20 30 40 50

    seq.RemoveAt(2);  // 10 20 40 50
    EXPECT_EQ(seq.Get(2), 40);
    seq.RemoveAt(0);  // 20 40 50
    EXPECT_EQ(seq.GetFirst(), 20);
    seq.RemoveAt(2);  // 20 40
    EXPECT_EQ(seq.GetLast(), 40);
    EXPECT_EQ(seq.GetLength(), 2u);
}

TEST(ArraySequenceTest, RemoveUntilEmpty) {
    ArraySequence<int> seq = MakeTens(3);
    seq.RemoveAt(0);
    seq.RemoveAt(0);
    seq.RemoveAt(0);
    EXPECT_TRUE(seq.IsEmpty());
    EXPECT_THROW(seq.RemoveAt(0), std::out_of_range);
}

TEST(ArraySequenceTest, RemoveOutOfRangeThrows) {
    ArraySequence<int> seq = MakeTens(2);
    EXPECT_THROW(seq.RemoveAt(2), std::out_of_range);
    EXPECT_EQ(seq.GetLength(), 2u);
}

// ============================================================================
// 6. Reserve и Swap
// ============================================================================

TEST(ArraySequenceTest, ReserveGrowsCapacityOnly) {
    ArraySequence<int> seq = MakeTens(3);
    seq.Reserve(50);
    EXPECT_EQ(seq.GetCapacity(), 50u);
    EXPECT_EQ(seq.GetLength(), 3u);  // длина не изменилась
    EXPECT_EQ(seq.GetLast(), 30);

    seq.Reserve(10);  // меньше текущей ёмкости — ничего не делает
    EXPECT_EQ(seq.GetCapacity(), 50u);
}

TEST(ArraySequenceTest, SwapExchangesContents) {
    ArraySequence<int> first = MakeTens(2);
    ArraySequence<int> second = MakeTens(5);

    first.Swap(second);
    EXPECT_EQ(first.GetLength(), 5u);
    EXPECT_EQ(second.GetLength(), 2u);
    EXPECT_EQ(first.GetLast(), 50);
    EXPECT_EQ(second.GetLast(), 20);
}

// ============================================================================
// 7. Копирование и перемещение
// ============================================================================

TEST(ArraySequenceTest, CopyIsIndependent) {
    ArraySequence<int> original = MakeTens(2);
    ArraySequence<int> copy = original;

    copy.Append(30);
    copy[0] = 999;
    EXPECT_EQ(original.GetLength(), 2u);  // оригинал не изменился
    EXPECT_EQ(original[0], 10);
    EXPECT_EQ(copy.GetLength(), 3u);
}

TEST(ArraySequenceTest, CopyAssignment) {
    ArraySequence<int> source = MakeTens(3);
    ArraySequence<int> target = MakeTens(1);

    target = source;
    EXPECT_EQ(target.GetLength(), 3u);
    EXPECT_EQ(target.GetLast(), 30);

    source[0] = -1;
    EXPECT_EQ(target[0], 10);  // глубокая копия
}

TEST(ArraySequenceTest, SelfCopyAssignment) {
    ArraySequence<int> seq = MakeTens(3);
    ArraySequence<int>& same = seq;  // через ссылку, иначе компилятор ругается
    seq = same;
    EXPECT_EQ(seq.GetLength(), 3u);
    EXPECT_EQ(seq.GetLast(), 30);
}

TEST(ArraySequenceTest, MoveConstructor) {
    ArraySequence<int> source = MakeTens(2);
    ArraySequence<int> destination = std::move(source);

    EXPECT_EQ(destination.GetLength(), 2u);
    EXPECT_EQ(destination[1], 20);
    EXPECT_TRUE(source.IsEmpty());
    EXPECT_EQ(source.GetCapacity(), 0u);
}

TEST(ArraySequenceTest, MoveAssignment) {
    ArraySequence<int> source = MakeTens(3);
    ArraySequence<int> target = MakeTens(1);

    target = std::move(source);
    EXPECT_EQ(target.GetLength(), 3u);
    EXPECT_TRUE(source.IsEmpty());

    source.Append(7);  // перемещённым объектом можно пользоваться дальше
    EXPECT_EQ(source.GetFirst(), 7);
}

TEST(ArraySequenceTest, SelfMoveAssignment) {
    ArraySequence<int> seq = MakeTens(3);
    ArraySequence<int>& same = seq;
    seq = std::move(same);
    EXPECT_EQ(seq.GetLength(), 3u);
    EXPECT_EQ(seq.GetLast(), 30);
}

// ============================================================================
// 8. Время жизни элементов и полиморфизм
// ============================================================================

TEST(ArraySequenceTest, AllObjectsAreDestroyed) {
    InstanceTracker::alive_count = 0;
    {
        ArraySequence<InstanceTracker> seq;
        for (int i = 0; i < 10; ++i)
            seq.Append(InstanceTracker(i));  // несколько перевыделений буфера
        seq.RemoveAt(3);
        ArraySequence<InstanceTracker> copy = seq;
        EXPECT_EQ(copy.Get(3).value, 4);
    }
    EXPECT_EQ(InstanceTracker::alive_count, 0);  // ни утечек, ни двойных удалений
}

TEST(ArraySequenceTest, WorksThroughSequenceInterface) {
    // Владеем через UnqPtr<Sequence<int>>: виртуальный деструктор Sequence
    // гарантирует, что удалится именно ArraySequence с его буфером
    UnqPtr<Sequence<int>> seq = MakeUnq<ArraySequence<int>>();
    seq->Append(1);
    seq->Append(2);
    seq->Prepend(0);

    EXPECT_EQ(seq->GetLength(), 3u);
    EXPECT_EQ(seq->GetFirst(), 0);
    EXPECT_EQ(seq->GetLast(), 2);
    EXPECT_EQ((*seq)[1], 1);
}
