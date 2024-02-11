cmake_minimum_required(VERSION 3.5.0)

project(server C)
set(CMAKE_C_STANDARD 99)

find_package(SDL2 REQUIRED)
include_directories(${SDL2_INCLUDE_DIRS})

add_executable(server
    src/main.c
    src/canvas.c
    src/net.c
    src/connection.c
    src/buffer.c
)

target_compile_options(server PRIVATE -Wall -Wextra -g)
target_link_libraries(server ${SDL2_LIBRARIES})