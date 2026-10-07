cmake_minimum_required(VERSION 3.28)
project(projekt LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)   # für clang-tidy und Editoren

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

add_compile_options(-Wall -Wextra -Wpedantic)

option(ENABLE_SANITIZERS "AddressSanitizer und UBSan" OFF)
if(ENABLE_SANITIZERS)
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer -g)
    add_link_options(-fsanitize=address,undefined)
endif()

# Programmlogik als Bibliothek: Programm und Tests nutzen denselben Code.
# Neue .cpp-Dateien hier eintragen.
add_library(projekt_lib
    src/statistik.cpp
)
target_include_directories(projekt_lib PUBLIC src)

add_executable(projekt src/main.cpp)
target_link_libraries(projekt PRIVATE projekt_lib)

# Tests
enable_testing()
find_package(GTest REQUIRED)
include(GoogleTest)

# Neue Testdateien hier eintragen.
add_executable(tests
    tests/test_statistik.cpp
)
target_link_libraries(tests PRIVATE projekt_lib GTest::gtest_main)
gtest_discover_tests(tests)             # jeder TEST() wird ein CTest-Test
