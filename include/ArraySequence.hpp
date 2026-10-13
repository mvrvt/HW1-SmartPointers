#pragma once

#include <cstddef>    // size_t
#include <stdexcept>  // std::out_of_range, std::invalid_argument
#include <utility>    // std::move, std::exchange, std::swap

#include "pointers/UnqPtr.hpp"
#include "Sequence.hpp"

// ========== ArraySequence<T> — динамический массив на UnqPtr<T[]> ==========
// Контейнер для проверки умных указателей: буфером владеет UnqPtr<T[]>,
// поэтому своего delete[] здесь нет. Ёмкость растёт вдвое при нехватке места.
template <typename T>
class ArraySequence : public Sequence<T> {
public:

    ArraySequence() noexcept = default; 

    explicit ArraySequence(size_t count) : size_(count), capacity_(count) {
        if (capacity_ > 0)
            buffer_ = MakeUnq<T[]>(capacity_);
    }

    // Копия n элементов из обычного массива items. 
    ArraySequence(const T* items, size_t count) {
        if (items == nullptr && count > 0) 
            throw std::invalid_argument("ArraySequence: items is nullptr");
        if (count == 0) 
            return;
        buffer_ = MakeUnq<T[]>(count);
        size_ = count; 
        capacity_ = count;
        for (size_t i = 0; i < count; ++i) 
            buffer_[i] = items[i];
    }

    // Deep copying (у копии свой буфер)
    ArraySequence(const ArraySequence& other) : size_(other.size_), capacity_(other.capacity_) {
        if (capacity_ > 0) 
            buffer_ = MakeUnq<T[]>(capacity_);
        for (size_t i = 0; i < size_; ++i) 
            buffer_[i] = other.buffer_[i];
    }

    // Copy-and-swap: если копирование бросит исключение, *this не изменится
    ArraySequence& operator=(const ArraySequence& other) {
        if (this != &other) {
            ArraySequence copy(other);
            Swap(copy);
        }
        return *this; 
    }

    ArraySequence(ArraySequence&& other) noexcept 
        : buffer_(std::move(other.buffer_)), 
          size_(std::exchange(other.size_, 0)),
          capacity_(std::exchange(other.capacity_, 0)) {}

    ArraySequence& operator=(ArraySequence&& other) noexcept {
        if (this != &other) {
            buffer_ = std::move(other.buffer_);
            size_ = std::exchange(other.size_, 0);
            capacity_ = std::exchange(other.capacity_, 0);
        }
        return *this;
    }

    // Буфер освобождает деструктор поля UnqPtr<T[]>
    ~ArraySequence() override = default;

    // Информация
    size_t GetLength() const noexcept override { return size_; }
    size_t GetCapacity() const noexcept  { return capacity_; }
    bool IsEmpty() const noexcept override { return size_ == 0; }

    // Доступ с проверкой (бросает out_of_range)
    const T& Get(size_t index) const override {
        CheckIndex(index);
        return buffer_[index];
    }

    T& Get(size_t index) override {
        CheckIndex(index);
        return buffer_[index];
    }

    const T& GetFirst() const override {
        CheckEmpty();
        return buffer_[0];
    }

    T& GetFirst()  override {
        CheckEmpty();
        return buffer_[0];
    }

    const T& GetLast() const override {
        CheckEmpty();
        return buffer_[size_ - 1];
    }

    T& GetLast() override {
        CheckEmpty();
        return buffer_[size_ - 1];
    }

    // Доступ без проверки (как у std::vector::operator[])
    const T& operator[](size_t index) const override {
        return buffer_[index];
    }

    T& operator[](size_t index) override {
        return buffer_[index];
    }

    void Append(const T& item) override { //TODO: объясни почему мы изменили Append()? Что в нём нас не устраивало и что в нём было неверно?
        T value = item;
        EnsureCapacity();
        buffer_[size_] = std::move(value);
        ++size_;
    }

    void InsertAt(const T& item, size_t index) override {
        if (index > size_) 
            throw std::out_of_range("ArraySequence::InsertAt: insert index is out of bounds");
        T value = item;
        EnsureCapacity();
        for (size_t i = size_; i > index; --i) 
            buffer_[i] = std::move(buffer_[i - 1]);
        buffer_[index] = std::move(value);
        ++size_;
    }

    void Prepend(const T& item) override {
        InsertAt(item, 0);
    }

    void RemoveAt(size_t index) override {
        CheckIndex(index);
        for (size_t i = index; i + 1  < size_; ++i) 
            buffer_[i] = std::move(buffer_[i + 1]);
        buffer_[size_ - 1] = T();
        --size_;
    }

    void Reserve(size_t new_capacity) {
        if (new_capacity <= capacity_)
            return;
        UnqPtr<T[]> new_buffer = MakeUnq<T[]>(new_capacity);
        for (size_t i = 0; i < size_; ++i) 
            new_buffer[i] = std::move(buffer_[i]);
        buffer_ = std::move(new_buffer); // старый буфер удалит UnqPtr
        capacity_ = new_capacity;
    }

    void Swap(ArraySequence& other) noexcept {
        buffer_.Swap(other.buffer_);
        std::swap(size_, other.size_);
        std::swap(capacity_, other.capacity_);
    }

private:
    void CheckIndex(size_t index) const {
        if (index >= size_)
            throw std::out_of_range("ArraySequence::CheckIndex: index is out of range");
    }

    void CheckEmpty() const {
        if (size_ == 0)
            throw std::out_of_range("ArraySequence::CheckEmpty: Sequence is empty");
    }

    void EnsureCapacity() {
        if (size_ == capacity_) 
            Reserve(capacity_ == 0 ? 4 : capacity_ * 2);
    }

    UnqPtr<T[]> buffer_;
    size_t size_ = 0;
    size_t capacity_ = 0;
};
