#pragma once

#include <cassert>     // assert
#include <concepts>    // std::convertible_to
#include <cstddef>     // std::size_t, std::nullptr_t
#include <type_traits> // std::is_array_v, std::is_unbounded_array_v
#include <utility>     // std::exchange, std::swap, std::forward

#include "UnqPtr.hpp"

// ========== ShrdPtr<T> - разделяемое владение одиночным объектом ==========
// Все копии, указывающие на один объект, делят один счётчик ссылок (std::size_t в куче).
template <typename T>
class ShrdPtr {
public:
    using ElementType = T;
    using Pointer = T*;

    ShrdPtr() noexcept = default; 
    ShrdPtr(std::nullptr_t) noexcept : ptr_(nullptr), ref_count_(nullptr) {}

    // Забираем объект у UnqPtr (подходит и UnqPtr<Derived> для ShrdPtr<Base>).
    // Сначала выделяем счётчик и только потом забираем объект: если new бросит
    // std::bad_alloc, объект останется в owner и не потеряется.
    template <typename U>
    requires std::convertible_to<U*, T*> && SafeUpcast<U, T>
    ShrdPtr(UnqPtr<U>&& owner) : ptr_(nullptr), ref_count_(nullptr) {
        if (!owner) 
            return; 
        ref_count_ = new std::size_t(1);
        ptr_ = owner.Release();
    }

    template <typename U>
    requires std::convertible_to<U*, T*> && SafeUpcast<U, T>
    explicit ShrdPtr(U* ptr) : ShrdPtr(UnqPtr<U>(ptr)) {}

    ShrdPtr(const ShrdPtr& other) noexcept : ptr_(other.ptr_), ref_count_(other.ref_count_) {
        AddRef();
    }

    ShrdPtr(ShrdPtr&& other) noexcept
        : ptr_(std::exchange(other.ptr_, nullptr)),
          ref_count_(std::exchange(other.ref_count_, nullptr)) {}

    // Подтипизация: ShrdPtr<Derived> -> ShrdPtr<Base>, ShrdPtr<T> -> ShrdPtr<const T>
    template <typename U>
    requires std::convertible_to<U*, T*> && SafeUpcast<U, T>
    ShrdPtr(const ShrdPtr<U>& other) noexcept : ptr_(other.ptr_), ref_count_(other.ref_count_) {
        AddRef();
    }

    template <typename U>
    requires std::convertible_to<U*, T*> && SafeUpcast<U, T>
    ShrdPtr(ShrdPtr<U>&& other) noexcept 
        : ptr_(std::exchange(other.ptr_, nullptr)),
          ref_count_(std::exchange(other.ref_count_, nullptr)) {}
    
    ShrdPtr& operator=(ShrdPtr other) noexcept {
        Swap(other);
        return *this; 
    } 

    ~ShrdPtr() noexcept {
        RemoveRef();
    }

    T& operator*() const noexcept {
        assert(ptr_ != nullptr && "Dereferencing empty ShrdPtr"); 
        return *ptr_;
    }

    T* operator->() const noexcept {
        assert(ptr_ != nullptr && "Member access through empty ShrdPtr");
        return ptr_;
    }

    T* Get() const noexcept { return ptr_; }
    explicit operator bool() const noexcept { return ptr_ != nullptr; }

    std::size_t UseCount() const noexcept {
        return ref_count_ != nullptr ? *ref_count_ : 0;
    }

    // Перестать владеть объектом (он удалится, если мы были последними)
    void Reset(std::nullptr_t = nullptr) noexcept {
        ShrdPtr empty;
        Swap(empty);
    } 

    template <typename U>
    requires std::convertible_to<U*, T*> && SafeUpcast<U, T>
    void Reset(U* ptr) {    
        if (ptr == ptr_) // p.Reset(p.Get()) не должен создать второй счётчик
            return;
        ShrdPtr new_owner(ptr);
        Swap(new_owner);
    } 

    void Swap(ShrdPtr& other) noexcept {
        std::swap(ptr_, other.ptr_);
        std::swap(ref_count_, other.ref_count_);
    }

    friend void swap(ShrdPtr& l, ShrdPtr& r) noexcept { 
        l.Swap(r);
    }

    bool operator==(const ShrdPtr& other) const noexcept { return ptr_ == other.ptr_; }
    bool operator==(std::nullptr_t) const noexcept { return ptr_ == nullptr; }

private:
    // ShrdPtr<Base> читает private-поля ShrdPtr<Derived> в конструкторах подтипизации
    template <typename U>
    friend class ShrdPtr; 

    void AddRef() noexcept {
        if (ref_count_ != nullptr) 
            ++(*ref_count_);
    }

