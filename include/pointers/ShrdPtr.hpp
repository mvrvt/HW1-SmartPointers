#pragma once

#include <cassert>     // assert
#include <concepts>    // std::convertible_to
#include <cstddef>     // size_t, std::nullptr_t
#include <type_traits> // std::is_array_v, std::is_unbounded_array_v
#include <utility>     // std::exchange, std::swap, std::forward

#include "UnqPtr.hpp"

// ========== ShrdPtr<T> — разделяемое владение одиночным объектом ==========
// Все копии, указывающие на один объект, делят один счётчик ссылок (size_t в куче).
// Объект удаляется, когда умирает последняя копия. Удаляет его UnqPtr:
// сам ShrdPtr никогда не вызывает delete для объекта.
template <typename T>
class ShrdPtr {
public:
    using ElementType = T;
    using Pointer = T*;
    //TODO: как проверить тип указателя в main или в тестах? Приведи примеры чтобы я понимал 

    ShrdPtr() noexcept : ptr_(nullptr), ref_count_(nullptr) {}
    //TODO: обязателен ли конструктор, который в аргументы принимает nullptr? У нас же принимается и сырой указатель и UnqPtr и без аргументов вовсе.
    ShrdPtr(std::nullptr_t) noexcept : ptr_(nullptr), ref_count_(nullptr) {}

    // Забираем объект у UnqPtr (подходит и UnqPtr<Derived> для ShrdPtr<Base>).
    // Сначала выделяем счётчик: если new бросит исключение,
    // объектом всё ещё владеет owner, и он его удалит - утечки нет.
    template <typename U>
    requires std::convertible_to<U*, T*>
    ShrdPtr(UnqPtr<U>&& owner) : ptr_(nullptr), ref_count_(nullptr) {
        if (!owner) //TODO: что конкретно здесь проверяется? Наличие UnqPtr'а на вход как аргумент? То есть не равен ли UnqPtr nullptr'у? 
            return; //TODO: может тогда если UnqPtr всё-таки nullptr либо создадим ShrdPtr как из конструктора по умолчанию, либо выведем ошибку std::invalid_argument?
        ref_count_ = new size_t(1);
        ptr_ = owner.Release();
    }

    // Из сырого указателя: сначала отдаём его временному UnqPtr, 
    // дальше сработает конструктор выше. Если new для счётчика бросит,
    // временный UnqPtr удалит объект утечки - утечки нет. 
    explicit ShrdPtr(T* ptr) : ShrdPtr(UnqPtr<T>(ptr)) {}

    ShrdPtr(const ShrdPtr& other) noexcept : ptr_(other.ptr_), ref_count_(other.ref_count_) {
        AddRef();
    }

    //TODO: почему в этом конструкторе используем std::exchange, а не std::move? 
    ShrdPtr(ShrdPtr&& other) noexcept
        : ptr_(std::exchange(other.ptr_, nullptr)),
          ref_count_(std::exchange(other.ref_count_, nullptr)) {}

    // Подтипизация: ShrdPtr<Derived> -> ShrdPtr<Base>, ShrdPtr<T> -> ShrdPtr<const T>
    template <typename U>
    requires std::convertible_to<U*, T*>
    ShrdPtr(const ShrdPtr<U>& other) noexcept : ptr_(other.ptr_), ref_count_(other.ref_count_) {
        AddRef();
    }

    template <typename U>
    requires std::convertible_to<U*, T*>
    ShrdPtr(ShrdPtr<U>&& other) noexcept 
        : ptr_(std::exchange(other.ptr_, nullptr)),
          ref_count_(std::exchange(other.ref_count_, nullptr)) {}
    
    // Одно присваивание на все случаи (copy-and-swap).
    // other — уже готовая копия правой части (или перемещённый в неё объект).
    // Меняемся с ним содержимым, а наше старое содержимое уйдёт
    // вместе с other, когда он умрёт в конце функции.
    //TODO: как этот оператор вообще работает? На какие все случаи это присваивание? Это же просто оператор присваивания копированием, разве нет? Тогда почему нельзя написать его по-обычному?
    ShrdPtr& operator=(ShrdPtr other) noexcept {
        Swap(other);
        return *this; 
    }

    ~ShrdPtr() noexcept {
        RemoveRef();
    }

