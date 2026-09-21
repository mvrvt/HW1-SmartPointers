#pragma once

#include "UnqPtr.hpp"
#include <cstddef>
#include <utility>

template <typename T>
class ShrdPtr {
 public:
    ShrdPtr() noexcept = default;

    // Создание из UnqPtr (забирает владение объектом)
    explicit ShrdPtr(UnqPtr<T>&& unique_ptr) {
        if (unique_ptr) {
            master_ = new UnqPtr<T>(std::move(unique_ptr));
            ref_count_ = new size_t(1);
        }
    }

    ~ShrdPtr() noexcept {
        ReleaseRef();
    }

    // Copy semantics
    ShrdPtr(const ShrdPtr& other) noexcept : master_(other.master_), ref_count_(other.ref_count_) {
        if (ref_count_ != nullptr)
            ++(*ref_count_);
    }

    ShrdPtr operator=(const ShrdPtr& other) noexcept {
        if (this != &other) {
            ReleaseRef();
            master_ = other.master_;
            ref_count_ = other.ref_count_;
            if (ref_count_ != nullptr) 
                ++(*ref_count_);
        }
        return *this; 
    }

    // Move semantics (забираем владение без изменения счётчика)
    ShrdPtr(ShrdPtr&& other) noexcept : master_(other.master_), ref_count_(other.ref_count_) {
        other.master_ = nullptr;
        other.ref_count_ = nullptr; 
    }

    ShrdPtr& operator=(ShrdPtr&& other) noexcept {
        if (this != &other) {
            ReleaseRef();
            master_ = other.master_;
            ref_count_ = other.ref_count_;
            other.master_ = nullptr;
            other.ref_count_ = nullptr;
        }
        return *this; 
    }

    // Доступ к объекту
    T& operator*() const noexcept { return **master_; }
    T* operator->() const noexcept { return master_->Get(); }
    T* Get() const noexcept { return master_ != nullptr ? master_->Get() : nullptr; }
    explicit operator bool() const noexcept { return Get() != nullptr; }

    // Вспомогательные методы 
    size_t UseCount() const noexcept {
        return ref_count_ != nullptr ? *ref_count_ : 0;
    }

    void Reset() noexcept {
        ReleaseRef();
    }

 private:
    void ReleaseRef() noexcept {
        if (ref_count_ != nullptr) {
            --(*ref_count_);
            if (*ref_count_ == 0) {
                delete master_;
                delete ref_count_;
            }
            master_    = nullptr;
            ref_count_ = nullptr;
        }
    }

    UnqPtr<T>* master_    = nullptr;
    size_t*    ref_count_ = nullptr;
};
