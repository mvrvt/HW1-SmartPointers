#pragma once

#include <cstddef>   // std::size_t 
#include <stdexcept> // исключения
#include <utility>   // std::move(), std::exchange()

#include "UnqPtr.hpp"
#include "ShrdPtr.hpp"
#include "MsPtr.hpp"

// ========== MemorySpan<T> - массив фиксированного размера ==========

template <typename T>
class MemorySpan {
public:
    using ElementType = T; 

    MemorySpan() noexcept = default; 

    explicit MemorySpan(std::size_t size) : data_(MakeUnq<T[]>(size)), size_(size) {}

    MemorySpan(const T* items, std::size_t count) {
        if (items == nullptr && count > 0)
            throw std::invalid_argument("MemorySpan: items is nullptr");
        data_ = MakeUnq<T[]>(count);
        size_ = count;
        for (std::size_t i = 0; i < count; ++i) 
            data_[i] = items[i];
    }

    ~MemorySpan() noexcept = default; 

    // Копирование запрещено: MemorySpan - единственный владелец своих элементов, как и UnqPtr
    MemorySpan(const MemorySpan&) = delete;
    MemorySpan& operator=(const MemorySpan&) = delete; 

    // Move semantics
    // MsPtr, привязанные к источнику, после этого видят пустой массив и при любом обращении бросают out_of_range, а не читают чужую память.
    MemorySpan(MemorySpan&& other) noexcept 
        : data_(std::move(other.data_)), size_(std::exchange(other.size_, 0)) {}

    MemorySpan& operator=(MemorySpan&& other) noexcept {
        data_ = std::move(other.data_);
        size_ = std::exchange(other.size_, 0);
        return *this; 
    }

    UnqPtr<T> Get(std::size_t index) const {
        CheckIndex(index); 
        return MakeUnq<T>(data_[index]); 
    }

    ShrdPtr<T> Copy(std::size_t index) const {
        CheckIndex(index);
        return MakeShrd<T>(data_[index]);
    }

    // index == Size() разрешён: это позиция "за последним" (как End()) 
    MsPtr<T> Locate(std::size_t index) {
        if (index > size_)
            throw std::out_of_range("MemorySpan::Locate: index is out of range");
        return MsPtr<T>(this, index); 
    }

    MsPtr<T> Begin() noexcept { return MsPtr<T>(this, 0); }
    MsPtr<T> End() noexcept { return MsPtr<T>(this, size_); }

    std::size_t Size() const noexcept { 
        return size_;
    }

    bool IsEmpty() const noexcept { return size_ == 0; }

private:
    // MsPtr читает элементы через operator[], границы он проверяет сам.
    friend class MsPtr<T>; 

    T& operator[](std::size_t index) noexcept {
        return data_[index];
    }

    void CheckIndex(std::size_t index) const {
        if (index >= size_)
            throw std::out_of_range("MemorySpan::CheckIndex: Index is out of range");
    }

    UnqPtr<T[]> data_ = nullptr;
    std::size_t size_ = 0;
};
