#pragma once

#include "IEnumerator.hpp"
#include "Sequence.hpp"

template <class T>
class SequenceIterator : public IEnumerator<T> {
public:
    explicit SequenceIterator( Sequence<T>* seq ) : seq_( seq ), index_( -1 ) { }

    bool MoveNext() override {
        ++index_;
        return index_ < seq_->GetLength();
    }

    const T& Current() const override {
        if ( index_ < 0 || index_ >= seq_->GetLength() )
            throw IndexOutOfRange( "SequenceIterator: index out of range" );
        return seq_->Get( index_ );
    }

    void Reset() override {
        index_ = -1;
    }

private:
    Sequence<T>* seq_;    // указатель на любую последовательность
    int          index_;  // -1 = до первого элемента
};
