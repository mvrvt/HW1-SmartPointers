#pragma once

#include <cassert> // assert'ы
#include <cstddef>
#include <utility>
#include <concepts>


// ========= UnqPtr<T> - единоличное владение одиночным объектом. =========
template <typename T>
class UnqPtr {
public:
    using ElementType = T;
    using Pointer = T*;

    UnqPtr() noexcept : ptr_(nullptr) {} // = UnqPtr() noexcept = default
    explicit UnqPtr(T* ptr) noexcept : ptr_(ptr) {}
    
    ~UnqPtr() noexcept {
        delete ptr_;
    }

    // Запрет копирования.
    UnqPtr(const UnqPtr&) = delete; 
    UnqPtr& operator=(const UnqPtr&) = delete;

    // Move semantics
    UnqPtr(UnqPtr&& other) noexcept : ptr_(other.Release()) {}

    UnqPtr& operator=(UnqPtr&& other) noexcept {
        if (this != &other)
            Reset(other.Release());
        return *this;
    }

    // Подтипизация (ковариантность для наследников U -> T).
    // UnqPtr<Derived> -> UnqPtr<Base>, UnqPtr<T> -> UnqPtr<const T>
    template <typename U>
    requires std::convertible_to<U*, T*>
    UnqPtr(UnqPtr<U>&& other) noexcept : ptr_(other.Release()) {}

    template <typename U>
    requires std::convertible_to<U*, T*>
    UnqPtr& operator=(UnqPtr<U>&& other) noexcept {
        Reset(other.Release());
        return *this; 
    }

    UnqPtr& operator=(std::nullptr_t) noexcept {
        Reset();
        return *this;
    }

    T& operator*()  const noexcept { return *ptr_; }
    T* operator->() const noexcept { return ptr_; }
    T* Get()        const noexcept { return ptr_; }
    explicit operator bool() const noexcept { return ptr_ != nullptr; }

    // Модификаторы владения 
    T* Release() noexcept {
        T* temp = ptr_;
        ptr_ = nullptr;
        return temp;
    }

    void Reset(T* new_ptr = nullptr) noexcept {
        if (ptr_ == new_ptr)
            return;
        T* old_ptr = ptr_;
        ptr_ = new_ptr;
        delete old_ptr;
    }

private:
    T* ptr_ = nullptr; 
};

template <typename T>
class UnqPtr<T[]> {
public:
    constexpr UnqPtr() noexcept = default;
    explicit UnqPtr(T* ptr) noexcept : ptr_(ptr) {}

    ~UnqPtr() noexcept {
        delete[] ptr_; 
    }

    UnqPtr(const UnqPtr&) = delete;
    UnqPtr& operator=(const UnqPtr&) = delete;

    // Move semantics
    UnqPtr(UnqPtr&& other) noexcept : ptr_(other.Release()) {}

    UnqPtr& operator=(UnqPtr&& other) noexcept {
        if (this != &other) 
            Reset(other.Release());
        return *this;
    }

    T& operator[](size_t index) const {
        return ptr_[index];
    }

    T* Get() const noexcept { return ptr_; }
    explicit operator bool() const noexcept { return ptr_ != nullptr; }

    T* Release() noexcept {
        T* temp = ptr_;
        ptr_ = nullptr;
        return temp;
    }

    void Reset(T* new_ptr = nullptr) noexcept {
        if (ptr_ == new_ptr) 
            return;
        T* old_ptr = ptr_;
        ptr_ = new_ptr;
        delete[] old_ptr;
    }

private:
    T* ptr_ = nullptr;
};

