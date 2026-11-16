#pragma once

#include <cstddef>   // std::size_t
#include <stdexcept> // std::invalid_argument

#include "Exceptions.hpp"
#include "ICollection.hpp"
#include "IEnumerable.hpp"

// ========== Sequence<T> - абстрактная последовательность ==========
// Наследники: MutableArraySequence, ImmutableArraySequence (ArraySequence.hpp),
//             MutableListSequence,  ImmutableListSequence  (ListSequence.hpp).
//
// Методы, которые ИЗМЕНЯЮТ последовательность (Append, Prepend, InsertAt, RemoveAt, Set, Clear),
// возвращают Sequence<T>*:
//   - Mutable-версия меняет саму себя и возвращает this     -> удалять результат НЕЛЬЗЯ;
//   - Immutable-версия себя не меняет, возвращает НОВЫЙ объект -> результат нужно удалить (delete).
//
// Методы, которые СОЗДАЮТ новую последовательность (Clone, GetSubsequence, Concat, Skip, Take,
// Map, Where), всегда возвращают новый объект того же вида (Mutable/Immutable, Array/List),
// его всегда нужно удалить (delete).
template <class T>
class Sequence : public ICollection<T>, public IEnumerable<T> {
public:
    // Результат IndexOf, если элемент не найден
    static constexpr std::size_t kNotFound = static_cast<std::size_t>(-1);

    ~Sequence() override = default;

    // ---------- Информация и доступ ----------
    // Get(index) объявлен в ICollection
    std::size_t GetCount() const override { return GetLength(); }

    virtual std::size_t GetLength() const = 0;
    virtual const T&    GetFirst()  const = 0; // бросает EmptyCollection
    virtual const T&    GetLast()   const = 0; // бросает EmptyCollection

    const T& operator[](std::size_t index) const { return this->Get(index); }

    // ---------- Изменение (см. комментарий в начале файла) ----------
    Sequence<T>* Append(const T& item) {
        Sequence<T>* target = Instance();
        try {
            target->AppendInternal(item);
        } catch (...) {
            DeleteIfCopy(target);
            throw;
        }
        return target;
    }

    Sequence<T>* Prepend(const T& item) {
        Sequence<T>* target = Instance();
        try {
            target->PrependInternal(item);
        } catch (...) {
            DeleteIfCopy(target);
            throw;
        }
        return target;
    }

    // Новый элемент получит индекс index; index == GetLength() - вставка в конец
    Sequence<T>* InsertAt(const T& item, std::size_t index) {
        if (index > GetLength()) // проверяем до копирования, чтобы Immutable-копия не утекла
            throw IndexOutOfRange("Sequence::InsertAt: index out of range");
        Sequence<T>* target = Instance();
        try {
            target->InsertAtInternal(item, index);
        } catch (...) {
            DeleteIfCopy(target);
            throw;
        }
        return target;
    }

    Sequence<T>* RemoveAt(std::size_t index) {
        if (index >= GetLength())
            throw IndexOutOfRange("Sequence::RemoveAt: index out of range");
        Sequence<T>* target = Instance();
        try {
            target->RemoveAtInternal(index);
        } catch (...) {
            DeleteIfCopy(target);
            throw;
        }
        return target;
    }

    Sequence<T>* Set(std::size_t index, const T& item) {
        if (index >= GetLength())
            throw IndexOutOfRange("Sequence::Set: index out of range");
        Sequence<T>* target = Instance();
        try {
            target->SetInternal(index, item);
        } catch (...) {
            DeleteIfCopy(target);
            throw;
        }
        return target;
    }

    Sequence<T>* Clear() {
        Sequence<T>* target = Instance();
        target->ClearInternal();
        return target;
    }

    // ---------- Создание новых последовательностей (результат всегда удалять) ----------

    // Пустая последовательность того же вида
    virtual Sequence<T>* CreateEmpty() const = 0;

    // Полная копия
    Sequence<T>* Clone() const { return CopyRange(0, GetLength()); }

    // Элементы с индексами start_index..end_index включительно
    Sequence<T>* GetSubsequence(std::size_t start_index, std::size_t end_index) const {
        if (end_index >= GetLength())
            throw IndexOutOfRange("Sequence::GetSubsequence: end_index out of range");
        if (start_index > end_index)
            throw std::invalid_argument("Sequence::GetSubsequence: start_index > end_index");
        return CopyRange(start_index, end_index + 1);
    }

