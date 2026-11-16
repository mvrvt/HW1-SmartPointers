#pragma once

#include <cstddef>     // std::size_t
#include <stdexcept>   // std::invalid_argument
#include <type_traits> // std::is_trivially_destructible_v
#include <utility>     // std::move, std::swap

#include "Exceptions.hpp"
#include "ICollection.hpp"
#include "IEnumerable.hpp"
#include "pointers/UnqPtr.hpp"

// ========== DynamicArray<T> - динамический массив (замена std::vector) ==========
// Элементы лежат подряд в одном буфере из кучи. Ёмкость (capacity) растёт вдвое,
// поэтому Append в среднем выполняется за O(1).
//
// Требования к T: конструктор по умолчанию (буфер создаётся через new T[]) и
// присваивание. Можно хранить и некопируемые типы (например, UnqPtr) - тогда
// нельзя копировать сам массив, а добавлять элементы нужно через std::move.
//
// Буфером владеет умный указатель UnqPtr<T[]>: своего delete[] в классе нет,
// память освобождается автоматически (в деструкторе, при исключении, при замене буфера).
template <class T>
class DynamicArray : public ICollection<T>, public IEnumerable<T> {
public:
    // ---------- Итератор для IEnumerable ----------
    class ArrayEnumerator : public IEnumerator<T> {
    public:
        explicit ArrayEnumerator(const DynamicArray<T>* array) : array_(array) {}

        bool MoveNext() override {
            if (!started_)
                started_ = true;
            else if (index_ < array_->GetCount())
                ++index_;
            return index_ < array_->GetCount();
        }

        const T& Current() const override {
            if (!started_ || index_ >= array_->GetCount())
                throw IndexOutOfRange("ArrayEnumerator: no current element");
            return array_->Get(index_);
        }

        void Reset() override {
            started_ = false;
            index_ = 0;
        }

    private:
        const DynamicArray<T>* array_;
        std::size_t index_ = 0;
        bool started_ = false;
    };

    // ---------- Конструкторы, копирование, перемещение ----------
    DynamicArray() = default;

    // count элементов, равных T() (для чисел - нули)
    explicit DynamicArray(std::size_t count) {
        Allocate(count);
        for (std::size_t i = 0; i < count; ++i)
            data_[i] = T(); // если бросит - буфер удалит деструктор поля data_
        size_ = count;
    }

    // count копий value
    DynamicArray(std::size_t count, const T& value) {
        Allocate(count);
        for (std::size_t i = 0; i < count; ++i)
            data_[i] = value; // если бросит - буфер удалит деструктор поля data_
        size_ = count;
    }

    // Копия count элементов обычного массива items
    DynamicArray(const T* items, std::size_t count) {
        if (items == nullptr && count > 0)
            throw std::invalid_argument("DynamicArray: items is nullptr");
        CopyFrom(items, count);
    }

    DynamicArray(const DynamicArray<T>& other) { CopyFrom(other.data_.Get(), other.size_); }

    DynamicArray(DynamicArray<T>&& other) noexcept
        : data_(std::move(other.data_)),
          size_(std::exchange(other.size_, 0)),
          capacity_(std::exchange(other.capacity_, 0)) {}

    // Copy-and-swap: если копирование бросит исключение, *this не изменится
    DynamicArray<T>& operator=(const DynamicArray<T>& other) {
        if (this != &other) {
            DynamicArray<T> copy(other);
            Swap(copy);
        }
        return *this;
    }

    DynamicArray<T>& operator=(DynamicArray<T>&& other) noexcept {
        if (this != &other) {
            data_ = std::move(other.data_); // старый буфер удалит UnqPtr
            size_ = std::exchange(other.size_, 0);
            capacity_ = std::exchange(other.capacity_, 0);
        }
        return *this;
    }

    // Буфер освобождает деструктор поля data_ (UnqPtr<T[]>)
    ~DynamicArray() override = default;

    // ---------- Информация ----------
    std::size_t GetCount() const override { return size_; }
    std::size_t GetSize() const { return size_; } // то же, что GetCount (название из задания)
    std::size_t GetCapacity() const { return capacity_; }

    // ---------- Доступ (все методы проверяют индекс) ----------
    const T& Get(std::size_t index) const override {
        CheckIndex(index);
        return data_[index];
    }

    T& Get(std::size_t index) {
        CheckIndex(index);
        return data_[index];
    }

    const T& operator[](std::size_t index) const { return Get(index); }
    T& operator[](std::size_t index) { return Get(index); }

    const T& GetFirst() const {
        CheckNotEmpty();
        return data_[0];
    }

    T& GetFirst() {
        CheckNotEmpty();
        return data_[0];
    }

    const T& GetLast() const {
        CheckNotEmpty();
        return data_[size_ - 1];
    }

    T& GetLast() {
        CheckNotEmpty();
        return data_[size_ - 1];
    }

    void Set(std::size_t index, const T& value) {
        CheckIndex(index);
        data_[index] = value;
    }

    void Set(std::size_t index, T&& value) {
        CheckIndex(index);
        data_[index] = std::move(value);
    }

    // ---------- Изменение ----------

