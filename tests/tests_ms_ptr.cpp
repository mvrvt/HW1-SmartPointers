#include <gtest/gtest.h>

#include <cstddef>
#include <limits>
#include <stdexcept>
#include <type_traits>

#include "pointers/MemorySpan.hpp"
#include "pointers/MsPtr.hpp"
#include "test_helpers.hpp"

// Заполняет span значениями 10, 20, 30, ... (через MsPtr — другого доступа нет)
void FillWithTens(MemorySpan<int>& span) {
    for (size_t i = 0; i < span.Size(); ++i)
        *span.Locate(i) = static_cast<int>((i + 1) * 10);
}

const ptrdiff_t kMaxOffset = std::numeric_limits<ptrdiff_t>::max();
const ptrdiff_t kMinOffset = std::numeric_limits<ptrdiff_t>::min();

// ============================================================================
// 0. Проверки во время компиляции
// ============================================================================

// MsPtr ничем не владеет (правило нуля): копируется побитово, деструктор ничего не делает
static_assert(std::is_trivially_copyable_v<MsPtr<int>>);
static_assert(std::is_trivially_destructible_v<MsPtr<int>>);
static_assert(std::is_nothrow_default_constructible_v<MsPtr<int>>);

// Размер: адрес MemorySpan + индекс
static_assert(sizeof(MsPtr<int>) == sizeof(MemorySpan<int>*) + sizeof(size_t));

// Привязанный MsPtr нельзя создать в обход MemorySpan::Locate: конструктор закрыт
static_assert(!std::is_constructible_v<MsPtr<int>, MemorySpan<int>*, size_t>);

// Псевдонимы типов
static_assert(std::is_same_v<MsPtr<int>::ElementType, int>);
static_assert(std::is_same_v<MsPtr<int>::Pointer, int*>);

// ============================================================================
// 1. Создание
// ============================================================================

TEST(MsPtrTest, DefaultPointerIsUnbound) {
    MsPtr<int> ptr;
    EXPECT_EQ(ptr.GetSpan(), nullptr);
    EXPECT_EQ(ptr.GetIndex(), 0u);
    EXPECT_FALSE(ptr.IsEnd());
    EXPECT_TRUE(ptr == MsPtr<int>());  // два непривязанных указателя равны
}

TEST(MsPtrTest, LocateBindsPointerToSpan) {
    MemorySpan<int> span(4);
    MsPtr<int> ptr = span.Locate(2);
    EXPECT_EQ(ptr.GetSpan(), &span);
    EXPECT_EQ(ptr.GetIndex(), 2u);
    EXPECT_FALSE(ptr.IsEnd());
}

TEST(MsPtrTest, PositionAfterLastIsEnd) {
    MemorySpan<int> span(4);
    MsPtr<int> end = span.Locate(4);
    EXPECT_TRUE(end.IsEnd());
    EXPECT_TRUE(end == span.End());
}

TEST(MsPtrTest, CopyIsIndependentPosition) {
    MemorySpan<int> span(4);
    MsPtr<int> first = span.Locate(1);
    MsPtr<int> second = first;

    ++second;
    EXPECT_EQ(first.GetIndex(), 1u);  // копия сдвинулась, оригинал — нет
    EXPECT_EQ(second.GetIndex(), 2u);
}

// ============================================================================
// 2. Доступ к элементу: *, ->, []
// ============================================================================

TEST(MsPtrTest, DereferenceReadsAndWrites) {
    MemorySpan<int> span(4);
    FillWithTens(span);
    MsPtr<int> ptr = span.Locate(1);

    EXPECT_EQ(*ptr, 20);
    *ptr = 99;
    EXPECT_EQ(*span.Get(1), 99);  // изменение видно в самом массиве
}

TEST(MsPtrTest, ArrowAccessesMember) {
    MemorySpan<InstanceTracker> span(3);
    MsPtr<InstanceTracker> ptr = span.Locate(2);

    ptr->value = 7;
    EXPECT_EQ(ptr->value, 7);
    EXPECT_EQ(span.Get(2)->value, 7);
}