    void RemoveRef() noexcept {
        if (ref_count_ == nullptr)
            return;
        --(*ref_count_);
        if (*ref_count_ == 0) {
            delete ref_count_;
            UnqPtr<T> owner(ptr_); 
        }
    }

    T* ptr_ = nullptr;
    std::size_t* ref_count_ = nullptr;
};

// ========== ShrdPtr<T[]> - разделяемое владение динамическим массивом ==========
// Подтипизации нет (как и у UnqPtr<T[]>): ни Derived -> Base, ни T -> const T.
template <typename T>
class ShrdPtr<T[]> {
public:
    using ElementType = T;  // для ShrdPtr<int[]> это int, а не int[]
    using Pointer = T*;
 
    ShrdPtr() noexcept : ptr_(nullptr), ref_count_(nullptr) {}
    ShrdPtr(std::nullptr_t) noexcept : ptr_(nullptr), ref_count_(nullptr) {}
 
    ShrdPtr(UnqPtr<T[]>&& owner) : ptr_(nullptr), ref_count_(nullptr) {
        if (!owner)
            return;
        ref_count_ = new std::size_t(1);
        ptr_ = owner.Release();
    }
 
    // У массивов подтипизации нет: принимаем только T* (как у UnqPtr<T[]>)
    explicit ShrdPtr(T* ptr) : ShrdPtr(UnqPtr<T[]>(ptr)) {}

    template <typename U>
    ShrdPtr(U* ptr) = delete;  // Derived* и любой другой указатель - ошибка компиляции
 
    ShrdPtr(const ShrdPtr& other) noexcept : ptr_(other.ptr_), ref_count_(other.ref_count_) {
        AddRef();
    }
 
    ShrdPtr(ShrdPtr&& other) noexcept
        : ptr_(std::exchange(other.ptr_, nullptr)),
          ref_count_(std::exchange(other.ref_count_, nullptr)) {}
 
    ShrdPtr& operator=(ShrdPtr other) noexcept {
        Swap(other);
        return *this;
    }
 
    ~ShrdPtr() noexcept {
        RemoveRef();
    }

    T& operator[](std::size_t index) const noexcept {
        assert(ptr_ != nullptr && "Indexing empty ShrdPtr");
        return ptr_[index];
    }
 
    T* Get() const noexcept { return ptr_; }
    explicit operator bool() const noexcept { return ptr_ != nullptr; }
 
    std::size_t UseCount() const noexcept {
        return ref_count_ != nullptr ? *ref_count_ : 0;
    }

    // Reset() и Reset(nullptr) - перестать владеть массивом
    void Reset(std::nullptr_t = nullptr) noexcept {
        ShrdPtr empty;
        Swap(empty);
    }
 
    // Стать единственным владельцем нового массива ptr
    void Reset(T* ptr) {
        if (ptr == ptr_) // p.Reset(p.Get()) не должен создать второй счётчик
            return;
        ShrdPtr new_owner(ptr);
        Swap(new_owner);
    }

    template <typename U>
    void Reset(U* ptr) = delete;  // только T*
 
    void Swap(ShrdPtr& other) noexcept {
        std::swap(ptr_, other.ptr_);
        std::swap(ref_count_, other.ref_count_);
    }
 
    friend void swap(ShrdPtr& l, ShrdPtr& r) noexcept {
        l.Swap(r);
    }
 
    bool operator==(const ShrdPtr& other) const noexcept { return ptr_ == other.ptr_; }
    bool operator==(std::nullptr_t) const noexcept { return ptr_ == nullptr; }
 
private:
    void AddRef() noexcept {
        if (ref_count_ != nullptr)
            ++(*ref_count_);
    }
 
    void RemoveRef() noexcept {
        if (ref_count_ == nullptr)
            return;
        --(*ref_count_);
        if (*ref_count_ == 0) {
            delete ref_count_;
            UnqPtr<T[]> owner(ptr_);  
        }
    }
 
    T* ptr_ = nullptr;
    std::size_t* ref_count_ = nullptr;
};

// Фабричные функции (аналог std::make_shared)
template <typename T, typename ... Args>
requires (!std::is_array_v<T>)
[[nodiscard]] ShrdPtr<T> MakeShrd(Args&&... args) {
    return ShrdPtr<T>(MakeUnq<T>(std::forward<Args>(args)...)); 
}

// MakeShrd<int[]>(n): массив из n элементов, заполненных нулями / конструктором по умолчанию
template <typename T>
requires std::is_unbounded_array_v<T>
[[nodiscard]] ShrdPtr<T> MakeShrd(std::size_t size) {
    return ShrdPtr<T>(MakeUnq<T>(size));
}
