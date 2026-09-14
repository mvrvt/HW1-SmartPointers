#pragma once

#include <stdexcept> // для ошибки out_of_range
#include <cstddef>   // для типа элементов size_t
#include <string>
#include "ICollection.hpp"
#include "IEnumerable.hpp"

class IndexOutOfRange : public std::out_of_range {
public:
    explicit IndexOutOfRange( const std::string& message )
        : std::out_of_range( message ) { }
};

template <class T>
class DynamicArray : public ICollection<T>, public IEnumerable<T> {
public:
    // Встроенный класс-итератор
    class ArrayIterator : public IEnumerator<T> {
    private:
        const DynamicArray<T>& arr_; // Теперь const
        int index_;

    public:
        explicit ArrayIterator( const DynamicArray<T>& arr ) : arr_( arr ), index_( -1 ) { }

        bool MoveNext() override {
            ++index_;
            return index_ < static_cast<int>( arr_.GetCount() );
        }

        const T& Current() const override {
            if ( index_ < 0 || index_ >= static_cast<int>( arr_.GetCount() ) )
                throw IndexOutOfRange( "ArrayIterator: index out of range" );
            return arr_.Get( static_cast<std::size_t>( index_ ) );
        }

        void Reset() override {
            index_ = -1;
        }
    };

    // Метод стал const
    IEnumerator<T>* GetEnumerator() const override {
        return new ArrayIterator( *this );
    }

    // Конструкторы

    DynamicArray( T* items, int count ) : data_( nullptr ), size_( count ) {
        if ( count < 0 ) {
            throw std::invalid_argument( "DynamicArray: count can't be negative" );
        }
        if ( count > 0 && items == nullptr ) {
            throw std::invalid_argument( "DynamicArray: items is nullptr" );
        }

        capacity_ = ( count == 0 ) ? 1 : count;
        data_ = new T[capacity_];
        for ( int idx = 0; idx < size_; ++idx ) {
            data_[idx] = items[idx];
        }
    }

    explicit DynamicArray( int size ) : data_( nullptr ), size_( size ) {
        if ( size < 0 ) {
            throw std::invalid_argument( "DynamicArray: size can't be negative" );
        }

        capacity_ = ( size == 0 ) ? 1 : size;
        data_ = new T[capacity_]();
        // новые элементы уже инициализированы нулями
    }

    DynamicArray( const DynamicArray<T>& other )
        : data_( nullptr ), size_( other.size_ ), capacity_( other.capacity_ ) {
        if ( capacity_ > 0 ) {
            data_ = new T[capacity_];
            for ( int idx = 0; idx < size_; ++idx ) {
                data_[idx] = other.data_[idx];
            }
        }
    }

    DynamicArray<T>& operator=( const DynamicArray<T>& other ) {
        if ( this != &other ) {
            T* new_data = nullptr;
            int new_capacity = other.capacity_;

            if ( new_capacity > 0 ) {
                new_data = new T[new_capacity];
                for ( int idx = 0; idx < other.size_; ++idx ) {
                    new_data[idx] = other.data_[idx];
                }
            }
            delete[] data_;
            data_ = new_data;
            size_ = other.size_;
            capacity_ = new_capacity;
        }
        return *this;
    }

    ~DynamicArray() override {
        delete[] data_;
    }

    // Доступ

    T& Get( std::size_t index ) override {
        CheckIndex( static_cast<int>( index ) );
        return data_[index];
    }

    const T& Get( std::size_t index ) const override {
        CheckIndex( static_cast<int>( index ) );
        return data_[index];
    }

    std::size_t GetCount() const override {
        return static_cast<std::size_t>( size_ );
    }

    void Set( int index, const T& value ) {
        CheckIndex( index );
        data_[index] = value;
    }

    // Управление размером

    void Resize( int newSize ) {
        if ( newSize < 0 ) {
            throw std::invalid_argument( "DynamicArray: new_size can't be negative" );
        }

        if ( newSize <= capacity_ ) {
            size_ = newSize;
            return;
        }

        // Не хватает – увеличиваем capacity_ (удваиваем, пока не станет >= newSize)
        int newCapacity = capacity_;
        while ( newCapacity < newSize ) {
            newCapacity *= 2;
        }

        T* newData = new T[newCapacity];
        // Копируем старые элементы (их size_ штук)
        for ( int idx = 0; idx < size_; ++idx ) {
            newData[idx] = data_[idx];
        }
        // Новые элементы (если надо) будут проинициализированы по умолчанию,
        // но мы их всё равно перезапишем позже через Set; можно оставить как есть.

        delete[] data_;
        data_ = newData;
        capacity_ = newCapacity;
        size_ = newSize;
    }

    int GetCapacity() const {
        return capacity_;
    }

    // Операторы индексации для удобства
    T& operator[]( int index ) {
        CheckIndex( index );
        return data_[index];
    }

    const T& operator[]( int index ) const {
        CheckIndex( index );
        return data_[index];
    }

private:
    T*  data_;
    int size_;
    int capacity_;

    void CheckIndex( int index ) const {
        if ( index < 0 || index >= size_ ) {
            throw IndexOutOfRange( "DynamicArray: index out of range" );
        }
    }

};