    // Добавить в конец, в среднем O(1)
    void Append(const T& value) {
        if (size_ < capacity_) {
            data_[size_] = value;
        } else {
            // value может ссылаться на элемент этого же массива (arr.Append(arr[0])),
            // а старый буфер при росте удаляется - поэтому сначала копируем
            T copy(value);
            Grow(size_ + 1);
            data_[size_] = std::move(copy);
        }
        ++size_;
    }

    void Append(T&& value) {
        if (size_ < capacity_) {
            data_[size_] = std::move(value);
        } else {
            T moved(std::move(value));
            Grow(size_ + 1);
            data_[size_] = std::move(moved);
        }
        ++size_;
    }

    // Добавить в начало, O(n)
    void Prepend(const T& value) { InsertAt(value, 0); }
    void Prepend(T&& value) { InsertAt(std::move(value), 0); }

    // Вставить так, чтобы новый элемент получил индекс index; index == GetCount() - вставка в конец. O(n)
    void InsertAt(const T& value, std::size_t index) {
        if (index > size_)
            throw IndexOutOfRange("DynamicArray::InsertAt: index out of range");
        T copy(value); // см. комментарий в Append
        InsertAt(std::move(copy), index);
    }

    void InsertAt(T&& value, std::size_t index) {
        if (index > size_)
            throw IndexOutOfRange("DynamicArray::InsertAt: index out of range");
        T moved(std::move(value)); // value может быть элементом этого же массива
        if (size_ == capacity_)
            Grow(size_ + 1);
        for (std::size_t i = size_; i > index; --i)
            data_[i] = std::move(data_[i - 1]);
        data_[index] = std::move(moved);
        ++size_;
    }

    // Удалить элемент, остальные сдвигаются влево. O(n)
    void RemoveAt(std::size_t index) {
        CheckIndex(index);
        for (std::size_t i = index; i + 1 < size_; ++i)
            data_[i] = std::move(data_[i + 1]);
        --size_;
        data_[size_] = T(); // освобождаем ресурсы удалённого элемента (важно для UnqPtr, строк и т.п.)
    }

    // Удалить последний элемент, O(1)
    void RemoveLast() {
        CheckNotEmpty();
        --size_;
        data_[size_] = T();
    }

    // Удалить все элементы. Ёмкость сохраняется - повторное заполнение не будет выделять память
    void Clear() { Resize(0); }

    // Изменить количество элементов. Новые элементы равны T(), лишние удаляются
    void Resize(std::size_t new_size) {
        if (new_size > capacity_)
            Grow(new_size);
        if (new_size > size_) {
            for (std::size_t i = size_; i < new_size; ++i)
                data_[i] = T();
        } else if constexpr (!std::is_trivially_destructible_v<T>) {
            // Для int/double это не нужно, а у строк, UnqPtr и т.п. освобождаем ресурсы
            for (std::size_t i = new_size; i < size_; ++i)
                data_[i] = T();
        }
        size_ = new_size;
    }

    // Заранее выделить память под new_capacity элементов (количество элементов не меняется)
    void Reserve(std::size_t new_capacity) {
        if (new_capacity <= capacity_)
            return;
        UnqPtr<T[]> new_data(new T[new_capacity]);
        for (std::size_t i = 0; i < size_; ++i)
            new_data[i] = std::move(data_[i]); // если бросит - new_data удалит себя сам
        data_ = std::move(new_data);           // старый буфер удалит UnqPtr
        capacity_ = new_capacity;
    }

    void Swap(DynamicArray<T>& other) noexcept {
        data_.Swap(other.data_);
        std::swap(size_, other.size_);
        std::swap(capacity_, other.capacity_);
    }

    // ---------- Обход ----------
    // Обычные указатели - самый быстрый обход:
    //     for (int& x : array) x *= 2;
    T* begin() { return data_.Get(); }
    T* end() { return data_.Get() + size_; }
    const T* begin() const { return data_.Get(); }
    const T* end() const { return data_.Get() + size_; }

    IEnumerator<T>* GetEnumerator() const override { return new ArrayEnumerator(this); }

private:
    void CheckIndex(std::size_t index) const {
        if (index >= size_)
            throw IndexOutOfRange("DynamicArray: index out of range");
    }

    void CheckNotEmpty() const {
        if (size_ == 0)
            throw EmptyCollection("DynamicArray: array is empty");
    }

    // Только для конструкторов: выделить буфер, когда массив ещё пуст
    void Allocate(std::size_t count) {
        if (count > 0) {
            data_.Reset(new T[count]);
            capacity_ = count;
        }
    }

    // Только для конструкторов: стать копией count элементов items
    void CopyFrom(const T* items, std::size_t count) {
        Allocate(count);
        for (std::size_t i = 0; i < count; ++i)
            data_[i] = items[i]; // если бросит - буфер удалит деструктор поля data_
        size_ = count;
    }

    // Увеличить ёмкость хотя бы до min_capacity: вдвое (или до 4, если массив пуст)
    void Grow(std::size_t min_capacity) {
        std::size_t new_capacity = (capacity_ == 0) ? 4 : capacity_ * 2;
        if (new_capacity < min_capacity)
            new_capacity = min_capacity;
        Reserve(new_capacity);
    }

    UnqPtr<T[]> data_;
    std::size_t size_ = 0;
    std::size_t capacity_ = 0;
};
