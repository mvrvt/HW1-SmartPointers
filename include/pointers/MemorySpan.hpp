#pragma once

#include <cstddef>
#include <stdexcept>
#include <utility>

#include "UnqPtr.hpp"
#include "ShrdPtr.hpp"
#include "MsPtr.hpp"

template <typename T>
class MemorySpan {
public:
    MemorySpan() noexcept = default;

    explicit MemorySpan(size_t size) : size_(size) {
        if (size_ > 0) 
            data_ = new T[size_](); // value-initialization (все ячейки зануляются)
    }

    ~MemorySpan() noexcept {
        delete[] data_;
    }

    // Запрещаем копирование контейнера, чтобы не плодить повисшие MsPtr
    MemorySpan(const MemorySpan&) = delete;
    MemorySpan& operator=(const MemorySpan&) = delete; 

    // Разрешаем перемещение контейнера (Move semantics)
    MemorySpan(MemorySpan&& other) noexcept : data_(other.data_), size_(other.size_) {
        other.data_ = nullptr;
        other.size_ = 0;
    }

    MemorySpan& operator=(MemorySpan&& other) noexcept {
        if (this != &other) {
            delete[] data_;
            data_ = other.data_;
            size_ = other.size_;
            other.data_ = nullptr;
            other.size_ = 0;
        }
        return *this;
    }

    // Методы доступа
    UnqPtr<T> Get(size_t index) const {
        CheckIndex(index);
        return UnqPtr<T>(new T(data_[index]));
    }

    ShrdPtr<T> Copy(size_t index) const {
        CheckIndex(index);
        return ShrdPtr<T>(UnqPtr<T>(new T(data_[index])));
    }

    MsPtr<T> Locate(size_t index) {
        if (index > size_) 
            throw std::out_of_range("Locate index is outside span boudaries");
        return MsPtr<T>(this, index);
    }

    size_t Size() const noexcept {
        return size_;
    }

    // Оператор прямого доступа по индексу для MsPtr
    T& operator[](size_t index) {
        return data_[index];
    }

    const T& operator[](size_t index) const {
        return data_[index];
    }

private:
    void CheckIndex(size_t index) const {
        if (index >= size_) 
            throw std::out_of_range("Index is out of range");
    }

    T* data_     = nullptr;
    size_t size_ = 0;
};

template <typename T>
void MsPtr<T>::CheckBounds(ptrdiff_t target) const {
    if (span_ == nullptr)
        throw std::runtime_error("MsPtr is not bound to any MemorySpan");
    if (target < 0 || static_cast<size_t>(target) > span_->Size()) 
        throw std::out_of_range("MsPtr arithmetic moved index out of bounds");
}
