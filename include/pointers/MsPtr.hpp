#pragma once

#include <cstddef>   // size_t, ptrdiff_t
#include <memory>    // std::addressof
#include <stdexcept> // std::out_of_range, std::invalid_argument, std::logic_error

// Forward declaration
template <typename T>
class MemorySpan;

// ========== MsPtr<T> - безопасный указатель с арифметикой внутри MemorySpan ==========
// Привязан к одному конкретному MemorySpan и хранит номер элемента (индекс).
// Может стоять на любом элементе [0, Size()) или на позиции «за последним» (Size()),
// но никогда - за границами массива: любой выход за них бросает исключение.
// MsPtr НЕ владеет элементами: их хранит и удаляет MemorySpan.
template <typename T>
class MsPtr {
public:
    using ElementType = T;
    using Pointer = T*;

    // Пустой указатель, ни к чему не привязан. Создать привязанный можно только через MemorySpan::Locate
    MsPtr() noexcept : span_(nullptr), index_(0) {}

    // Копирование, присваивание и деструктор - те, что генерирует компилятор:
    // MsPtr ничем не владеет, копия - просто ещё один указатель в тот же массив.

    // ---------- Доступ к элементу: только к существующему, [0, Size()) ----------
    T& operator*() const {
        CheckReadable(0);
        return (*span_)[index_];
    }

    T* operator->() const { 
        CheckReadable(0);
        return std::addressof((*span_)[index_]); 
    }

    // it[offset] - элемент, отстоящий от текущего на offset (как *(it + offset))
    T& operator[](ptrdiff_t offset) const { 
        CheckReadable(offset);
        return (*span_)[static_cast<size_t>(static_cast<ptrdiff_t>(index_) + offset)];
    }

    // ---------- Арифметика: результат в [0, Size()], включая позицию «за последним» ----------
    MsPtr& operator+=(ptrdiff_t offset) {
        CheckMovableForward(offset);
        index_ = static_cast<size_t>(static_cast<ptrdiff_t>(index_) + offset);
        return *this; 
    }

    MsPtr& operator-=(ptrdiff_t offset) {
        CheckMovableBack(offset);
        index_ = static_cast<size_t>(static_cast<ptrdiff_t>(index_) - offset);
        return *this; 
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

    // operator+ для ситуаций (2 + ptr) прописан в низу файла

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
    bool operator==(const MsPtr& other) const noexcept { 
        return span_ == other.span_ && index_ == other.index_;
    }

    bool operator!=(const MsPtr& other) const noexcept {
        return !(*this == other);
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
    // void CheckMovable(ptrdiff_t offset) const {
    //     CheckBound();
    //     const ptrdiff_t index = static_cast<ptrdiff_t>(index_);
    //     const ptrdiff_t size = static_cast<ptrdiff_t>(span_->Size());
    //     if (offset < -index || offset > size - index)
    //         throw std::out_of_range("MsPtr moved out of MemorySpan bounds");
    // }

    // Можно ли сдвинуться вперёд на offset: index + offset должен быть в [0, Size()]
    void CheckMovableForward(ptrdiff_t offset) const {
        CheckBound();
        const ptrdiff_t index = static_cast<ptrdiff_t>(index_);
        const ptrdiff_t size = static_cast<ptrdiff_t>(span_->Size());
        if (offset < -index || offset > size - index) 
            throw std::out_of_range("MsPtr moved out of MemorySpan bounds");
    }

    // Можно ли сдвинуться назад на offset: index - offset должен быть в [0, Size()]
    //   index - offset >= 0     <=>  offset <= index
    //   index - offset <= size  <=>  offset >= index - size
    void CheckMovableBack(ptrdiff_t offset) const {
        CheckBound();
        const ptrdiff_t index = static_cast<ptrdiff_t>(index_);
        const ptrdiff_t size = static_cast<ptrdiff_t>(span_->Size());
        if (offset > index || offset < index - size)
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


// Оператор чтобы можно было писать MsPtr<int> a = 2 + b;
template <typename T>
MsPtr<T> operator+(ptrdiff_t offset, const MsPtr<T>& ptr) {
    return ptr + offset;
}
