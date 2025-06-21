cmake_minimum_required(VERSION 3.5.1)
project(dummy-quoter)

set(CMAKE_CXX_STANDARD 14)

find_package(Threads REQUIRED)

add_executable(dummy-quoter 
    src/main.cpp
    src/quoter.cpp
    src/book_tops_processor.cpp
    src/book_fills_processor.cpp
)

target_include_directories(dummy-quoter PRIVATE src)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    target_link_libraries(dummy-quoter stdc++fs)
endif()