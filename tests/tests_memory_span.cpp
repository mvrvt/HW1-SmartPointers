#include <gtest/gtest.h>

#include <cstddef>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

#include "pointers/MemorySpan.hpp"
#include "test_helpers.hpp"

// Можно ли написать span[0]? Для MemorySpan ответ должен быть «нет»:
// по ТЗ доступ к элементам — только через умные указатели.
template <typename Span>
concept HasPublicIndexing = requires(Span& span) { span[0]; };

// ============================================================================
// 0. Проверки во время компиляции
// ============================================================================

// Нет span[i] снаружи — только Get / Copy / Locate
static_assert(!HasPublicIndexing<MemorySpan<int>>);

// Копировать нельзя (единственный владелец элементов), перемещать можно, без исключений
static_assert(!std::is_copy_constructible_v<MemorySpan<int>>);
static_assert(!std::is_copy_assignable_v<MemorySpan<int>>);
static_assert(std::is_nothrow_move_constructible_v<MemorySpan<int>>);
static_assert(std::is_nothrow_move_assignable_v<MemorySpan<int>>);

// explicit: MemorySpan<int> span(5) можно, MemorySpan<int> span = 5 нельзя
static_assert(std::is_constructible_v<MemorySpan<int>, size_t>);
static_assert(!std::is_convertible_v<size_t, MemorySpan<int>>);

static_assert(std::is_same_v<MemorySpan<int>::ElementType, int>);

// ============================================================================
// 1. Создание
// ============================================================================

TEST(MemorySpanTest, DefaultSpanIsEmpty) {
    MemorySpan<int> span;
    EXPECT_EQ(span.Size(), 0u);
    EXPECT_TRUE(span.IsEmpty());
    EXPECT_TRUE(span.Begin() == span.End());
}

TEST(MemorySpanTest, SizedSpanIsFilledWithZeros) {
    MemorySpan<int> span(5);
    EXPECT_EQ(span.Size(), 5u);
    EXPECT_FALSE(span.IsEmpty());
    for (size_t i = 0; i < span.Size(); ++i)
        EXPECT_EQ(*span.Get(i), 0);
}

TEST(MemorySpanTest, ZeroSizedSpanIsEmpty) {
    MemorySpan<int> span(0);
    EXPECT_TRUE(span.IsEmpty());
    EXPECT_THROW(span.Get(0), std::out_of_range);
}

TEST(MemorySpanTest, SizedSpanCreatesAndDeletesEveryElement) {
    InstanceTracker::alive_count = 0;
    {
        MemorySpan<InstanceTracker> span(4);
        EXPECT_EQ(InstanceTracker::alive_count, 4);
    }
    EXPECT_EQ(InstanceTracker::alive_count, 0);  // деструктор удалил все 4
}

TEST(MemorySpanTest, CreateFromArrayCopiesItems) {
    int items[] = {1, 2, 3};
    MemorySpan<int> span(items, 3);

    EXPECT_EQ(span.Size(), 3u);
    EXPECT_EQ(*span.Get(0), 1);
    EXPECT_EQ(*span.Get(2), 3);

    items[0] = 100;  // исходный массив и MemorySpan независимы
    EXPECT_EQ(*span.Get(0), 1);
}

TEST(MemorySpanTest, CreateFromNullArray) {
    EXPECT_THROW(MemorySpan<int>(nullptr, 3), std::invalid_argument);

    MemorySpan<int> empty(nullptr, 0);  // ноль элементов из nullptr — допустимо
    EXPECT_TRUE(empty.IsEmpty());
}

TEST(MemorySpanTest, WorksWithStrings) {
    std::string words[] = {"one", "two"};
    MemorySpan<std::string> span(words, 2);

    *span.Locate(1) += "!";
    EXPECT_EQ(*span.Get(1), "two!");
    EXPECT_EQ(span.Locate(0)->size(), 3u);
}

// ============================================================================
// 2. Get — UnqPtr на независимую копию элемента
// ============================================================================

TEST(MemorySpanTest, GetReturnsUnqPtrToCopy) {
    int items[] = {10, 20, 30};
    MemorySpan<int> span(items, 3);

    UnqPtr<int> copy = span.Get(1);
    static_assert(std::is_same_v<decltype(span.Get(1)), UnqPtr<int>>);
    EXPECT_EQ(*copy, 20);

    *copy = 999;                // меняем копию...
    EXPECT_EQ(*span.Get(1), 20);  // ...массив не изменился
}

TEST(MemorySpanTest, GetCopyOutlivesSpan) {
    InstanceTracker::alive_count = 0;
    UnqPtr<InstanceTracker> copy;
    {
        MemorySpan<InstanceTracker> span(2);
        span.Locate(0)->value = 5;
        copy = span.Get(0);
        EXPECT_EQ(InstanceTracker::alive_count, 3);  // 2 в массиве + копия
    }
    EXPECT_EQ(InstanceTracker::alive_count, 1);  // массив удалён, копия жива
    EXPECT_EQ(copy->value, 5);
}