    //TODO: Разве это дереференсинг?
    T& operator*() const noexcept {
        assert(ptr_ != nullptr && "Dereferencing empty ShrdPtr"); //TODO: Ты говорил, что мы используем assert'ы поскольку мы проверяем на ошибки программиста, но разве не логичнее будет выдавать std::invalid_argument с сообщением внутри о том, что разыменовывается указатель равный nullptr? Еще раз расскажи почему мы используем именно ассерты, а не выводим ошибки и хорошо аргументируй свою позицию, либо расскажи почему сделать мой вариант с ошибками будет более правильно
        return *ptr_;
    }

    //TODO: обращение по оператору стрелочка называется Member access? 
    T* operator->() const noexcept {
        assert(ptr_ != nullptr && "Member access through empty ShrdPtr");
        return ptr_;
    }

    T* Get() const noexcept { return ptr_; }
    //TODO: что делает этот оператор bool()? Что он значит и для чего он нужен, если ниже мы перегрузим оператор "=="? 
    explicit operator bool() const noexcept { return ptr_ != nullptr; }

    // Сколько ShrdPtr сейчас владеют объектом (0 - если указатель пустой)
    size_t UseCount() const noexcept {
        return ref_count_ != nullptr ? *ref_count_ : 0;
    }

    // Перестать владеть объектом (он удалится, если мы были последними)
    void Reset() noexcept {
        ShrdPtr empty;
        Swap(empty);
    } // здесь empty (уже со старым объектом) умирает и уменьшает его счётчик

    //TODO: разве нельзя сделать как с ЮнкПтром? Сделать один метод Reset(T* ptr = nullptr), который будет распространяться на оба случая
    //TODO: Where's noexcept?
    void Reset(T* ptr) {
        if (ptr == ptr_) // p.Reset(p.Get()) не должен создать второй счётчик
            return;
        ShrdPtr new_owner(ptr);
        Swap(new_owner);
    } // здесь new_owner со старым объектом умирает
    //TODO: wouldn't it be easier to do this method with the exchange? 

    void Swap(ShrdPtr& other) noexcept {
        std::swap(ptr_, other.ptr_);
        std::swap(ref_count_, other.ref_count_);
    }

    friend void swap(ShrdPtr& lhs, ShrdPtr& rhs) noexcept { //TODO: Что конкретно значит этот метод свап? Зачем он нужен, если внутри он использует выше написанный Swap? Что здесь такое lhs, rhs, почему он сделан friend'ом и что этот статус "друга" вообще даёт этому методу?
        lhs.Swap(rhs);
    }

    bool operator==(const ShrdPtr& other) const noexcept { return ptr_ == other.ptr_; }
    bool operator==(std::nullptr_t) const noexcept { return ptr_ == nullptr; }

private:
    // ShrdPtr<Base> читает private-поля ShrdPtr<Derived> в конструкторах подтипизации
    template <typename U>
    friend class ShrdPtr; //TODO: Так мы сделали для того чтобы ShrdPtr<Base> видел private-поля ShrdPtr<Derived> и мог с помощью концепта понять можно ли Base преобразовать в Derived? Объясни попонятнее, если моя версия неверна.

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
            UnqPtr<T> owner(ptr_); // возвращаем объект UnqPtr - он его и удалит //TODO: Как конкретно UnqPtr удалит его? Мы же просто явно создали UnqPtr, или это произойдет из-за того что с концом метода RemoveRef() область видимости этого указателя закончится и будет вызван деструктор? 
        }
    }

    //TODO: понять почему в UnqPtr'е поля были проинициализированы сразу, а в Shrd'е клод сначала не проинициализировал поля как nullptr'ы (Объясни это)
    T* ptr_ = nullptr;
    size_t*    ref_count_ = nullptr;
};

// ========== ShrdPtr<T[]> — разделяемое владение динамическим массивом ==========
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
 
    // Только T* (или T* -> const T*), но НЕ Derived* — как у UnqPtr<T[]>
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
 
    // Reset() и Reset(nullptr) — перестать владеть массивом
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
 
    friend void swap(ShrdPtr& lhs, ShrdPtr& rhs) noexcept {
        lhs.Swap(rhs);
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
 
    T* ptr_;
    size_t* ref_count_;
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
