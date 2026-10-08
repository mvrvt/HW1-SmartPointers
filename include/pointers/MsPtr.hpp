#pragma once

#include <cstddef>   // size_t, ptrdiff_t
#include <limits>    // std::numeric_limits
#include <memory>    // std::addressof
#include <stdexcept> // std::out_of_range, std::invalid_argument, std::logic_error

// Forward declaration
template <typename T>
class MemorySpan;

// ========== MsPtr<T> — безопасный указатель с арифметикой внутри MemorySpan ==========
// Привязан к одному конкретному MemorySpan и хранит номер элемента (индекс).
// Может стоять на любом элементе [0, Size()) или на позиции «за последним» (Size()),
// но никогда — за границами массива: любой выход за них бросает исключение.
// MsPtr НЕ владеет элементами: их хранит и удаляет MemorySpan.
template <typename T>
class MsPtr {
public:
    using ElementType = T;
    using Pointer = T*;

    //TODO: почему и зачем в подсчёте индекса указателя в этом классе мы используем именно ptrdiff_t, а затем явно 

    // Пустой указатель, ни к чему не привязан. Создать привязанный можно только через MemorySpan::Locate
    MsPtr() noexcept : span_(nullptr), index_(0) {}

    // Копирование, присваивание и деструктор — те, что генерирует компилятор:
    // MsPtr ничем не владеет, копия — просто ещё один указатель в тот же массив.

    // ---------- Доступ к элементу: только к существующему, [0, Size()) ----------
    T& operator*() const {
        CheckReadable(0); //TODO: почему 0? 
        return (*span_)[index_];
    }

    T* operator->() const {
        CheckReadable(0);
        return std::addressof((*span_)[index_]); //TODO: что это такое 
    }

    // it[offset] — элемент, отстоящий от текущего на offset (как *(it + offset))
    T& operator[](ptrdiff_t offset) const {
        CheckReadable(offset);
        return (*span_)[static_cast<size_t>(static_cast<ptrdiff_t>(index_) + offset)];
    }

    // ---------- Арифметика: результат в [0, Size()], включая позицию «за последним» ----------
    // Сначала проверка, потом изменение: если бросили исключение, указатель не изменился.
    MsPtr& operator+=(ptrdiff_t offset) {
        CheckMovable(offset);
        index_ = static_cast<size_t>(static_cast<ptrdiff_t>(index_) + offset);
        return *this;
    }

    MsPtr& operator-=(ptrdiff_t offset) {
        // -PTRDIFF_MIN не помещается в ptrdiff_t (переполнение — UB),
        // а такой сдвиг в любом случае выводит за границы любого массива
        if (offset == std::numeric_limits<ptrdiff_t>::min()) //TODO: почему мы проверяем что offset не равен минимуму? Не разумнее ли будет сравнивать с -PTRDIFF_T *this - offset? 
            throw std::out_of_range("MsPtr moved out of MemorySpan bounds");
        return *this += -offset;
    }

    MsPtr operator+(ptrdiff_t offset) const {
        MsPtr result = *this;
        result += offset;
        return result;
    }

    MsPtr operator-(ptrdiff_t offset) const {
        MsPtr result = *this;
        result -= offset;
        return result;
    }

    //TODO: Вообще непонятно что это, для чего оно нужно и как оно работает
    // 2 + it — как у обычного указателя
    friend MsPtr operator+(ptrdiff_t offset, const MsPtr& ptr) {
        return ptr + offset;
    }

    MsPtr& operator++() {
        return *this += 1;
    }

    MsPtr operator++(int) {
        MsPtr old = *this;
        *this += 1;
        return old;
    }

    MsPtr& operator--() {
        return *this -= 1;
    }

    MsPtr operator--(int) {
        MsPtr old = *this;
        *this -= 1;
        return old;
    }

    // Расстояние между указателями одного MemorySpan: (it + n) - it == n.
    ptrdiff_t operator-(const MsPtr& other) const {
        CheckSameSpan(other);
        return static_cast<ptrdiff_t>(index_) - static_cast<ptrdiff_t>(other.index_);
    }

    // ---------- Сравнения ----------
    // Равенство осмысленно всегда: указатели в разные массивы просто не равны.
    // В C++20 из == компилятор сам выводит !=.
    bool operator==(const MsPtr& other) const noexcept {
        return span_ == other.span_ && index_ == other.index_;
    }

    // А порядок (<, >) есть только внутри одного массива.
    bool operator<(const MsPtr& other) const {
        CheckSameSpan(other);
        return index_ < other.index_;
    }

    bool operator>(const MsPtr& other) const {
        return other < *this;
    }

    bool operator<=(const MsPtr& other) const {
        return !(other < *this);
    }

    bool operator>=(const MsPtr& other) const {
        return !(*this < other);
    }
    
    size_t GetIndex() const noexcept { return index_; }
    MemorySpan<T>* GetSpan() const noexcept { return span_; }

    // Стоит на позиции "за последним" (разыменовывать нельзя)
    bool IsEnd() const noexcept {
        return span_ != nullptr && index_ == span_->Size();
    }

private:
    // Только MemorySpan::Locate создаёт привязанный MsPtr - после проверки индекса
    friend class MemorySpan<T>;

    MsPtr(MemorySpan<T>* span, size_t index) noexcept : span_(span), index_(index) {}

    void CheckBound() const {
        if (span_ == nullptr) 
            throw std::logic_error("MsPtr isn't bound to any MemorySpan");
    }

    // Можно ли сдвинуться на offset: результат должен быть в [0, Size()].
    // Сравниваем offset с допустимыми границами, а не считаем index_ + offset:
    // при огромном offset сумма переполнила бы ptrdiff_t (это UB).
    void CheckMovable(ptrdiff_t offset) const {
        CheckBound();
        const ptrdiff_t index = static_cast<ptrdiff_t>(index_);
        const ptrdiff_t size = static_cast<ptrdiff_t>(span_->Size());
        if (offset < -index || offset > size - index)
            throw std::out_of_range("MsPtr moved out of MemorySpan bounds");
    }

    // Можно ли прочитать элемент со сдвигом offset: результат должен быть в [0, Size())
    void CheckReadable(ptrdiff_t offset) const {
        CheckBound();
        const ptrdiff_t index = static_cast<ptrdiff_t>(index_);
        const ptrdiff_t size = static_cast<ptrdiff_t>(span_->Size());
        if (offset < -index || offset >= size - index)
            throw std::out_of_range("MsPtr access out of MemorySpan bounds");
    }

    void CheckSameSpan(const MsPtr& other) const {
        if (span_ != other.span_)
            throw std::invalid_argument("MsPtrs belong to different MemorySpans");
    }

    MemorySpan<T>* span_ = nullptr; // к какому массиву привязан (не владеет им)
    size_t index_ = 0;              // номер элемента в нём 
};
