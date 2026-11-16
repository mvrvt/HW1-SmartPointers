#pragma once

#include <cstddef> // std::size_t

// М-1: общий интерфейс всех коллекций - Get() и GetCount()
template <class T>
class ICollection {
public:
    virtual ~ICollection() = default;

    virtual const T&    Get(std::size_t index) const = 0; // бросает IndexOutOfRange
    virtual std::size_t GetCount()             const = 0;

    bool IsEmpty() const { return GetCount() == 0; }
};
