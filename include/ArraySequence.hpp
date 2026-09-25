#pragma once

#include <cstddef>
#include <stdexcept>
#include <utility>

#include "pointers/UnqPtr.hpp"
#include "Sequence.hpp"

template <typename T>
class ArraySequence : public Sequence<T> {
public:

    ArraySequence() noexcept = default; 

    explicit ArraySequence(size_t count) : size_(count), capacity_(count) {
        if (capacity_ > 0)
            buffer_.Reset(new T[capacity_]()); // value-initialization 
    }

    ArraySequence(const T* items, size_t count) {
        if (items == nullptr && count > 0) 
            throw std::invalid_argument("Cannot initialize from nullptr with non-zero count");
        if (count > 0) {
            size_ = count;
            capacity_ = count; 
            buffer_.Reset(new T[capacity_]());
            for (size_t i = 0; i < size_; ++i) {
                buffer_[i] = items[i];
            }
        }
    }

    // Deep copying 
    ArraySequence(const ArraySequence& other) : size_(other.size_), capacity_(other.capacity_) {
        if (capacity_ > 0) {
            buffer_.Reset(new T[capacity_]());
            for (size_t i = 0; i < size_; ++i) {
                buffer_[i] = other.buffer_[i];
            }
        }
    }

    ArraySequence& operator=(const ArraySequence& other) {
        if (this != &other){
            ArraySequence temp(other);
            Swap(temp);
        }
        return *this; 
    }

    // Move semantics
    ArraySequence(ArraySequence&& other) noexcept 
        : buffer_(std::move(other.buffer_)),
          size_(other.size_),
          capacity_(other.capacity_) {
        other.size_ = 0;
        other.capacity_ = 0;
    }

    ArraySequence& operator=(ArraySequence&& other) noexcept {
        if (this != &other) {
            buffer_ = std::move(other.buffer_);
            size_ = other.size_;
            capacity_ = other.capacity_;
            other.size_ = 0;
            other.capacity_ = 0;
        }
        return *this;
    }

    // Деструктор: buffer_ автоматически вызовет delete[] ptr_ 
    ~ArraySequence() override = default;
    
    size_t GetLength() const noexcept override {
        return size_;
    }

    size_t GetCapacity() const noexcept  {
        return capacity_;
    }

    bool IsEmpty() const noexcept override {
        return size_ == 0;
    }

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

    const T& operator[](size_t index) const override {
        return buffer_[index];
    }

    T& operator[](size_t index) override {
        return buffer_[index];
    }

    void Append(const T& item) override {
        EnsureCapacity();
        buffer_[size_] = item;
        ++size_;
    }

    void InsertAt(const T& item, size_t index) override {
        if (index > size_) 
            throw std::out_of_range("Insert index is out of bounds");
        EnsureCapacity();
        for (size_t i = size_; i > index; --i) {
            buffer_[i] = std::move(buffer_[i - 1]);
        }
        buffer_[index] = item;
        ++size_;
    }

    void Prepend(const T& item) override {
        InsertAt(item, 0);
    }

    void RemoveAt(size_t index) override {
        CheckIndex(index);
        for (size_t i = index; i < size_ - 1; ++i) {
            buffer_[i] = std::move(buffer_[i + 1]);
        }
        buffer_[size_ - 1] = T();
        --size_;
    }

    void Reserve(size_t new_capacity) {
        if (new_capacity <= capacity_)
            return;
        UnqPtr<T[]> new_buffer(new T[new_capacity]());
        for (size_t i = 0; i < size_; ++i) 
            new_buffer[i] = std::move(buffer_[i]);
        buffer_ = std::move(new_buffer);
        capacity_ = new_capacity;
    }

    void Swap(ArraySequence& other) noexcept {
        UnqPtr<T[]> temp_buffer = std::move(buffer_);
        buffer_ = std::move(other.buffer_);
        other.buffer_ = std::move(temp_buffer);

        std::swap(size_, other.size_);
        std::swap(capacity_, other.capacity_);
    }



private:
    void CheckIndex(size_t index) const {
        if (index >= size_)
            throw std::out_of_range("Index is out of range");
    }

    void CheckEmpty() const {
        if (size_ == 0)
            throw std::out_of_range("Sequence is empty");
    }

    void EnsureCapacity() {
        if (size_ == capacity_) {
            Reserve(capacity_ == 0 ? 4 : capacity_ * 2);
        }
    }

    UnqPtr<T[]> buffer_;
    size_t size_ = 0;
    size_t capacity_ = 0;
};
