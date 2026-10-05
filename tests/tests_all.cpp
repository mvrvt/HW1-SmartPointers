#include <gtest/gtest.h>

#include <cstddef>
#include <stdexcept>
#include <string>
#include <utility>

#include "ArraySequence.hpp"
#include "Sequence.hpp"
#include "pointers/MemorySpan.hpp"
#include "pointers/MsPtr.hpp"
#include "pointers/ShrdPtr.hpp"
#include "pointers/UnqPtr.hpp"
#include "test_helpers.hpp"

// Тесты UnqPtr перенесены в tests/test_unq_ptr.cpp

// ============================================================================
// 1. Тесты ShrdPtr<T> (Децентрализованное разделяемое владение)
// ============================================================================

TEST(ShrdPtrTest, ReferenceCountingAndCleanup) {
    InstanceTracker::alive_count = 0;
    {
        ShrdPtr<InstanceTracker> ptr1(UnqPtr<InstanceTracker>(new InstanceTracker(55)));
        EXPECT_EQ(InstanceTracker::alive_count, 1);
        EXPECT_EQ(ptr1.UseCount(), 1);
        EXPECT_EQ(ptr1->value, 55);

        {
            ShrdPtr<InstanceTracker> ptr2 = ptr1;
            EXPECT_EQ(ptr1.UseCount(), 2);
            EXPECT_EQ(ptr2.UseCount(), 2);
            EXPECT_EQ(InstanceTracker::alive_count, 1);

            ShrdPtr<InstanceTracker> ptr3;
            ptr3 = ptr2;
            EXPECT_EQ(ptr1.UseCount(), 3);
            EXPECT_EQ(ptr3.UseCount(), 3);
        }

        EXPECT_EQ(ptr1.UseCount(), 1);
        EXPECT_EQ(InstanceTracker::alive_count, 1);
    }
    EXPECT_EQ(InstanceTracker::alive_count, 0);
}

TEST(ShrdPtrTest, MoveSemanticsDoesNotChangeCounter) {
    InstanceTracker::alive_count = 0;
    {
        ShrdPtr<InstanceTracker> ptr1(UnqPtr<InstanceTracker>(new InstanceTracker(77)));
        EXPECT_EQ(ptr1.UseCount(), 1);

        ShrdPtr<InstanceTracker> ptr2 = std::move(ptr1);
        EXPECT_EQ(ptr1.Get(), nullptr);
        EXPECT_EQ(ptr1.UseCount(), 0);
        ASSERT_NE(ptr2.Get(), nullptr);
        EXPECT_EQ(ptr2.UseCount(), 1);
        EXPECT_EQ(InstanceTracker::alive_count, 1);
    }
    EXPECT_EQ(InstanceTracker::alive_count, 0);
}

TEST(ShrdPtrTest, SelfAssignmentIsSafe) {
    ShrdPtr<int> ptr(UnqPtr<int>(new int(42)));
    ptr = ptr;  // Не должно привести к падению или утечке
    EXPECT_EQ(ptr.UseCount(), 1);
    EXPECT_EQ(*ptr, 42);
}

// ============================================================================
// 2. Тесты MemorySpan<T> и MsPtr<T> (Безопасная арифметика)
// ============================================================================

TEST(MemorySpanTest, CreationAndElementAccess) {
    constexpr std::size_t kSize = 5;
    MemorySpan<int> span(kSize);
    EXPECT_EQ(span.Size(), kSize);

    for (std::size_t i = 0; i < kSize; ++i) {
        span[i] = static_cast<int>(i * 10);
    }

    UnqPtr<int> unq_copy = span.Get(2);
    EXPECT_EQ(*unq_copy, 20);

    ShrdPtr<int> shrd_copy = span.Copy(4);
    EXPECT_EQ(*shrd_copy, 40);

    // Выход за границы в Get и Copy
    EXPECT_THROW(span.Get(10), std::out_of_range);
    EXPECT_THROW(span.Copy(10), std::out_of_range);
}

TEST(MsPtrTest, ArithmeticAndNavigation) {
    constexpr std::size_t kSize = 6;
    MemorySpan<int> span(kSize);
    for (std::size_t i = 0; i < kSize; ++i) {
        span[i] = static_cast<int>(i + 1);
    }

    MsPtr<int> it = span.Locate(0);
    EXPECT_EQ(*it, 1);

    // Префиксный и постфиксный инкремент
    EXPECT_EQ(*(++it), 2);
    EXPECT_EQ(*(it++), 2);
    EXPECT_EQ(*it, 3);

    // Сложение со смещением
    MsPtr<int> advanced = it + 2;
    EXPECT_EQ(*advanced, 5);

    // Разность указателей
    EXPECT_EQ(advanced - it, 2);
    EXPECT_EQ(it - advanced, -2);

    // Оператор индексации
    EXPECT_EQ(it[1], 4);
    EXPECT_EQ(it[-1], 2);
}

TEST(MsPtrTest, Comparisons) {
    MemorySpan<int> span(5);
    MsPtr<int> first = span.Locate(1);
    MsPtr<int> second = span.Locate(3);
    MsPtr<int> first_dup = span.Locate(1);

    EXPECT_TRUE(first == first_dup);
    EXPECT_TRUE(first != second);
    EXPECT_TRUE(first < second);
    EXPECT_TRUE(second > first);
    EXPECT_TRUE(first <= first_dup);
    EXPECT_TRUE(first >= first_dup);
}

