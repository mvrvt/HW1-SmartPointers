#pragma once

#include "IEnumerator.hpp"

template <class T>
class IEnumerable {
public:
    virtual IEnumerator<T>* GetEnumerator() const = 0; // создать и вернуть итератор

    virtual ~IEnumerable() = default;
};
