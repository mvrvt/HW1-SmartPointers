#pragma once

#include <cassert>     // assert
#include <concepts>    // std::convertible_to
#include <cstddef>     // size_t, std::nullptr_t
#include <type_traits> // std::is_array_v, std::is_unbounded_array_v
#include <utility>     // std::exchange, std::swap, std::forward

#include "UnqPtr.hpp"

// ========== ShrdPtr<T> - разделяемое владение одиночным объектом ==========
// Все копии, указывающие на один объект, делят один счётчик ссылок (size_t в куче).
// Объект удаляется, когда умирает последняя копия. Удаляет его UnqPtr:
// сам ShrdPtr никогда не вызывает delete для объекта.
template <typename T>
class ShrdPtr {
public:
    using ElementType = T;
    using Pointer = T*;

    ShrdPtr() noexcept : ptr_(nullptr), ref_count_(nullptr) {}
    ShrdPtr(std::nullptr_t) noexcept : ptr_(nullptr), ref_count_(nullptr) {}

    // Забираем объект у UnqPtr (подходит и UnqPtr<Derived> для ShrdPtr<Base>).
    // Сначала выделяем счётчик: если new бросит исключение,
    // объектом всё ещё владеет owner, и он его удалит - утечки нет.
    template <typename U>
    requires std::convertible_to<U*, T*> && SafeUpcast<U, T>
    ShrdPtr(UnqPtr<U>&& owner) : ptr_(nullptr), ref_count_(nullptr) {
        if (!owner) 
            return; 
        ref_count_ = new size_t(1);
        ptr_ = owner.Release();
    }

    // Из сырого указателя: сначала отдаём его временному UnqPtr, 
    // дальше сработает конструктор выше. Если new для счётчика бросит,
    // временный UnqPtr удалит объект - утечки нет. 
    explicit ShrdPtr(T* ptr) : ShrdPtr(UnqPtr<T>(ptr)) {}

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
    
    // Одно присваивание на все случаи (copy-and-swap).
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

    size_t UseCount() const noexcept {
        return ref_count_ != nullptr ? *ref_count_ : 0;
    }

    // Перестать владеть объектом (он удалится, если мы были последними)
    void Reset() noexcept {
        ShrdPtr empty;
        Swap(empty);
    } // здесь empty (со старым объектом) умирает и уменьшает его счётчик

    
    void Reset(T* ptr) {
        if (ptr == ptr_) // p.Reset(p.Get()) не должен создать второй счётчик
            return;
        ShrdPtr new_owner(ptr);
        Swap(new_owner);
    } // здесь new_owner со старым объектом умирает

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
    size_t* ref_count_ = nullptr;
};

// ========== ShrdPtr<T[]> - разделяемое владение динамическим массивом ==========
// Подтипизации нет (как и у UnqPtr<T[]>), можно только добавить const.
template <typename T>
class ShrdPtr<T[]> {
public:
    using ElementType = T;  // для ShrdPtr<int[]> это int, а не int[]
    using Pointer = T*;
 
    ShrdPtr() noexcept : ptr_(nullptr), ref_count_(nullptr) {}
    ShrdPtr(std::nullptr_t) noexcept : ptr_(nullptr), ref_count_(nullptr) {}
 
    template <typename U>
    requires std::convertible_to<U(*)[], T(*)[]>
    ShrdPtr(UnqPtr<U[]>&& owner) : ptr_(nullptr), ref_count_(nullptr) {
        if (!owner)
            return;
        ref_count_ = new size_t(1);
        ptr_ = owner.Release();
    }
 
    // Только T* (или T* -> const T*), но НЕ Derived*
    template <typename U>
    requires std::convertible_to<U(*)[], T(*)[]>
    explicit ShrdPtr(U* ptr) : ShrdPtr(UnqPtr<U[]>(ptr)) {}
 
    ShrdPtr(const ShrdPtr& other) noexcept : ptr_(other.ptr_), ref_count_(other.ref_count_) {
        AddRef();
    }
 
    ShrdPtr(ShrdPtr&& other) noexcept
        : ptr_(std::exchange(other.ptr_, nullptr)),
          ref_count_(std::exchange(other.ref_count_, nullptr)) {}
 
    // Только добавление const: ShrdPtr<int[]> -> ShrdPtr<const int[]>
    template <typename U>
    requires std::convertible_to<U(*)[], T(*)[]>
    ShrdPtr(const ShrdPtr<U[]>& other) noexcept : ptr_(other.ptr_), ref_count_(other.ref_count_) {
        AddRef();
    }
 
    template <typename U>
    requires std::convertible_to<U(*)[], T(*)[]>
    ShrdPtr(ShrdPtr<U[]>&& other) noexcept
        : ptr_(std::exchange(other.ptr_, nullptr)),
          ref_count_(std::exchange(other.ref_count_, nullptr)) {}
 
    ShrdPtr& operator=(ShrdPtr other) noexcept {
        Swap(other);
        return *this;
    }
 
    ~ShrdPtr() noexcept {
        RemoveRef();
    }
 
    // Границы не проверяются: размер массива ShrdPtr неизвестен
    T& operator[](size_t index) const noexcept {
        assert(ptr_ != nullptr && "Indexing empty ShrdPtr");
        return ptr_[index];
    }
 
    T* Get() const noexcept { return ptr_; }
    explicit operator bool() const noexcept { return ptr_ != nullptr; }
 
    size_t UseCount() const noexcept {
        return ref_count_ != nullptr ? *ref_count_ : 0;
    }

    // Reset() и Reset(nullptr) - перестать владеть массивом
    void Reset(std::nullptr_t = nullptr) noexcept {
        ShrdPtr empty;
        Swap(empty);
    }
 
    template <typename U>
    requires std::convertible_to<U(*)[], T(*)[]>
    void Reset(U* ptr) {
        if (ptr == ptr_)
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
            UnqPtr<T[]> owner(ptr_);  // UnqPtr<T[]> удалит массив через delete[]
        }
    }
 
    T* ptr_ = nullptr;
    size_t* ref_count_ = nullptr;
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
[[nodiscard]] ShrdPtr<T> MakeShrd(size_t size) {
    return ShrdPtr<T>(MakeUnq<T>(size));
}
