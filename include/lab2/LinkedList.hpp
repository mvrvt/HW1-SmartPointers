#pragma once

#include <cstddef>   // std::size_t
#include <stdexcept> // std::invalid_argument
#include <utility>   // std::move, std::swap

#include "Exceptions.hpp"
#include "ICollection.hpp"
#include "IEnumerable.hpp"

// Узел односвязного списка. Вынесен из LinkedList, чтобы класс начинался с public:
// итераторы ниже используют тип узла, а он должен быть объявлен раньше них.
template <class T>
struct LinkedListNode {
    T                  data;
    LinkedListNode<T>* next = nullptr;

    explicit LinkedListNode(const T& value) : data(value) {}
    explicit LinkedListNode(T&& value) : data(std::move(value)) {}
};

// ========== LinkedList<T> - односвязный список (замена std::list / std::forward_list) ==========
// Хранит указатели на голову и хвост, поэтому Append, Prepend, RemoveFirst, GetFirst
// и GetLast работают за O(1). Доступ по индексу - O(n): для обхода всех элементов
// используйте цикл for (auto& x : list), а не Get(i) в цикле (это было бы O(n^2)).
template <class T>
class LinkedList : public ICollection<T>, public IEnumerable<T> {
public:
    using Node = LinkedListNode<T>;

    // ---------- Итераторы для цикла for ----------
    class Iterator {
    public:
        explicit Iterator(Node* node) : node_(node) {}

        T& operator*() const { return node_->data; }
        T* operator->() const { return &node_->data; }

        Iterator& operator++() {
            node_ = node_->next;
            return *this;
        }

        bool operator==(const Iterator& other) const { return node_ == other.node_; }
        bool operator!=(const Iterator& other) const { return node_ != other.node_; }

    private:
        Node* node_;
    };

    class ConstIterator {
    public:
        explicit ConstIterator(const Node* node) : node_(node) {}

        const T& operator*() const { return node_->data; }
        const T* operator->() const { return &node_->data; }

        ConstIterator& operator++() {
            node_ = node_->next;
            return *this;
        }

        bool operator==(const ConstIterator& other) const { return node_ == other.node_; }
        bool operator!=(const ConstIterator& other) const { return node_ != other.node_; }

    private:
        const Node* node_;
    };

    // ---------- Итератор для IEnumerable (идёт по указателям, весь обход - O(n)) ----------
    class ListEnumerator : public IEnumerator<T> {
    public:
        explicit ListEnumerator(const Node* head) : head_(head) {}

        bool MoveNext() override {
            if (!started_) {
                started_ = true;
                current_ = head_;
            } else if (current_ != nullptr) {
                current_ = current_->next;
            }
            return current_ != nullptr;
        }

        const T& Current() const override {
            if (current_ == nullptr)
                throw IndexOutOfRange("ListEnumerator: no current element");
            return current_->data;
        }

        void Reset() override {
            started_ = false;
            current_ = nullptr;
        }

    private:
        const Node* head_;
        const Node* current_ = nullptr;
        bool started_ = false; // отличает "ещё не начали" от "уже дошли до конца"
    };

    // ---------- Конструкторы, копирование, перемещение ----------
    LinkedList() = default;

    // Копия count элементов обычного массива items
    LinkedList(const T* items, std::size_t count) {
        if (items == nullptr && count > 0)
            throw std::invalid_argument("LinkedList: items is nullptr");
        try {
            for (std::size_t i = 0; i < count; ++i)
                Append(items[i]);
        } catch (...) {
            Clear(); // деструктор не вызовется, если бросил конструктор
            throw;
        }
    }

    LinkedList(const LinkedList<T>& other) {
        try {
            for (const Node* node = other.head_; node != nullptr; node = node->next)
                Append(node->data);
        } catch (...) {
            Clear();
            throw;
        }
    }

    LinkedList(LinkedList<T>&& other) noexcept
        : head_(other.head_), tail_(other.tail_), size_(other.size_) {
        other.head_ = nullptr;
        other.tail_ = nullptr;
        other.size_ = 0;
    }

    // Copy-and-swap: если копирование бросит исключение, *this не изменится
    LinkedList<T>& operator=(const LinkedList<T>& other) {
        if (this != &other) {
            LinkedList<T> copy(other);
            Swap(copy);
        }
        return *this;
    }

    LinkedList<T>& operator=(LinkedList<T>&& other) noexcept {
        if (this != &other) {
            Clear();
            Swap(other);
        }
        return *this;
    }

    ~LinkedList() override { Clear(); }

    // ---------- Информация ----------
    std::size_t GetCount() const override { return size_; }
    std::size_t GetLength() const { return size_; } // то же, что GetCount (название из задания)

    // ---------- Доступ ----------
    const T& Get(std::size_t index) const override { return GetNode(index)->data; } // O(n)
    T& Get(std::size_t index) { return GetNode(index)->data; }                      // O(n)

