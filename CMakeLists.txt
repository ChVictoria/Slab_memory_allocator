cmake_minimum_required(VERSION 3.26)
project(Slab_memory_allocator C)

set(CMAKE_C_STANDARD 11)

add_executable(Slab_memory_allocator main.c
        allocator.c
        allocator.h
        config.h
        bitmap.c
        bitmap.h
        radix_tree.c
        radix_tree.h
        kernel.c
        kernel.h
        hashtable.c
)