TEST(MemorySpanTest, GetOutOfRangeThrows) {
    MemorySpan<int> span(3);
    EXPECT_THROW(span.Get(3), std::out_of_range);  // индекс == Size()
    EXPECT_THROW(span.Get(100), std::out_of_range);
    EXPECT_NO_THROW(span.Get(2));
}

// ============================================================================
// 3. Copy — ShrdPtr на копию элемента
// ============================================================================

TEST(MemorySpanTest, CopyReturnsShrdPtrToCopy) {
    int items[] = {10, 20, 30};
    MemorySpan<int> span(items, 3);

    ShrdPtr<int> copy = span.Copy(2);
    static_assert(std::is_same_v<decltype(span.Copy(2)), ShrdPtr<int>>);
    EXPECT_EQ(*copy, 30);
    EXPECT_EQ(copy.UseCount(), 1u);

    *copy = 999;
    EXPECT_EQ(*span.Get(2), 30);  // массив не изменился
}

TEST(MemorySpanTest, CopyCanBeShared) {
    MemorySpan<int> span(2);
    ShrdPtr<int> first = span.Copy(0);
    ShrdPtr<int> second = first;

    *first = 7;
    EXPECT_EQ(*second, 7);  // общий объект у копий ShrdPtr
    EXPECT_EQ(first.UseCount(), 2u);
}

TEST(MemorySpanTest, CopyOutOfRangeThrows) {
    MemorySpan<int> span(3);
    EXPECT_THROW(span.Copy(3), std::out_of_range);
    EXPECT_THROW(span.Copy(100), std::out_of_range);
}

// ============================================================================
// 4. Locate, Begin, End — MsPtr на сам элемент
// ============================================================================

TEST(MemorySpanTest, LocateGivesAccessToElementItself) {
    MemorySpan<int> span(3);
    static_assert(std::is_same_v<decltype(span.Locate(0)), MsPtr<int>>);

    *span.Locate(1) = 42;           // меняем через MsPtr...
    EXPECT_EQ(*span.Get(1), 42);    // ...и изменился сам массив
}

TEST(MemorySpanTest, LocateAllowsEndPosition) {
    MemorySpan<int> span(3);
    EXPECT_NO_THROW(span.Locate(3));             // Size() — позиция «за последним»
    EXPECT_TRUE(span.Locate(3).IsEnd());
    EXPECT_THROW(span.Locate(4), std::out_of_range);
    EXPECT_THROW(span.Locate(100), std::out_of_range);
}

TEST(MemorySpanTest, BeginAndEnd) {
    MemorySpan<int> span(4);
    EXPECT_TRUE(span.Begin() == span.Locate(0));
    EXPECT_TRUE(span.End() == span.Locate(4));
    EXPECT_EQ(span.End() - span.Begin(), 4);
}

// ============================================================================
// 5. Перемещение
// ============================================================================

TEST(MemorySpanTest, MoveConstructorTakesElements) {
    InstanceTracker::alive_count = 0;
    {
        MemorySpan<InstanceTracker> source(3);
        source.Locate(0)->value = 5;

        MemorySpan<InstanceTracker> destination = std::move(source);
        EXPECT_EQ(InstanceTracker::alive_count, 3);  // элементы не копировались
        EXPECT_EQ(destination.Size(), 3u);
        EXPECT_EQ(destination.Get(0)->value, 5);

        EXPECT_EQ(source.Size(), 0u);  // источник стал пустым
        EXPECT_TRUE(source.IsEmpty());
        EXPECT_THROW(source.Get(0), std::out_of_range);
    }
    EXPECT_EQ(InstanceTracker::alive_count, 0);
}

TEST(MemorySpanTest, MoveAssignmentDeletesOldElements) {
    InstanceTracker::alive_count = 0;
    {
        MemorySpan<InstanceTracker> first(2);
        MemorySpan<InstanceTracker> second(3);
        EXPECT_EQ(InstanceTracker::alive_count, 5);

        first = std::move(second);
        EXPECT_EQ(InstanceTracker::alive_count, 3);  // старые 2 элемента first удалены
        EXPECT_EQ(first.Size(), 3u);
        EXPECT_TRUE(second.IsEmpty());
    }
    EXPECT_EQ(InstanceTracker::alive_count, 0);
}

TEST(MemorySpanTest, SelfMoveAssignmentKeepsElements) {
    int items[] = {1, 2, 3};
    MemorySpan<int> span(items, 3);
    MemorySpan<int>& same_span = span;  // через ссылку, иначе компилятор ругается

    span = std::move(same_span);
    EXPECT_EQ(span.Size(), 3u);
    EXPECT_EQ(*span.Get(2), 3);
}

// MsPtr привязан к объекту MemorySpan. После перемещения источник пуст,
// и старый MsPtr бросает исключение, а не читает чужую память.
TEST(MemorySpanTest, PointerToMovedFromSpanThrows) {
    MemorySpan<int> source(3);
    MsPtr<int> ptr = source.Locate(1);

    MemorySpan<int> destination = std::move(source);
    EXPECT_THROW(*ptr, std::out_of_range);
    EXPECT_THROW(++ptr, std::out_of_range);
    EXPECT_NO_THROW(*destination.Locate(1));
}
