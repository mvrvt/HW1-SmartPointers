#pragma once

#include <cstddef>
#include <stdexcept>
#include <utility>

#include "UnqPtr.hpp"
#include "ShrdPtr.hpp"
#include "MsPtr.hpp"

// ========== MemorySpan<T> — массив фиксированного размера ==========
// Владеет своими элементами: хранит их в UnqPtr<T[]>, и тот удаляет их через delete[].
// Прямого доступа к элементам (span[i]) снаружи нет — только через умные указатели:
//   Get(i)    -> UnqPtr<T>  независимая копия элемента (единоличное владение)
//   Copy(i)   -> ShrdPtr<T> копия элемента с разделяемым владением
//   Locate(i) -> MsPtr<T>   указатель на сам элемент внутри массива, с проверкой границ

template <typename T>
class MemorySpan {
public:
    using ElementType = T; //TODO: А почему у нас нет "using Pointer = T*", но есть ElementType?

    MemorySpan() noexcept = default; 

    explicit MemorySpan(size_t size) : data_(MakeUnq<T[]>(size)), size_(size) {}

    // Копия count элементов из обычного массива items
    MemorySpan(const T* items, size_t count) {
        if (items == nullptr && count > 0)
            throw std::invalid_argument("MemorySpan: items is nullptr");
        data_ = MakeUnq<T[]>(count);
        size_ = count;
        for (size_t i = 0; i < count; ++i) 
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

    // Методы доступа через умные указатели
    UnqPtr<T> Get(size_t index) const {
        CheckIndex(index); 
        return MakeUnq<T>(data_[index]); //TODO: Правильно ли я понимаю, что мы получаем новый UnqPtr (не массив), ptr_ которого будет равен data_[index]?
    }

    ShrdPtr<T> Copy(size_t index) const {
        CheckIndex(index);
        return MakeShrd<T>(data_[index]);
    }

    // index == Size() разрешён: это позиция "за последним" (как End())
    MsPtr<T> Locate(size_t index) {
        if (index > size_)
            throw std::out_of_range("MemorySpan::Locate: index is out of range");
        return MsPtr<T>(this, index); //TODO: Вообще не понял что делает эта функция
    }

    MsPtr<T> Begin() noexcept { return MsPtr<T>(this, 0); }
    MsPtr<T> End() noexcept { return MsPtr<T>(this, size_); }

    size_t Size() const noexcept {
        return size_;
    }

    bool IsEmpty() const noexcept { return size_ == 0; }

private:
    // MsPtr читает элементы через operator[], границы он проверяет сам.
    friend class MsPtr<T>;

    T& operator[](size_t index) noexcept {
        return data_[index];
    }

    void CheckIndex(size_t index) const {
        if (index >= size_)
            throw std::out_of_range("MemorySpan::CheckIndex: Index is out of range");
    }

    UnqPtr<T[]> data_ = nullptr;
    size_t size_ = 0;
};