TEST(MsPtrTest, IndexingIsRelativeToCurrentPosition) {
    MemorySpan<int> span(5);
    FillWithTens(span);
    MsPtr<int> ptr = span.Locate(2);  // стоим на 30

    EXPECT_EQ(ptr[0], 30);
    EXPECT_EQ(ptr[2], 50);
    EXPECT_EQ(ptr[-2], 10);

    ptr[1] = 400;
    EXPECT_EQ(*span.Get(3), 400);
    EXPECT_EQ(ptr.GetIndex(), 2u);  // [] не сдвигает указатель
}

TEST(MsPtrTest, DereferencingEndThrows) {
    MemorySpan<int> span(4);
    MsPtr<int> end = span.End();

    EXPECT_THROW(*end, std::out_of_range);
    EXPECT_THROW(end.operator->(), std::out_of_range);
    EXPECT_THROW(end[0], std::out_of_range);
}

TEST(MsPtrTest, IndexingOutOfBoundsThrows) {
    MemorySpan<int> span(4);
    MsPtr<int> ptr = span.Locate(1);

    EXPECT_THROW(ptr[3], std::out_of_range);   // индекс 4 — это end, читать нельзя
    EXPECT_THROW(ptr[-2], std::out_of_range);  // индекс -1
    EXPECT_NO_THROW(ptr[2]);                    // индекс 3 — последний элемент
    EXPECT_NO_THROW(ptr[-1]);                   // индекс 0 — первый элемент
}

// ============================================================================
// 3. Сдвиги: ++, --, +=, -=, +, -
// ============================================================================

TEST(MsPtrTest, PrefixAndPostfixIncrement) {
    MemorySpan<int> span(4);
    FillWithTens(span);
    MsPtr<int> ptr = span.Begin();

    EXPECT_EQ(*(++ptr), 20);    // префиксный: сначала сдвиг, потом значение
    EXPECT_EQ(*(ptr++), 20);    // постфиксный: возвращает старую позицию
    EXPECT_EQ(*ptr, 30);
}

TEST(MsPtrTest, PrefixAndPostfixDecrement) {
    MemorySpan<int> span(4);
    FillWithTens(span);
    MsPtr<int> ptr = span.End();

    EXPECT_EQ(*(--ptr), 40);
    EXPECT_EQ(*(ptr--), 40);
    EXPECT_EQ(*ptr, 30);
}

TEST(MsPtrTest, CompoundAssignmentMovesPointer) {
    MemorySpan<int> span(5);
    FillWithTens(span);
    MsPtr<int> ptr = span.Begin();

    ptr += 3;
    EXPECT_EQ(*ptr, 40);
    ptr -= 2;
    EXPECT_EQ(*ptr, 20);
    ptr += -1;  // отрицательный сдвиг тоже законен
    EXPECT_EQ(*ptr, 10);
    ptr += 0;
    EXPECT_EQ(ptr.GetIndex(), 0u);
}

TEST(MsPtrTest, PlusAndMinusDoNotChangeOriginal) {
    MemorySpan<int> span(5);
    FillWithTens(span);
    MsPtr<int> ptr = span.Locate(2);

    MsPtr<int> forward = ptr + 2;
    MsPtr<int> backward = ptr - 2;
    EXPECT_EQ(*forward, 50);
    EXPECT_EQ(*backward, 10);
    EXPECT_EQ(ptr.GetIndex(), 2u);
}

TEST(MsPtrTest, NumberPlusPointer) {
    MemorySpan<int> span(4);
    FillWithTens(span);
    MsPtr<int> ptr = span.Begin();

    EXPECT_TRUE(2 + ptr == ptr + 2);  // как у обычных указателей
    EXPECT_EQ(*(2 + ptr), 30);
}

