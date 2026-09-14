#pragma once

#include <cstddef> // для size_t
#include "Sequence.hpp"
#include "LinkedList.hpp"
#include "SequenceIterator.hpp"

// forward declaration
template <class T> class MutableListSequence;

template <class T>
class ListSequence : public Sequence<T> {
protected:
    LinkedList<T>* items_;

    virtual ListSequence<T>* Instance() = 0;

    ListSequence<T>* AppendInternal( const T& item ) {
        items_->Append( item );
        return this;
    }

    ListSequence<T>* PrependInternal( const T& item ) {
        items_->Prepend( item );
        return this;
    }

    ListSequence<T>* InsertAtInternal( const T& item, int index ) {
        items_->InsertAt( item, index );
        return this;
    }

public:
    IEnumerator<T>* GetEnumerator() override {
        return new SequenceIterator<T>( this );
    }

    ListSequence() : items_( new LinkedList<T>() ) { }

    ListSequence( T* data, int count ) : items_( new LinkedList<T>( data, count ) ) { }

    ListSequence( const ListSequence<T>& other ) : items_( new LinkedList<T>( *other.items_ ) ) { }

    ~ListSequence() override {
        delete items_;
    }

    T& Get( std::size_t index ) override {
        return items_->Get( index ); // делегируем в LinkedList
    }
    const T& Get( std::size_t index ) const override {
        return items_->Get( index );
    }

    T&   GetFirst() const override { return items_->GetFirst(); }
    T&   GetLast()  const override { return items_->GetLast(); }
    int GetLength() const override {
        return items_->GetLength();
    }

    // IEnumerator (делегируем в LL)
    auto begin() { return items_->begin(); }
    auto end() { return items_->end(); }
    auto begin() const { return items_->begin(); }
    auto end() const { return items_->end(); }

    Sequence<T>* GetSubsequence( int start, int end ) const override {
        if ( start < 0 || start >= items_->GetLength() ) {
            throw IndexOutOfRange( "ListSequence: start index out of range" );
        }
        if ( end < 0 || end >= items_->GetLength() ) {
            throw IndexOutOfRange( "ListSequence: end index out of range" );
        }
        if ( start > end ) {
            throw std::invalid_argument( "ListSequence: start > end" );
        }

        Sequence<T>* result = this->CreateEmpty();
        for ( int idx = start; idx <= end; ++idx ) {
            Sequence<T>* next = result->Append( items_->Get( idx ) );
            if ( next != result ) delete result;
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
            throw std::invalid_argument( "ListSequence: other is empty(nullptr)" );
        }

        // Копируем себя через Append, сохраняя правильный тип
        Sequence<T>* result = this->CreateEmpty();
        for ( int idx = 0; idx < GetLength(); ++idx ) {
            Sequence<T>* next = result->Append( items_->Get( idx ) );
            if ( next != result ) delete result;
            result = next;
        }
        for ( int idx = 0; idx < other->GetLength(); ++idx ) {
            Sequence<T>* next = result->Append( other->Get( idx ) );
            if ( next != result ) delete result;
            result = next;
        }
        return result;
    }
};

// MutableListSequence
template <class T>
class MutableListSequence : public ListSequence<T> {
protected:
    ListSequence<T>* Instance() override { return this; }

public:
    MutableListSequence() : ListSequence<T>() { }

    MutableListSequence( T* data, int count ) : ListSequence<T>( data, count ) { }

    MutableListSequence( const ListSequence<T>& other ) : ListSequence<T>( other ) { }

    Sequence<T>* CreateEmpty() const override {
        return new MutableListSequence<T>();
    }
};
//------------------------------------------------------------------------------------

// ImmutableListSequence
template <class T>
class ImmutableListSequence : public ListSequence<T> {
protected:
    ListSequence<T>* Instance() override {
        return new ImmutableListSequence<T>( *this );
    }

public:
    ImmutableListSequence() : ListSequence<T>() { }

    ImmutableListSequence( T* data, int count ) : ListSequence<T>( data, count ) { }

    ImmutableListSequence( const ListSequence<T>& other ) : ListSequence<T> ( other ) { }

    Sequence<T>* CreateEmpty() const override {
        return new ImmutableListSequence<T>();
    }
};
