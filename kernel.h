#include <memoryapi.h>

void * kernel_alloc(size_t size);
void kernel_free(void *ptr);
void kernel_reset(void *ptr, size_t size);
void *kernel_realloc(void *ptr, size_t new_size, size_t size);