TEST(MsPtrTest, CanMoveToEndButNotPast) {
    MemorySpan<int> span(4);
    MsPtr<int> ptr = span.Begin();

    ptr += 4;
    EXPECT_TRUE(ptr.IsEnd());
    EXPECT_THROW(++ptr, std::out_of_range);
    EXPECT_THROW(ptr += 1, std::out_of_range);
    EXPECT_THROW(ptr + 1, std::out_of_range);
}

TEST(MsPtrTest, CannotMoveBeforeBegin) {
    MemorySpan<int> span(4);
    MsPtr<int> ptr = span.Begin();

    EXPECT_THROW(--ptr, std::out_of_range);
    EXPECT_THROW(ptr--, std::out_of_range);
    EXPECT_THROW(ptr -= 1, std::out_of_range);
    EXPECT_THROW(ptr - 1, std::out_of_range);
    EXPECT_THROW(ptr += -1, std::out_of_range);
}

// Если сдвиг не удался, указатель остаётся на месте (строгая гарантия)
TEST(MsPtrTest, FailedMoveKeepsPosition) {
    MemorySpan<int> span(4);
    MsPtr<int> ptr = span.Locate(1);

    EXPECT_THROW(ptr += 10, std::out_of_range);
    EXPECT_EQ(ptr.GetIndex(), 1u);
    EXPECT_THROW(ptr -= 10, std::out_of_range);
    EXPECT_EQ(ptr.GetIndex(), 1u);

    MsPtr<int> end = span.End();
    EXPECT_THROW(end++, std::out_of_range);
    EXPECT_TRUE(end.IsEnd());
}

// Огромные сдвиги: проверка не должна переполнять ptrdiff_t (это было бы UB)
TEST(MsPtrTest, HugeOffsetsThrowWithoutOverflow) {
    MemorySpan<int> span(4);
    MsPtr<int> ptr = span.Locate(2);

    EXPECT_THROW(ptr += kMaxOffset, std::out_of_range);
    EXPECT_THROW(ptr += kMinOffset, std::out_of_range);
    EXPECT_THROW(ptr -= kMaxOffset, std::out_of_range);
    EXPECT_THROW(ptr -= kMinOffset, std::out_of_range);  // -kMinOffset не помещается в ptrdiff_t
    EXPECT_THROW(ptr[kMaxOffset], std::out_of_range);
    EXPECT_THROW(ptr[kMinOffset], std::out_of_range);
    EXPECT_EQ(ptr.GetIndex(), 2u);
}

// ============================================================================
// 4. Расстояние между указателями
// ============================================================================

TEST(MsPtrTest, DistanceBetweenPointers) {
    MemorySpan<int> span(5);
    MsPtr<int> begin = span.Begin();
    MsPtr<int> end = span.End();

    EXPECT_EQ(end - begin, 5);   // число элементов
    EXPECT_EQ(begin - end, -5);
    EXPECT_EQ((begin + 3) - begin, 3);
    EXPECT_EQ(begin - begin, 0);
}

TEST(MsPtrTest, DistanceBetweenDifferentSpansThrows) {
    MemorySpan<int> first(4);
    MemorySpan<int> second(4);
    EXPECT_THROW(first.Begin() - second.Begin(), std::invalid_argument);
}

// ============================================================================
// 5. Сравнения
// ============================================================================

TEST(MsPtrTest, EqualityComparesSpanAndIndex) {
    MemorySpan<int> span(4);
    MemorySpan<int> other_span(4);

    EXPECT_TRUE(span.Locate(1) == span.Locate(1));
    EXPECT_TRUE(span.Locate(1) != span.Locate(2));
    EXPECT_TRUE(span.Locate(1) != other_span.Locate(1));  // тот же индекс, но другой массив
}

TEST(MsPtrTest, OrderingInsideOneSpan) {
    MemorySpan<int> span(5);
    MsPtr<int> first = span.Locate(1);
    MsPtr<int> second = span.Locate(3);

    EXPECT_TRUE(first < second);
    EXPECT_FALSE(second < first);
    EXPECT_TRUE(second > first);
    EXPECT_TRUE(first <= second);
    EXPECT_TRUE(first <= span.Locate(1));
    EXPECT_TRUE(second >= first);
    EXPECT_TRUE(second >= span.Locate(3));
    EXPECT_FALSE(first < span.Locate(1));
}

