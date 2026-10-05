#pragma once

#include <string>

// ============================================================================
// Вспомогательные типы для тестов
// ============================================================================

// Считает, сколько объектов сейчас живо: ++ в каждом конструкторе, -- в деструкторе.
// Если после теста alive_count == 0, значит, утечек нет и ничего не удалено дважды.
struct InstanceTracker {
    static inline int alive_count = 0;
    int value = 0;

    InstanceTracker() : value(0) {
        ++alive_count;
    }

    explicit InstanceTracker(int val) : value(val) {
        ++alive_count;
    }

    InstanceTracker(const InstanceTracker& other) : value(other.value) {
        ++alive_count;
    }

    InstanceTracker& operator=(const InstanceTracker& other) {
        value = other.value;
        return *this;
    }

    ~InstanceTracker() {
        --alive_count;
    }
};

// Базовый и производный класс для проверки подтипизации
class BaseEntity {
public:
    static inline int alive_entities = 0;

    BaseEntity() {
        ++alive_entities;
    }

    virtual ~BaseEntity() {
        --alive_entities;
    }

    virtual std::string GetName() const {
        return "BaseEntity";
    }
};

class DerivedEntity : public BaseEntity {
public:
    std::string GetName() const override {
        return "DerivedEntity";
    }
};

// Никак не связан с BaseEntity
class UnrelatedEntity {};

// Закрытое наследование: снаружи DerivedEntity* -> BaseEntity* недоступно
class PrivateDerivedEntity : private BaseEntity {};
