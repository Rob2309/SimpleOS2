#include "simpleos_alloc.h"

#include "syscall.h"

int64 alloc_pages(void* addr, uint64 numPages) {
    return syscall_invoke(syscall_alloc, (uint64)addr, numPages);
}
int64 free_pages(void* addr, uint64 numPages) {
    return syscall_invoke(syscall_free, (uint64)addr, (uint64)numPages);
}