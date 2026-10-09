cmake_minimum_required(VERSION 3.20)
project(SmartPointersHW VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Если тип сборки не указан — собираем Debug (отладочная информация, assert'ы включены)
if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

# Санитайзеры (AddressSanitizer + UndefinedBehaviorSanitizer):
#   cmake -S . -B build-asan -DENABLE_ASAN=ON
option(ENABLE_ASAN "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
if (ENABLE_ASAN)
    message(STATUS "Sanitizers enabled: address, undefined")
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
endif()

# ---------- GoogleTest ----------
include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)
FetchContent_MakeAvailable(googletest)

# ---------- Наша библиотека: только заголовки из include/ ----------
# Компилировать в ней нечего, она лишь передаёт настройки (путь к заголовкам
# и флаги предупреждений) всем программам, которые её подключают.
add_library(smart_pointers INTERFACE)
target_include_directories(smart_pointers INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_options(smart_pointers INTERFACE -Wall -Wextra -Wpedantic)

# ---------- Программа с меню ----------
add_executable(smart_pointers_app
    src/main.cpp
)
target_link_libraries(smart_pointers_app PRIVATE smart_pointers)

# ---------- Тесты ----------
enable_testing()

add_executable(unit_tests
    tests/tests_all.cpp
    tests/tests_unq_ptr.cpp
    tests/tests_shrd_ptr.cpp
    tests/tests_ms_ptr.cpp
    tests/tests_memory_span.cpp
)
# gtest_main содержит готовую функцию main(), которая запускает все тесты
target_link_libraries(unit_tests PRIVATE smart_pointers GTest::gtest_main)

add_executable(alloc_failure_tests
    tests/tests_alloc_failure.cpp
)
target_link_libraries(alloc_failure_tests PRIVATE smart_pointers GTest::gtest_main)

# Регистрируем каждый TEST(...) в ctest и во вкладке Testing в VS Code
include(GoogleTest)
gtest_discover_tests(unit_tests)
gtest_discover_tests(alloc_failure_tests)
