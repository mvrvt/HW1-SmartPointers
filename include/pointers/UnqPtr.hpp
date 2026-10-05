#pragma once

#include <cassert> // assert'ы
#include <concepts>
#include <cstddef>
#include <type_traits>
#include <utility>

// ========== UnqPtr<T> - единоличное владение одиночным объектом. ==========
template <typename T>
class UnqPtr {
public:
    using ElementType = T;
    using Pointer = T*;

    UnqPtr() noexcept : ptr_(nullptr) {} // = UnqPtr() noexcept = default
    UnqPtr(std::nullptr_t) noexcept : ptr_(nullptr) {}
    explicit UnqPtr(T* ptr) noexcept : ptr_(ptr) {}
    
    ~UnqPtr() noexcept {
        Destroy(ptr_);
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

    T& operator*() const noexcept { 
        assert(ptr_ != nullptr && "Dereferencing empty UnqPtr");
        return *ptr_; 
    }

    T* operator->() const noexcept { 
        assert(ptr_ != nullptr && "Member access through empty UnqPtr");
        return ptr_; 
    }

    T* Get() const noexcept { return ptr_; }

    explicit operator bool() const noexcept { return ptr_ != nullptr; }

    [[nodiscard]] T* Release() noexcept {
        T* temp = ptr_;
        ptr_ = nullptr;
        return temp;
    }

    void Reset(T* new_ptr = nullptr) noexcept {
        if (ptr_ == new_ptr) // p.Reset(p.Get()) не должен удалить живой объект
            return;
        Destroy(std::exchange(ptr_, new_ptr));
    }

    void Swap(UnqPtr& other) noexcept {
        std::swap(ptr_, other.ptr_);
    }

    friend void swap(UnqPtr& lhs, UnqPtr& rhs) noexcept {
        lhs.Swap(rhs);
    }

    bool operator==(const UnqPtr& other) const noexcept { return ptr_ == other.ptr_; }
    bool operator==(std::nullptr_t) const noexcept { return ptr_ == nullptr; }

private:
    static void Destroy(T* ptr) noexcept {
        static_assert(sizeof(T) > 0, "UnqPtr can't delete an incomplete type");
        delete ptr;
    }

    T* ptr_ = nullptr; 
};

// ========== UnqPtr<T[]> — единоличное владение динамическим массивом ==========
template <typename T>
class UnqPtr<T[]> {
public:
    using ElementType = T;
    using Pointer = T*;

    UnqPtr() noexcept = default;
    UnqPtr(std::nullptr_t) noexcept {}

    // Только T* (или T* -> const T*), но НЕ Derived*:
    // delete[] и индексация через Base* для массива Derived — UB
    template <typename U>
    requires std::convertible_to<U(*)[], T(*)[]>
    explicit UnqPtr(U* ptr) noexcept : ptr_(ptr) {}

    UnqPtr(const UnqPtr&) = delete;
    UnqPtr& operator=(const UnqPtr&) = delete;

    UnqPtr(UnqPtr&& other) noexcept : ptr_(other.Release()) {}
    
    UnqPtr& operator=(UnqPtr&& other) noexcept {
        if (this != &other) 
            Reset(other.Release());
        return *this;
    }

    // Разрешено только добавление const: UnqPtr<int[]> -> UnqPtr<const int[]>
    template <typename U>
    requires std::convertible_to<U(*)[], T(*)[]>
    UnqPtr(UnqPtr<U[]>&& other) noexcept : ptr_(other.Release()) {}

    template <typename U>
    requires std::convertible_to<U(*)[], T(*)[]>
    UnqPtr& operator=(UnqPtr<U[]>&& other) noexcept {
        Reset(other.Release());
        return *this;
    }

    UnqPtr& operator=(std::nullptr_t) noexcept {
        Reset();
        return *this;
    }

    ~UnqPtr() noexcept {
        Destroy(ptr_);
    }

    // Границы не проверяются: размер массива UnqPtr неизвестен
    T& operator[](size_t index) const noexcept {
        assert(ptr_ != nullptr && "Indexing empty UnqPtr");
        return ptr_[index];
    }

    T* Get() const noexcept { return ptr_; }
    explicit operator bool() const noexcept { return ptr_ != nullptr; }

    [[nodiscard]] T* Release() noexcept {
        return std::exchange(ptr_, nullptr);
    }

    template <typename U>
    requires std::convertible_to<U(*)[], T(*)[]>
    void Reset(U* new_ptr) noexcept {
        if (ptr_ == new_ptr)
            return;
        Destroy(std::exchange(ptr_, new_ptr));
    }

    void Reset(std::nullptr_t = nullptr) noexcept {
        Destroy(std::exchange(ptr_, nullptr));
    }

    void Swap(UnqPtr& other) noexcept {
        std::swap(ptr_, other.ptr_);
    }
    
    friend void swap(UnqPtr& lhs, UnqPtr& rhs) noexcept {
        lhs.Swap(rhs);
    }

    bool operator==(const UnqPtr& other) const noexcept { return ptr_ == other.ptr_; }
    bool operator==(std::nullptr_t) const noexcept { return ptr_ == nullptr; }

private:
    static void Destroy(T* ptr) noexcept {
        static_assert(sizeof(T) > 0, "UnqPtr can't delete an incomplete type");
        delete[] ptr;
    }

    T* ptr_ = nullptr;
};

// Фабричные методы (аналог std::make_unique())
template <typename T, typename... Args>
requires (!std::is_array_v<T>)
[[nodiscard]] UnqPtr<T> MakeUnq(Args&&... args) {
    return UnqPtr<T>(new T(std::forward<Args>(args)...));
}

// MakeUnq<int[]>(n): массив из n элементов, заполненных нулями/конструкторами по умолчанию
template <typename T>
requires std::is_unbounded_array_v<T>
[[nodiscard]] UnqPtr<T> MakeUnq(size_t size) {
    return UnqPtr<T>(new std::remove_extent_t<T>[size]());
}
