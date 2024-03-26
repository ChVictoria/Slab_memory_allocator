#include "kernel.h"
#include <stdio.h>
#include <errhandlingapi.h>

static _Noreturn void failed_kernel_alloc(void)
{
    fprintf(stderr, "kernel_alloc() failed\n");
    exit(EXIT_FAILURE);
}

static _Noreturn void failed_kernel_free(void)
{
    fprintf(stderr, "kernel_free() failed\n");
    exit(EXIT_FAILURE);
}

static _Noreturn void failed_kernel_reset(void)
{
    fprintf(stderr, "kernel_reset() failed\n");
    exit(EXIT_FAILURE);
}
static _Noreturn void failed_kernel_realloc(void){
    fprintf(stderr, "kernel_realloc() failed\n");
    exit(EXIT_FAILURE);
}

void * kernel_alloc(size_t size){
    void *ptr;
    ptr = VirtualAlloc(NULL, size, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
    if (ptr == NULL) {
        printf("Error code %lu\n", GetLastError());
        failed_kernel_alloc();
    }
    return ptr;
}

void * kernel_realloc(void *ptr, size_t new_size, size_t size){
    if (new_size<size){
        VirtualFree((void *)((unsigned long long)ptr+size-new_size), 0, MEM_DECOMMIT);
        return ptr;
    }
    void *add_ptr = VirtualAlloc((void *)((unsigned long long)ptr+new_size-size), size, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
    if (add_ptr == NULL) {
        printf("Failed realloc kernel memory, free and alloc it again\n");
    }
    return ptr;
}

void kernel_free(void *ptr){
    if (VirtualFree(ptr, 0, MEM_RELEASE) == 0){
        printf("Error code %lu\n", GetLastError());
        failed_kernel_free();
    }
}

void kernel_reset(void *ptr, size_t size){
    if (VirtualAlloc(ptr, size, MEM_RESET, PAGE_READWRITE) == NULL) {
        printf("Error code %lu\n", GetLastError());
        failed_kernel_reset();
    }
}

