
/**
 * This file contains several declarations that are needed for global constructors to work
 **/

extern "C" {

    void* __dso_handle;

    int __cxa_atexit(void (*f)(void *), void *objptr, void *dso) { }

    void __cxa_finalize(void *f) { }

}