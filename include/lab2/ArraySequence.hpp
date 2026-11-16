#pragma once

#include <cstddef> // std::size_t

#include "DynamicArray.hpp"
#include "Sequence.hpp"

// ========== ArraySequence<T> - последовательность на DynamicArray ==========
// Общая часть для MutableArraySequence и ImmutableArraySequence.
// Get(i) - O(1), Append - в среднем O(1), Prepend/InsertAt/RemoveAt - O(n).
template <class T>
class ArraySequence : public Sequence<T> {
public:
    ArraySequence() = default;

    // Копия count элементов обычного массива items
    ArraySequence(const T* items, std::size_t count) : items_(items, count) {}

    // Копия динамического массива
    explicit ArraySequence(const DynamicArray<T>& array) : items_(array) {}

    // Копия любой другой последовательности (например, ListSequence -> ArraySequence)
    explicit ArraySequence(const Sequence<T>& other) {
        items_.Reserve(other.GetLength());
        for (const T& item : other)
            items_.Append(item);
    }

    // Копирование, присваивание, перемещение и деструктор генерирует компилятор:
    // items_ хранится по значению, а DynamicArray сам умеет копироваться и удаляться.

    const T& Get(std::size_t index) const override { return items_.Get(index); }
    std::size_t GetLength() const override { return items_.GetCount(); }
    const T& GetFirst() const override { return items_.GetFirst(); }
    const T& GetLast() const override { return items_.GetLast(); }

    // Быстрый обход (обычные указатели): for (const T& item : sequence)
    const T* begin() const { return items_.begin(); }
    const T* end() const { return items_.end(); }

    IEnumerator<T>* GetEnumerator() const override { return items_.GetEnumerator(); }

protected:
    void AppendInternal(const T& item) override { items_.Append(item); }
    void PrependInternal(const T& item) override { items_.Prepend(item); }
    void InsertAtInternal(const T& item, std::size_t index) override { items_.InsertAt(item, index); }
    void RemoveAtInternal(std::size_t index) override { items_.RemoveAt(index); }
    void SetInternal(std::size_t index, const T& item) override { items_.Set(index, item); }
    void ClearInternal() override { items_.Clear(); }

    DynamicArray<T> items_;
};

// ---------- Изменяемая: Append и т.п. меняют сам объект и возвращают this ----------
template <class T>
class MutableArraySequence : public ArraySequence<T> {
public:
    MutableArraySequence() = default;
    MutableArraySequence(const T* items, std::size_t count) : ArraySequence<T>(items, count) {}
    explicit MutableArraySequence(const DynamicArray<T>& array) : ArraySequence<T>(array) {}
    explicit MutableArraySequence(const Sequence<T>& other) : ArraySequence<T>(other) {}

    Sequence<T>* CreateEmpty() const override { return new MutableArraySequence<T>(); }

    // Только у изменяемой последовательности - доступ к элементам по ссылке:
    //     seq[0] = 5;   for (int& x : seq) x *= 2;
    using ArraySequence<T>::Get;
    using ArraySequence<T>::operator[];
    using ArraySequence<T>::begin;
    using ArraySequence<T>::end;

    T& Get(std::size_t index) { return this->items_.Get(index); }
    T& operator[](std::size_t index) { return this->items_.Get(index); }
    T* begin() { return this->items_.begin(); }
    T* end() { return this->items_.end(); }

    // Заранее выделить память под capacity элементов (полезно перед серией Append)
    void Reserve(std::size_t capacity) { this->items_.Reserve(capacity); }
    std::size_t GetCapacity() const { return this->items_.GetCapacity(); }

protected:
    Sequence<T>* Instance() override { return this; }
};

// ---------- Неизменяемая: Append и т.п. возвращают новый объект (его нужно удалить) ----------
template <class T>
class ImmutableArraySequence : public ArraySequence<T> {
public:
    ImmutableArraySequence() = default;
    ImmutableArraySequence(const T* items, std::size_t count) : ArraySequence<T>(items, count) {}
    explicit ImmutableArraySequence(const DynamicArray<T>& array) : ArraySequence<T>(array) {}
    explicit ImmutableArraySequence(const Sequence<T>& other) : ArraySequence<T>(other) {}

    Sequence<T>* CreateEmpty() const override { return new ImmutableArraySequence<T>(); }

protected:
    Sequence<T>* Instance() override { return new ImmutableArraySequence<T>(*this); }
};
