#pragma once

#include <cstddef>
#include "Sequence.hpp"
#include "DynamicArray.hpp"
#include "SequenceIterator.hpp"

template <class T>
class ArraySequence : public Sequence<T> {
public:
    // Конструкторы
    ArraySequence() : items_( new DynamicArray<T>(0) ) {}

    ArraySequence( T* data, int count ) : items_( new DynamicArray<T>( data, count ) ) {}

    ArraySequence( const ArraySequence<T>& other ) : items_( new DynamicArray<T>( *other.items_ ) ) {}

    ~ArraySequence() override {
        delete items_;
    }

    // IEnumerator
    IEnumerator<T>* GetEnumerator() const override {
        return new SequenceIterator<T>( const_cast<ArraySequence<T>*>( this ) );
    }

    // Декомпозиция
    T& GetFirst() const override {
        if ( items_->GetCount() == 0 ) {
            throw IndexOutOfRange( "ArraySequence: sequence is empty" );
        }
        return items_->Get( 0 );
    }

    T& GetLast() const override {
        int len = items_->GetCount();
        if ( len == 0 ) {
            throw IndexOutOfRange( "ArraySequence: sequence is empty" );
        }
        return items_->Get( len - 1 );
    }

    T& Get( std::size_t index ) override {
        if ( static_cast<int>( index ) >= items_->GetCount() ) {
            throw IndexOutOfRange( "ArraySequence: index out of range" );
        }
        return items_->Get( index );
    }

    const T& Get( std::size_t index ) const override {
        if ( static_cast<int>( index ) >= items_->GetCount() ) {
            throw IndexOutOfRange( "ArraySequence: index out of range" );
        }
        return items_->Get( index );
    }

    int GetLength() const override {
        return items_->GetCount();
    }

    // Операции с последовательностью
    Sequence<T>* GetSubsequence( int start, int end ) const override {
        int len = items_->GetCount();
        if ( start < 0 || start >= len ) {
            throw IndexOutOfRange( "ArraySequence: start index out of range" );
        }
        if ( end < 0 || end >= len ) {
            throw IndexOutOfRange( "ArraySequence: end index out of range" );
        }
        if ( start > end ) {
            throw std::invalid_argument( "ArraySequence: start > end" );
        }

        Sequence<T>* result = this->CreateEmpty();
        for ( int idx = start; idx <= end; ++idx ) {
            Sequence<T>* next = result->Append( items_->Get( idx ) );
            if ( next != result ) delete result; // для Immutable - удаляем старую копию
            result = next;
        }
        return result;
    }

    Sequence<T>* Append( const T& item ) override {
        return Instance()->AppendInternal( item );
    }

    Sequence<T>* Prepend( const T& item ) override {
        return Instance()->PrependInternal( item );
    }

    Sequence<T>* InsertAt( const T& item, int index ) override {
        return Instance()->InsertAtInternal( item, index );
    }

    Sequence<T>* Concat( Sequence<T>* other ) const override {
        if ( other == nullptr ) {
            throw std::invalid_argument( "ArraySequence: other is nullptr" );
        }

        Sequence<T>* result = this->CreateEmpty();

        // Копируем все элементы текущей последовательности
        for ( int idx = 0; idx < items_->GetCount(); ++idx ) {
            Sequence<T>* next = result->Append( items_->Get( idx ) );
            if ( next != result ) delete result;
            result = next;
        }

        // Копируем все элементы другой последовательности
        for ( int idx = 0; idx < other->GetLength(); ++idx ) {
            Sequence<T>* next = result->Append( other->Get( idx ) );
            if ( next != result ) delete result;
            result = next;
        }
        return result;
    }

    // Вспомогательное для operator[]
    const T& operator[]( int index ) const {
        if ( index < 0 || index >= items_->GetCount() )
            throw IndexOutOfRange("ArraySequence: index out of range");
        return items_->Get( index );
    }

    T& operator[]( int index ) {
        if ( index < 0 || index >= items_->GetCount() )
            throw IndexOutOfRange("ArraySequence: index out of range");
        return items_->Get( index );
    }

protected:
    DynamicArray<T>* items_;

    virtual ArraySequence<T>* Instance() = 0;

    // Внутренние методы, изменяющие текущий объект (используются для mutable и immutable)
    ArraySequence<T>* AppendInternal( const T& item ) {
        int oldSize = items_->GetCount();
        items_->Resize( oldSize + 1 );
        items_->Set( oldSize, item );
        return this;
    }

    ArraySequence<T>* PrependInternal( const T& item ) {
        int oldSize = items_->GetCount();
        items_->Resize( oldSize + 1 );
        // Сдвигаем все существующие элементы вправо
        for ( int idx = oldSize; idx > 0; --idx ) {
            items_->Set( idx, items_->Get( idx - 1 ) );
        }
        items_->Set( 0, item );
        return this;
    }

    ArraySequence<T>* InsertAtInternal( const T& item, int index ) {
        int oldSize = items_->GetCount();
        if ( index < 0 || index > oldSize ) {
            throw IndexOutOfRange( "ArraySequence: index out of range" );
        }
        items_->Resize( oldSize + 1 );
        // Сдвигаем элементы, начиная с конца, чтобы освободить место
        for ( int idx = oldSize; idx > index; --idx ) {
            items_->Set( idx, items_->Get( idx - 1 ) );
        }
        items_->Set( index, item );
        return this;
    }

};

// -------------------------------------------------------------------
// MutableArraySequence
template <class T>
class MutableArraySequence : public ArraySequence<T> {
public:
    MutableArraySequence() : ArraySequence<T>() { }
    MutableArraySequence( T* data, int count ) : ArraySequence<T>( data, count ) { }
    MutableArraySequence( const ArraySequence<T>& other ) : ArraySequence<T>( other ) { }

    Sequence<T>* Clone() const override {
        return new MutableArraySequence<T>(*this);
    }

    Sequence<T>* CreateEmpty() const override {
        return new MutableArraySequence<T>();
    }

protected:
    ArraySequence<T>* Instance() override {
        return this; // изменяем текущий объект
    }

};

// -------------------------------------------------------------------
// ImmutableArraySequence
template <class T>
class ImmutableArraySequence : public ArraySequence<T> {
public:
    ImmutableArraySequence() : ArraySequence<T>() { }
    ImmutableArraySequence( T* data, int count ) : ArraySequence<T>( data, count ) { }
    ImmutableArraySequence( const ArraySequence<T>& other ) : ArraySequence<T>( other ) { }

    Sequence<T>* Clone() const override {
        return new ImmutableArraySequence<T>(*this);
    }

    Sequence<T>* CreateEmpty() const override {
        return new ImmutableArraySequence<T>();
    }

protected:
    ArraySequence<T>* Instance() override {
        return new ImmutableArraySequence<T>( *this ); // создаём копию
    }

};
