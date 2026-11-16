#pragma once

#include <stdexcept>
#include <string>

// Индекс вне допустимого диапазона (Get, Set, InsertAt, RemoveAt, ...)
class IndexOutOfRange : public std::out_of_range {
public:
    explicit IndexOutOfRange(const char* message) : std::out_of_range(message) {}
    explicit IndexOutOfRange(const std::string& message) : std::out_of_range(message) {}
};

// Операция требует хотя бы один элемент (GetFirst, GetLast, RemoveFirst, ...).
// Наследуется от IndexOutOfRange, поэтому catch (IndexOutOfRange&) ловит и её.
class EmptyCollection : public IndexOutOfRange {
public:
    explicit EmptyCollection(const char* message) : IndexOutOfRange(message) {}
    explicit EmptyCollection(const std::string& message) : IndexOutOfRange(message) {}
};