    // Сначала элементы this, затем элементы other
    Sequence<T>* Concat(const Sequence<T>& other) const {
        Sequence<T>* result = Clone();
        try {
            for (const T& item : other)
                result->AppendInternal(item);
        } catch (...) {
            delete result;
            throw;
        }
        return result;
    }

    // Все элементы, кроме первых count
    Sequence<T>* Skip(std::size_t count) const {
        std::size_t from = (count < GetLength()) ? count : GetLength();
        return CopyRange(from, GetLength());
    }

    // Первые count элементов (или все, если их меньше)
    Sequence<T>* Take(std::size_t count) const {
        std::size_t to = (count < GetLength()) ? count : GetLength();
        return CopyRange(0, to);
    }

    // [f(a0), f(a1), ...]. func - функция или лямбда вида T func(const T& item)
    template <class Func>
    Sequence<T>* Map(Func func) const {
        Sequence<T>* result = CreateEmpty();
        try {
            for (const T& item : *this)
                result->AppendInternal(func(item));
        } catch (...) {
            delete result;
            throw;
        }
        return result;
    }

    // Только элементы, для которых predicate вернул true. predicate - вида bool predicate(const T& item)
    template <class Func>
    Sequence<T>* Where(Func predicate) const {
        Sequence<T>* result = CreateEmpty();
        try {
            for (const T& item : *this) {
                if (predicate(item))
                    result->AppendInternal(item);
            }
        } catch (...) {
            delete result;
            throw;
        }
        return result;
    }

    // Свёртка: func(...func(func(initial, a0), a1)..., an).
    // func - вида T2 func(const T2& accumulated, const T& item); T2 может отличаться от T,
    // например сумма Sequence<int> в long long: seq.Reduce(Sum, 0LL)
    template <class T2, class Func>
    T2 Reduce(Func func, T2 initial) const {
        T2 result = initial;
        for (const T& item : *this)
            result = func(result, item);
        return result;
    }

    // ---------- Поиск и сравнение (нужен operator== у T) ----------

    // Индекс первого элемента, равного item, или kNotFound
    std::size_t IndexOf(const T& item) const {
        std::size_t index = 0;
        for (const T& current : *this) {
            if (current == item)
                return index;
            ++index;
        }
        return kNotFound;
    }

    bool Contains(const T& item) const { return IndexOf(item) != kNotFound; }

    // Одинаковые элементы в одинаковом порядке (вид последовательности не важен)
    bool operator==(const Sequence<T>& other) const {
        if (GetLength() != other.GetLength())
            return false;
        EnumeratorIterator<T> other_item = other.begin();
        for (const T& item : *this) {
            if (!(item == *other_item))
                return false;
            ++other_item;
        }
        return true;
    }

    bool operator!=(const Sequence<T>& other) const { return !(*this == other); }

protected:
    // Mutable: возвращает this. Immutable: возвращает новую копию себя
    virtual Sequence<T>* Instance() = 0;

    // Изменяют именно этот объект. Индексы уже проверены в публичных методах выше
    virtual void AppendInternal(const T& item)                      = 0;
    virtual void PrependInternal(const T& item)                     = 0;
    virtual void InsertAtInternal(const T& item, std::size_t index) = 0;
    virtual void RemoveAtInternal(std::size_t index)                = 0;
    virtual void SetInternal(std::size_t index, const T& item)      = 0;
    virtual void ClearInternal()                                    = 0;

private:
    // Новая последовательность из элементов с индексами [from, to)
    Sequence<T>* CopyRange(std::size_t from, std::size_t to) const {
        Sequence<T>* result = CreateEmpty();
        try {
            std::size_t index = 0;
            for (const T& item : *this) {
                if (index >= to)
                    break;
                if (index >= from)
                    result->AppendInternal(item);
                ++index;
            }
        } catch (...) {
            delete result;
            throw;
        }
        return result;
    }

    // Если операция над Immutable-копией не удалась, копию нужно удалить
    void DeleteIfCopy(Sequence<T>* target) {
        if (target != this)
            delete target;
    }
};