TEST(MsPtrTest, OrderingBetweenDifferentSpansThrows) {
    MemorySpan<int> first_span(4);
    MemorySpan<int> second_span(4);
    MsPtr<int> first = first_span.Begin();
    MsPtr<int> second = second_span.Begin();

    EXPECT_THROW((void)(first < second), std::invalid_argument);
    EXPECT_THROW((void)(first > second), std::invalid_argument);
    EXPECT_THROW((void)(first <= second), std::invalid_argument);
    EXPECT_THROW((void)(first >= second), std::invalid_argument);
}

// ============================================================================
// 6. Обход массива
// ============================================================================

TEST(MsPtrTest, ForwardIterationVisitsEveryElement) {
    MemorySpan<int> span(4);
    FillWithTens(span);

    int sum = 0;
    size_t steps = 0;
    for (MsPtr<int> ptr = span.Begin(); ptr != span.End(); ++ptr) {
        sum += *ptr;
        ++steps;
    }
    EXPECT_EQ(sum, 100);
    EXPECT_EQ(steps, 4u);
}

TEST(MsPtrTest, BackwardIterationVisitsEveryElement) {
    MemorySpan<int> span(4);
    FillWithTens(span);

    int last_seen = 0;
    MsPtr<int> ptr = span.End();
    while (ptr != span.Begin()) {
        --ptr;
        last_seen = *ptr;
    }
    EXPECT_EQ(last_seen, 10);
}

TEST(MsPtrTest, IterationOverEmptySpanDoesNothing) {
    MemorySpan<int> span;
    EXPECT_TRUE(span.Begin() == span.End());
    EXPECT_TRUE(span.Begin().IsEnd());
    EXPECT_THROW(*span.Begin(), std::out_of_range);
    EXPECT_THROW(++span.Begin(), std::out_of_range);
}

// ============================================================================
// 7. Непривязанный MsPtr: любая операция бросает logic_error
// ============================================================================

TEST(MsPtrTest, UnboundPointerThrowsOnUse) {
    MsPtr<InstanceTracker> ptr;

    EXPECT_THROW(*ptr, std::logic_error);
    EXPECT_THROW(ptr.operator->(), std::logic_error);
    EXPECT_THROW(ptr[0], std::logic_error);
    EXPECT_THROW(++ptr, std::logic_error);
    EXPECT_THROW(--ptr, std::logic_error);
    EXPECT_THROW(ptr += 1, std::logic_error);
    EXPECT_THROW(ptr + 1, std::logic_error);
}

TEST(MsPtrTest, UnboundAndBoundPointersAreDifferent) {
    MemorySpan<int> span(4);
    MsPtr<int> unbound;
    MsPtr<int> bound = span.Begin();

    EXPECT_TRUE(unbound != bound);
    EXPECT_THROW(bound - unbound, std::invalid_argument);
}

TEST(MsPtrTest, DefaultPointerCanBeBoundLater) {
    MemorySpan<int> span(4);
    FillWithTens(span);

    MsPtr<int> ptr;
    ptr = span.Locate(3);
    EXPECT_EQ(*ptr, 40);
}

// ============================================================================
// 8. MsPtr не владеет элементами
// ============================================================================

TEST(MsPtrTest, PointersDoNotDeleteElements) {
    InstanceTracker::alive_count = 0;
    {
        MemorySpan<InstanceTracker> span(3);
        EXPECT_EQ(InstanceTracker::alive_count, 3);
        {
            MsPtr<InstanceTracker> first = span.Begin();
            MsPtr<InstanceTracker> copy = first;
            ++copy;
        }
        // указатели умерли, а элементы живы: ими владеет MemorySpan
        EXPECT_EQ(InstanceTracker::alive_count, 3);
    }
    EXPECT_EQ(InstanceTracker::alive_count, 0);  // удалил MemorySpan
}
