#pragma once

#include <cstddef>
#include <stdexcept>

// Forward declaration
template <typename T>
class MemorySpan;

template <typename T>
class MsPtr {
public:
    constexpr MsPtr() noexcept = default; 

    MsPtr(MemorySpan<T>* span, size_t index) noexcept : span_(span), index_(index) {}

    // Разыменовывание (с проверкой границ)
    T& operator*() const {
        CheckValidIndex();
        return (*span_)[index_];
    }

    T* operator->() const {
        CheckValidIndex();
        return &((*span_)[index_]);
    }

    T& operator[](ptrdiff_t offset) const {
        ptrdiff_t target = static_cast<ptrdiff_t>(index_) + offset; 
        CheckBounds(target);
        return (*span_)[static_cast<size_t>(target)];
    }

    // Арифметика: операторы += и -=
    MsPtr& operator+=(ptrdiff_t offset) {
        ptrdiff_t target = static_cast<ptrdiff_t>(index_) + offset;
        CheckBounds(target);
        index_ = static_cast<size_t>(target);
        return *this; 
    }

    MsPtr& operator-=(ptrdiff_t offset) {
        return *this += (-offset);
    }

    // Операторы + и - с числами 
    MsPtr operator+(ptrdiff_t offset) const {
        MsPtr temp = *this; 
        temp += offset; 
        return temp;
    }

    MsPtr operator-(ptrdiff_t offset) const {
        MsPtr temp = *this;
        temp -= offset; 
        return temp;
    }

    // Разность двух указателей (расстояние между ними)
    ptrdiff_t operator-(const MsPtr& other) const {
        if (span_ != other.span_) 
            throw std::invalid_argument("Poiners belong to different MemorySpans");
        return static_cast<ptrdiff_t>(index_) - static_cast<ptrdiff_t>(other.index_);
    }

    bool operator==(const MsPtr& other) const {
        return span_ == other.span_ && index_ == other.index_;
    }

    bool operator!=(const MsPtr& other) const {
        return !(*this == other);
    }

    bool operator<(const MsPtr& other) const {
        CheckSameSpan(other);
        return index_ < other.index_;
    }

    bool operator>(const MsPtr& other) const {
        CheckSameSpan(other);
        return other < *this; 
    }

    bool operator<=(const MsPtr& other) const {
        CheckSameSpan(other);
        return !(other < *this);
    }

    bool operator>=(const MsPtr& other) const {
        CheckSameSpan(other);
        return !(*this < other);
    }

    // Префиксный
    MsPtr& operator++() {
        *this += 1;
        return *this;
    }

    // Постфиксный
    MsPtr operator++(int) {
        MsPtr temp = *this;
        *this += 1;
        return temp;
    }

    MsPtr& operator--() {
        *this -= 1;
        return *this; 
    }

    MsPtr operator--(int) {
        MsPtr temp = *this;
        *this -= 1;
        return temp;
    }

    // Getters 
    size_t GetIndex() const noexcept { return index_; }
    MemorySpan<T>* GetSpan() const noexcept { return span_; }


private:
    void CheckValidIndex() const {
        if (span_ == nullptr) {
            throw std::runtime_error("Dereferencing null MsPtr");
        }
        CheckBounds(static_cast<ptrdiff_t>(index_));
    }

    void CheckBounds(ptrdiff_t target) const; // Реализуется после MemorySpan

    void CheckSameSpan(const MsPtr& other) const {
        if (span_ != other.span_) {
            throw std::invalid_argument("Comparing pointers from different MemorySpans");
        }
    }

    MemorySpan<T>* span_ = nullptr;
    size_t         index_ = 0;
};
