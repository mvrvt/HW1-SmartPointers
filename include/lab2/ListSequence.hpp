#pragma once

#include <cstddef> // std::size_t

#include "LinkedList.hpp"
#include "Sequence.hpp"

// ========== ListSequence<T> - последовательность на LinkedList ==========
// Общая часть для MutableListSequence и ImmutableListSequence.
// Append, Prepend, GetFirst, GetLast - O(1); Get(i), InsertAt, RemoveAt - O(n).
// Для обхода используйте for (const T& item : sequence) - это O(n), а Get(i) в цикле - O(n^2).
template <class T>
class ListSequence : public Sequence<T> {
public:
    ListSequence() = default;

    // Копия count элементов обычного массива items
    ListSequence(const T* items, std::size_t count) : items_(items, count) {}

    // Копия связного списка
    explicit ListSequence(const LinkedList<T>& list) : items_(list) {}

    // Копия любой другой последовательности (например, ArraySequence -> ListSequence)
    explicit ListSequence(const Sequence<T>& other) {
        for (const T& item : other)
            items_.Append(item);
    }

    // Копирование, присваивание, перемещение и деструктор генерирует компилятор:
    // items_ хранится по значению, а LinkedList сам умеет копироваться и удаляться.

    const T& Get(std::size_t index) const override { return items_.Get(index); }
    std::size_t GetLength() const override { return items_.GetCount(); }
    const T& GetFirst() const override { return items_.GetFirst(); }
    const T& GetLast() const override { return items_.GetLast(); }

    // Быстрый обход: for (const T& item : sequence)
    typename LinkedList<T>::ConstIterator begin() const { return items_.begin(); }
    typename LinkedList<T>::ConstIterator end() const { return items_.end(); }

    IEnumerator<T>* GetEnumerator() const override { return items_.GetEnumerator(); }

protected:
    void AppendInternal(const T& item) override { items_.Append(item); }
    void PrependInternal(const T& item) override { items_.Prepend(item); }
    void InsertAtInternal(const T& item, std::size_t index) override { items_.InsertAt(item, index); }
    void RemoveAtInternal(std::size_t index) override { items_.RemoveAt(index); }
    void SetInternal(std::size_t index, const T& item) override { items_.Set(index, item); }
    void ClearInternal() override { items_.Clear(); }

    LinkedList<T> items_;
};

// ---------- Изменяемая: Append и т.п. меняют сам объект и возвращают this ----------
template <class T>
class MutableListSequence : public ListSequence<T> {
public:
    MutableListSequence() = default;
    MutableListSequence(const T* items, std::size_t count) : ListSequence<T>(items, count) {}
    explicit MutableListSequence(const LinkedList<T>& list) : ListSequence<T>(list) {}
    explicit MutableListSequence(const Sequence<T>& other) : ListSequence<T>(other) {}

    Sequence<T>* CreateEmpty() const override { return new MutableListSequence<T>(); }

    // Только у изменяемой последовательности - доступ к элементам по ссылке:
    //     seq[0] = 5;   for (int& x : seq) x *= 2;
    using ListSequence<T>::Get;
    using ListSequence<T>::operator[];
    using ListSequence<T>::begin;
    using ListSequence<T>::end;

    T& Get(std::size_t index) { return this->items_.Get(index); }        // O(n)
    T& operator[](std::size_t index) { return this->items_.Get(index); } // O(n)
    typename LinkedList<T>::Iterator begin() { return this->items_.begin(); }
    typename LinkedList<T>::Iterator end() { return this->items_.end(); }

protected:
    Sequence<T>* Instance() override { return this; }
};

// ---------- Неизменяемая: Append и т.п. возвращают новый объект (его нужно удалить) ----------
template <class T>
class ImmutableListSequence : public ListSequence<T> {
public:
    ImmutableListSequence() = default;
    ImmutableListSequence(const T* items, std::size_t count) : ListSequence<T>(items, count) {}
    explicit ImmutableListSequence(const LinkedList<T>& list) : ListSequence<T>(list) {}
    explicit ImmutableListSequence(const Sequence<T>& other) : ListSequence<T>(other) {}

    Sequence<T>* CreateEmpty() const override { return new ImmutableListSequence<T>(); }

protected:
    Sequence<T>* Instance() override { return new ImmutableListSequence<T>(*this); }
};
