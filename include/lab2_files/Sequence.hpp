#pragma once

#include <functional>
#include <stdexcept>
#include <utility>
#include <algorithm>
#include "DynamicArray.hpp"
#include "ICollection.hpp"
#include "IEnumerable.hpp"

template <class T> class MutableArraySequence;

template <class T>
class Sequence : public ICollection<T>, public IEnumerable<T> {
public:
    ~Sequence() override = default;

    // ICollection
    T& Get( std::size_t index )             override = 0;
    const T& Get( std::size_t index ) const override = 0;
    std::size_t GetCount() const override {
        return static_cast<std::size_t>( GetLength() );
    }

    // IEnumerable
    virtual IEnumerator<T>* GetEnumerator() const override = 0;

    // Декомпозиция
    virtual T&           GetFirst()                           const = 0;
    virtual T&           GetLast()                            const = 0;
    virtual int          GetLength()                          const = 0;
    virtual Sequence<T>* GetSubsequence( int start, int end ) const = 0;

    const T& operator[]( int index ) const {
        if ( index < 0 || static_cast<size_t>( index ) >= static_cast<size_t>( GetLength() ) )
            throw IndexOutOfRange( "Sequence: index out of range" );
        return Get( static_cast<size_t>( index ) );
    }

    T& operator[]( int index ) {
        if ( index < 0 || static_cast<size_t>( index ) >= static_cast<size_t>( GetLength() ) )
            throw IndexOutOfRange( "Sequence: index out of range" );
        return Get( static_cast<size_t>( index ) );
    }

    // Операции
    virtual Sequence<T>* Append( const T& item )                    = 0;
    virtual Sequence<T>* Prepend( const T& item )                   = 0;
    virtual Sequence<T>* InsertAt( const T& item, int index )       = 0;
    virtual Sequence<T>* Concat( Sequence<T>* other)          const = 0;

    virtual Sequence<T>* Clone() const = 0;

    // Фабричный метод
    virtual Sequence<T>* CreateEmpty() const = 0;

    // ------------------------------------------------------------------
    // Map / Where / Reduce (Оптимизированы через константные итераторы)
    // ------------------------------------------------------------------

    Sequence<T>* MapIndexed( std::function<T( const T&, int )> func ) const {
        Sequence<T>* result = CreateEmpty();
        IEnumerator<T>* it = this->GetEnumerator(); // Без костылей!
        int idx = 0;

        while ( it->MoveNext() ) {
            Sequence<T>* next = result->Append( func( it->Current(), idx++ ) );
            if ( next != result ) delete result;
            result = next;
        }
        delete it;
        return result;
    }

    Sequence<T>* Map( std::function<T( const T& )> func ) const {
        return MapIndexed( [func]( const T& val, int /*idx*/ ) {
            return func( val );
        });
    }

    template <typename T2>
    Sequence<T2>* MapIndexedTo( std::function<T2( const T&, int )> func, const Sequence<T2>* prototype ) const {
        Sequence<T2>* result = prototype->CreateEmpty();
        IEnumerator<T>* it = this->GetEnumerator();
        int idx = 0;

        while ( it->MoveNext() ) {
            Sequence<T2>* next = result->Append( func( it->Current(), idx++ ) );
            if ( next != result ) delete result;
            result = next;
        }
        delete it;
        return result;
    }

    Sequence<T>* Where( std::function<bool( const T& )> predicate ) const {
        Sequence<T>* result = CreateEmpty();
        IEnumerator<T>* it = this->GetEnumerator();

        while ( it->MoveNext() ) {
            if ( predicate( it->Current() ) ) {
                Sequence<T>* next = result->Append( it->Current() );
                if ( next != result ) delete result;
                result = next;
            }
        }
        delete it;
        return result;
    }

    template <class T2>
    T2 Reduce( std::function<T2( T2, const T& )> func, T2 initial ) const {
        T2 result = initial;
        IEnumerator<T>* it = this->GetEnumerator();

        while ( it->MoveNext() ) {
            result = func( result, it->Current() );
        }
        delete it;
        return result;
    }

    Sequence<T>* ZipWithReversed() const {
        int n = GetLength();
        return MapIndexed( [this, n]( const T& val, int idx ) {
            return val + Get( n - idx - 1 );
        });
    }

    Sequence<T>* Skip( int count ) const {
        if ( count < 0 )
            throw std::invalid_argument( "Sequence: Skip count < 0" );
        if ( count >= GetLength() )
            return this->CreateEmpty();
        return GetSubsequence( count, GetLength() - 1 );
    }

    Sequence<T>* Take( int count ) const {
        if ( count < 0 )
            throw std::invalid_argument( "Sequence: Take count < 0" );
        if ( count == 0 )
            return this->CreateEmpty();
        int actual = std::min( count, GetLength() );
        return GetSubsequence( 0, actual - 1 );
    }

    template <typename T2>
    Sequence<T2>* FlatMap( std::function<Sequence<T2>*( const T& )> func, const Sequence<T2>* prototype ) const {
        Sequence<T2>* result = prototype->CreateEmpty();
        IEnumerator<T>* it = this->GetEnumerator();

        try {
            while ( it->MoveNext() ) {
                Sequence<T2>* part = func( it->Current() );
                try {
                    IEnumerator<T2>* partIt = part->GetEnumerator();
                    while ( partIt->MoveNext() ) {
                        Sequence<T2>* next = result->Append( partIt->Current() );
                        if ( next != result ) delete result;
                        result = next;
                    }
                    delete partIt;
                } catch ( ... ) {
                    delete part;
                    throw;
                }
                delete part;
            }
        } catch ( ... ) {
            delete result;
            throw;
        }
        delete it;
        return result;
    }

    template <typename T2>
    Sequence<std::pair<T, T2>>* Zip( const Sequence<T2>* other, const Sequence<std::pair<T, T2>>* prototype ) const {
        if ( other == nullptr )
            throw std::invalid_argument( "Sequence: Zip other is empty(nullptr)" );

        Sequence<std::pair<T, T2>>* result = prototype->CreateEmpty();

        IEnumerator<T>* it1 = this->GetEnumerator();
        IEnumerator<T2>* it2 = other->GetEnumerator();

        while ( it1->MoveNext() && it2->MoveNext() ) {
            Sequence<std::pair<T, T2>>* next = result->Append( { it1->Current(), it2->Current() } );
            if ( next != result ) delete result;
            result = next;
        }

        delete it1;
        delete it2;
        return result;
    }

    // П-1: (min, max, avg) за один проход через Reduce
    struct MinMaxAvg {
        T min;
        T max;
        double sum;
        int count;
    };

    MinMaxAvg GetMinMaxAvg() const {
        if ( GetLength() == 0 ) {
            throw IndexOutOfRange( "Sequence: GetMinMaxAvg called on empty sequence" );
        }

        T first = Get( 0 );
        MinMaxAvg initial = { first, first, 0.0, 0 };

        return Reduce<MinMaxAvg>(
            std::function<MinMaxAvg(MinMaxAvg, const T&)>(
                []( MinMaxAvg result, const T& value ) -> MinMaxAvg {
                    if ( value < result.min ) { result.min = value; }
                    if ( value > result.max ) { result.max = value; }
                    result.sum += static_cast<double>( value );
                    ++result.count;
                    return result;
                }
            ),
            initial
        );
    }

    double GetAvg() const {
        auto r = GetMinMaxAvg();
        return r.sum / r.count;
    }

    T GetMin() const { return GetMinMaxAvg().min; }
    T GetMax() const { return GetMinMaxAvg().max; }
};
