cmake_minimum_required(VERSION 3.20)
project(SmartPointersHW VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Опция для включения AddressSanitizer (аналог Valgrind на macOS)
option(ENABLE_ASAN "Enable AddressSanitizer" OFF)

if (ENABLE_ASAN)
    message(STATUS "AddressSanitizer enabled (-fsanitize=address)")
    add_compile_options(-fsanitize=address -g -O1 -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address)
endif()

# Google тесты
include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)
# Для избежания переопределения настроек компилятора
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()

# Подключение папки с заголовочными файлами
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

add_executable(smart_pointers_app 
    src/main.cpp
    tests/tests_all.cpp
)

target_link_libraries(smart_pointers_app PRIVATE GTest::gtest)
