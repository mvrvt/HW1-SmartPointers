#include <gtest/gtest.h>

#include <cstddef>
#include <stdexcept>
#include <string>
#include <utility>

#include "ArraySequence.hpp"
#include "Sequence.hpp"
#include "test_helpers.hpp"

// Тесты указателей — в tests/tests_unq_ptr.cpp, tests_shrd_ptr.cpp, tests_ms_ptr.cpp,
// tests_memory_span.cpp. Здесь — тесты контейнера ArraySequence.

// ============================================================================
// Тесты ArraySequence<T> (Контейнер на UnqPtr<T[]>)
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