TEST(MsPtrTest, BoundaryProtectionThrows) {
    MemorySpan<int> span(4);
    MsPtr<int> it = span.Locate(0);

    // Шаг влево от нуля
    EXPECT_THROW(it - 1, std::out_of_range);
    EXPECT_THROW(it -= 1, std::out_of_range);

    // Шаг вправо за пределы size
    EXPECT_THROW(it + 10, std::out_of_range);

    // Позиция size (one-past-the-end) валидна для итератора, но не для чтения!
    MsPtr<int> end_it = span.Locate(4);
    EXPECT_TRUE(end_it == span.Locate(4));
    EXPECT_THROW(*end_it, std::out_of_range);
    EXPECT_THROW(end_it.operator->(), std::out_of_range);

    // Запрет сравнения или вычитания указателей из РАЗНЫХ span
    MemorySpan<int> another_span(4);
    MsPtr<int> other_it = another_span.Locate(0);
    EXPECT_THROW(it - other_it, std::invalid_argument);
    EXPECT_THROW((void)(it < other_it), std::invalid_argument);
}

// ============================================================================
// 3. Тесты ArraySequence<T> (Контейнер на UnqPtr<T[]>)
// ============================================================================

TEST(ArraySequenceTest, DefaultAndEmptyState) {
    ArraySequence<int> seq;
    EXPECT_TRUE(seq.IsEmpty());
    EXPECT_EQ(seq.GetLength(), 0);
    EXPECT_THROW(seq.GetFirst(), std::out_of_range);
    EXPECT_THROW(seq.GetLast(), std::out_of_range);
    EXPECT_THROW(seq.Get(0), std::out_of_range);
}

TEST(ArraySequenceTest, SizedConstructorInitializes) {
    ArraySequence<int> seq(5);
    EXPECT_FALSE(seq.IsEmpty());
    EXPECT_EQ(seq.GetLength(), 5);
    for (std::size_t i = 0; i < 5; ++i) {
        EXPECT_EQ(seq.Get(i), 0);
    }
}

TEST(ArraySequenceTest, ArrayConstructorWithNullptrGuard) {
    EXPECT_THROW(ArraySequence<int>(nullptr, 5), std::invalid_argument);

    int initial_values[] = {1, 2, 3};
    ArraySequence<int> seq(initial_values, 3);
    EXPECT_EQ(seq.GetLength(), 3);
    EXPECT_EQ(seq.Get(0), 1);
    EXPECT_EQ(seq.Get(2), 3);
}

TEST(ArraySequenceTest, AppendAndPrepend) {
    ArraySequence<int> seq;
    seq.Append(10);
    seq.Append(20);
    seq.Prepend(5);

    EXPECT_EQ(seq.GetLength(), 3);
    EXPECT_EQ(seq.GetFirst(), 5);
    EXPECT_EQ(seq.Get(1), 10);
    EXPECT_EQ(seq.GetLast(), 20);
}

TEST(ArraySequenceTest, InsertAtAndRemoveAt) {
    ArraySequence<std::string> seq;
    seq.Append("B");
    seq.Append("D");

    seq.InsertAt("A", 0);   // В начало
    seq.InsertAt("C", 2);   // В середину
    seq.InsertAt("E", 4);   // В конец
    EXPECT_THROW(seq.InsertAt("X", 10), std::out_of_range);

    EXPECT_EQ(seq.GetLength(), 5);
    EXPECT_EQ(seq[0], "A");
    EXPECT_EQ(seq[1], "B");
    EXPECT_EQ(seq[2], "C");
    EXPECT_EQ(seq[3], "D");
    EXPECT_EQ(seq[4], "E");

    seq.RemoveAt(2);  // Удаляем "C"
    EXPECT_EQ(seq.GetLength(), 4);
    EXPECT_EQ(seq[2], "D");

    seq.RemoveAt(0);  // Удаляем "A"
    EXPECT_EQ(seq.GetLength(), 3);
    EXPECT_EQ(seq[0], "B");

    EXPECT_THROW(seq.RemoveAt(10), std::out_of_range);
}

TEST(ArraySequenceTest, DeepCopyIndependence) {
    ArraySequence<int> original;
    original.Append(1);
    original.Append(2);

    ArraySequence<int> copy = original;
    copy.Append(3);
    copy[0] = 999;

    // Изменение копии не должно отразиться на оригинале
    EXPECT_EQ(original.GetLength(), 2);
    EXPECT_EQ(original[0], 1);
    EXPECT_EQ(copy.GetLength(), 3);
    EXPECT_EQ(copy[0], 999);
}

TEST(ArraySequenceTest, MoveSemantics) {
    ArraySequence<int> source;
    source.Append(100);
    source.Append(200);

    ArraySequence<int> destination = std::move(source);
    EXPECT_EQ(source.GetLength(), 0);
    EXPECT_EQ(destination.GetLength(), 2);
    EXPECT_EQ(destination[0], 100);
    EXPECT_EQ(destination[1], 200);
}

TEST(ArraySequenceTest, PolymorphicSequenceInterface) {
    // Проверка чисто виртуального контракта Sequence<T>
    Sequence<int>* polymorphic_seq = new ArraySequence<int>();
    polymorphic_seq->Append(42);
    EXPECT_EQ(polymorphic_seq->GetLength(), 1);
    EXPECT_EQ(polymorphic_seq->GetFirst(), 42);

    delete polymorphic_seq;  // Виртуальный деструктор освобождает буфер без утечек
}
