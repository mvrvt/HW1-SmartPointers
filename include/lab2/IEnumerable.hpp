#pragma once

#include "IEnumerator.hpp"

// Обёртка над IEnumerator<T>*, чтобы любую IEnumerable-коллекцию можно было
// обходить циклом for (const T& item : collection).
// Сама удаляет итератор в деструкторе, поэтому delete писать не нужно.
// Копировать нельзя (итератор у неё один), перемещать можно.
template <class T>
class EnumeratorIterator {
public:
    EnumeratorIterator() = default; // итератор "конец"

    explicit EnumeratorIterator(IEnumerator<T>* enumerator) : enumerator_(enumerator) {
        MoveToNext(); // встаём на первый элемент (или сразу в "конец", если коллекция пуста)
    }

    EnumeratorIterator(const EnumeratorIterator&) = delete;
    EnumeratorIterator& operator=(const EnumeratorIterator&) = delete;

    EnumeratorIterator(EnumeratorIterator&& other) noexcept : enumerator_(other.enumerator_) {
        other.enumerator_ = nullptr;
    }

    EnumeratorIterator& operator=(EnumeratorIterator&& other) noexcept {
        if (this != &other) {
            delete enumerator_;
            enumerator_ = other.enumerator_;
            other.enumerator_ = nullptr;
        }
        return *this;
    }

    ~EnumeratorIterator() { delete enumerator_; }

    const T& operator*() const { return enumerator_->Current(); }
    const T* operator->() const { return &enumerator_->Current(); }

    EnumeratorIterator& operator++() {
        MoveToNext();
        return *this;
    }

    // Итератор, дошедший до конца, хранит nullptr - поэтому он равен end()
    bool operator==(const EnumeratorIterator& other) const { return enumerator_ == other.enumerator_; }
    bool operator!=(const EnumeratorIterator& other) const { return enumerator_ != other.enumerator_; }

private:
    void MoveToNext() {
        if (enumerator_ != nullptr && !enumerator_->MoveNext()) {
            delete enumerator_;
            enumerator_ = nullptr;
        }
    }

    IEnumerator<T>* enumerator_ = nullptr;
};

template <class T>
class IEnumerable {
public:
    virtual ~IEnumerable() = default;

    // Создаёт новый итератор в куче. Его удаляет вызывающий (delete),
    // либо можно пользоваться циклом for (const T& item : collection) - там удалять ничего не надо.
    virtual IEnumerator<T>* GetEnumerator() const = 0;

    // Обход любой коллекции циклом for, только для чтения.
    // Конкретные классы (DynamicArray, LinkedList, ...) объявляют свои, более быстрые begin()/end(),
    // которые перекрывают эти. Эти используются, когда известен лишь базовый тип (например, Sequence<T>&).
    EnumeratorIterator<T> begin() const { return EnumeratorIterator<T>(GetEnumerator()); }
    EnumeratorIterator<T> end() const { return EnumeratorIterator<T>(); }
};
