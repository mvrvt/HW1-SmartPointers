#pragma once

template <class T>
class IEnumerator {
public:
    virtual bool       MoveNext()       = 0; // перейти к следующему, вернуть false если конец
    virtual const T&   Current() const  = 0; // вернуть текущий элемент
    virtual void       Reset()          = 0; // сброс в начало (перед первым элементом)

    virtual ~IEnumerator() = default;
};