    const T& operator[](std::size_t index) const { return Get(index); }
    T& operator[](std::size_t index) { return Get(index); }

    const T& GetFirst() const {
        CheckNotEmpty();
        return head_->data;
    }

    T& GetFirst() {
        CheckNotEmpty();
        return head_->data;
    }

    const T& GetLast() const {
        CheckNotEmpty();
        return tail_->data;
    }

    T& GetLast() {
        CheckNotEmpty();
        return tail_->data;
    }

    void Set(std::size_t index, const T& value) { GetNode(index)->data = value; }

    // Новый список из элементов с индексами start_index..end_index включительно
    LinkedList<T> GetSubList(std::size_t start_index, std::size_t end_index) const {
        if (end_index >= size_)
            throw IndexOutOfRange("LinkedList::GetSubList: end_index out of range");
        if (start_index > end_index)
            throw std::invalid_argument("LinkedList::GetSubList: start_index > end_index");

        LinkedList<T> result;
        const Node* node = GetNode(start_index);
        for (std::size_t i = start_index; i <= end_index; ++i) {
            result.Append(node->data);
            node = node->next;
        }
        return result;
    }

    // Новый список: сначала элементы this, затем элементы other
    LinkedList<T> Concat(const LinkedList<T>& other) const {
        LinkedList<T> result(*this);
        for (const Node* node = other.head_; node != nullptr; node = node->next)
            result.Append(node->data);
        return result;
    }

    // ---------- Изменение ----------
    void Append(const T& value) { LinkAtEnd(new Node(value)); } // O(1)
    void Append(T&& value) { LinkAtEnd(new Node(std::move(value))); }

    void Prepend(const T& value) { LinkAtStart(new Node(value)); } // O(1)
    void Prepend(T&& value) { LinkAtStart(new Node(std::move(value))); }

    // Вставить так, чтобы новый элемент получил индекс index; index == GetCount() - вставка в конец. O(n)
    void InsertAt(const T& value, std::size_t index) {
        if (index > size_)
            throw IndexOutOfRange("LinkedList::InsertAt: index out of range");
        if (index == 0) {
            Prepend(value);
            return;
        }
        if (index == size_) {
            Append(value);
            return;
        }
        Node* prev = GetNode(index - 1);
        Node* node = new Node(value);
        node->next = prev->next;
        prev->next = node;
        ++size_;
    }

    // O(1)
    void RemoveFirst() {
        CheckNotEmpty();
        Node* old = head_;
        head_ = head_->next;
        if (head_ == nullptr)
            tail_ = nullptr;
        delete old;
        --size_;
    }

    // O(n): список односвязный, предыдущий перед хвостом приходится искать с головы
    void RemoveLast() {
        CheckNotEmpty();
        RemoveAt(size_ - 1);
    }

    // O(n)
    void RemoveAt(std::size_t index) {
        CheckIndex(index);
        if (index == 0) {
            RemoveFirst();
            return;
        }
        Node* prev = GetNode(index - 1);
        Node* old = prev->next;
        prev->next = old->next;
        if (old == tail_)
            tail_ = prev;
        delete old;
        --size_;
    }

    // Удалить все элементы
    void Clear() {
        Node* node = head_;
        while (node != nullptr) {
            Node* next = node->next;
            delete node;
            node = next;
        }
        head_ = nullptr;
        tail_ = nullptr;
        size_ = 0;
    }

    void Swap(LinkedList<T>& other) noexcept {
        std::swap(head_, other.head_);
        std::swap(tail_, other.tail_);
        std::swap(size_, other.size_);
    }

    // ---------- Обход ----------
    //     for (int& x : list) x *= 2;
    Iterator begin() { return Iterator(head_); }
    Iterator end() { return Iterator(nullptr); }
    ConstIterator begin() const { return ConstIterator(head_); }
    ConstIterator end() const { return ConstIterator(nullptr); }

    IEnumerator<T>* GetEnumerator() const override { return new ListEnumerator(head_); }

private:
    void CheckIndex(std::size_t index) const {
        if (index >= size_)
            throw IndexOutOfRange("LinkedList: index out of range");
    }

    void CheckNotEmpty() const {
        if (size_ == 0)
            throw EmptyCollection("LinkedList: list is empty");
    }

    // Узел по индексу, O(n)
    Node* GetNode(std::size_t index) const {
        CheckIndex(index);
        Node* node = head_;
        for (std::size_t i = 0; i < index; ++i)
            node = node->next;
        return node;
    }

    void LinkAtEnd(Node* node) {
        if (tail_ == nullptr)
            head_ = node;
        else
            tail_->next = node;
        tail_ = node;
        ++size_;
    }

    void LinkAtStart(Node* node) {
        node->next = head_;
        head_ = node;
        if (tail_ == nullptr)
            tail_ = node;
        ++size_;
    }

    Node* head_ = nullptr;
    Node* tail_ = nullptr;
    std::size_t size_ = 0;
};
