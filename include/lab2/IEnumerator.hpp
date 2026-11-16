#pragma once

// Итератор в стиле C#: сначала стоит ПЕРЕД первым элементом,
// каждый MoveNext() сдвигает его на один элемент вперёд.
//
//     while (it->MoveNext())
//         Use(it->Current());
template <class T>
class IEnumerator {
public:
    virtual ~IEnumerator() = default;

    virtual bool     MoveNext()      = 0; // перейти к следующему; false - элементы кончились
    virtual const T& Current() const = 0; // текущий элемент (бросает IndexOutOfRange, если его нет)
    virtual void     Reset()         = 0; // вернуться в начало (перед первым элементом)
};
