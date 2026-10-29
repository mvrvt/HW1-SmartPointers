#pragma once

#include <cstddef>   // std::size_t, ptrdiff_t
#include <memory>    // std::addressof
#include <stdexcept> // std::out_of_range, std::invalid_argument, std::logic_error

// Forward declaration
template <typename T>
class MemorySpan;

// ========== MsPtr<T> - безопасный указатель с арифметикой внутри MemorySpan ==========
// Привязан к одному конкретному MemorySpan и хранит номер элемента (индекс).
template <typename T>
class MsPtr {
public:
    using ElementType = T;
    using Pointer = T*;

    // Пустой указатель, ни к чему не привязан. Создать привязанный можно только через MemorySpan::Locate
    MsPtr() noexcept : span_(nullptr), index_(0) {}

    T& operator*() const {
        CheckReadable(0);
        return (*span_)[index_];
    }

    T* operator->() const { 
        CheckReadable(0);
        return std::addressof((*span_)[index_]); 
    }

    T& operator[](ptrdiff_t offset) const { 
        CheckReadable(offset);
        return (*span_)[static_cast<std::size_t>(static_cast<ptrdiff_t>(index_) + offset)];
    }

    // (Арифметика: результат в [0, Size()], включая позицию «за последним»)
    MsPtr& operator+=(ptrdiff_t offset) {
        CheckMovableForward(offset);
        index_ = static_cast<std::size_t>(static_cast<ptrdiff_t>(index_) + offset);
        return *this; 
    }

    MsPtr& operator-=(ptrdiff_t offset) {
        CheckMovableBack(offset);
        index_ = static_cast<std::size_t>(static_cast<ptrdiff_t>(index_) - offset);
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

    bool operator==(const MsPtr& other) const noexcept { 
        return span_ == other.span_ && index_ == other.index_;
    }

    bool operator!=(const MsPtr& other) const noexcept {
        return !(*this == other);
    }

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
    
    std::size_t GetIndex() const noexcept { return index_; }
    MemorySpan<T>* GetSpan() const noexcept { return span_; }

    // Стоит на позиции "за последним" (разыменовывать нельзя)
    bool IsEnd() const noexcept {
        return span_ != nullptr && index_ == span_->Size();
    }

private:
    // Только MemorySpan::Locate создаёт привязанный MsPtr - после проверки индекса
    friend class MemorySpan<T>;

    MsPtr(MemorySpan<T>* span, std::size_t index) noexcept : span_(span), index_(index) {}

    void CheckBound() const {
        if (span_ == nullptr) 
            throw std::logic_error("MsPtr isn't bound to any MemorySpan");
    }

    void CheckMovableForward(ptrdiff_t offset) const {
        CheckBound();
        const ptrdiff_t index = static_cast<ptrdiff_t>(index_);
        const ptrdiff_t size = static_cast<ptrdiff_t>(span_->Size());
        if (offset < -index || offset > size - index) 
            throw std::out_of_range("MsPtr moved out of MemorySpan bounds");
    }

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

    MemorySpan<T>* span_ = nullptr; 
    std::size_t index_ = 0;
};


// Оператор чтобы можно было писать MsPtr<int> a = 2 + b;
template <typename T>
MsPtr<T> operator+(ptrdiff_t offset, const MsPtr<T>& ptr) {
    return ptr + offset;
}
