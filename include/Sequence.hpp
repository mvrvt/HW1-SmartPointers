#pragma once

#include <cstddef>

template <typename T>
class Sequence {
public:
    virtual ~Sequence() = default;

    virtual size_t GetLength() const noexcept = 0;
    virtual bool IsEmpty() const noexcept = 0;

    virtual const T& Get(size_t index) const = 0;
    virtual T& Get(size_t index) = 0;
    virtual const T& GetFirst() const = 0;
    virtual T& GetFirst() = 0;
    virtual const T& GetLast() const = 0;
    virtual T& GetLast() = 0;

    virtual const T& operator[](size_t index) const = 0;
    virtual T& operator[](size_t index) = 0;

    virtual void Append(const T& item) = 0;
    virtual void Prepend(const T& item) = 0;
    virtual void InsertAt(const T& item, size_t index) = 0;
    virtual void RemoveAt(size_t index) = 0;
};
