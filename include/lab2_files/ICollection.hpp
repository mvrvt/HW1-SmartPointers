#pragma once

#include <cstddef> // для size_t

// М-1: реализация ICollection и его методов Get() и GetCount для каждого типа "коллекции"
template <class T>
class ICollection {
public:
    virtual T&       Get( std::size_t index )       = 0;  // для записи
    virtual const T& Get( std::size_t index ) const = 0;  // для чтения из const
    virtual std::size_t GetCount()            const = 0;
    
    virtual ~ICollection() = default;
};
