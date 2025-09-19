cmake_minimum_required(VERSION 3.13)

project(http-server-c)

file(GLOB_RECURSE SOURCE_FILES src/*.c src/*.h)

set(CMAKE_C_STANDARD 23) # Use the C23 standard (falls back to compiler support)

add_executable(http-server ${SOURCE_FILES})

# Link pthreads and enable common warnings
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

target_link_libraries(http-server PRIVATE Threads::Threads)

target_compile_options(http-server PRIVATE -Wall -Wextra -Wpedantic)
