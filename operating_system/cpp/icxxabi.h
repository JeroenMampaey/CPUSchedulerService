// This is all from: https://wiki.osdev.org/C%2B%2B#Global_objects
// and https://forum.osdev.org/viewtopic.php?t=23337

#ifndef ICXXABI_H_
#define ICXXABI_H_
#define ATEXIT_FUNC_MAX 128
extern "C" {

typedef unsigned uarch_t;

struct atexitFuncEntry_t {
    void (*destructorFunc) (void *);
    void *objPtr;
    void *dsoHandle;

};

extern void *__dso_handle;

int __cxa_atexit(void (*f)(void *), void *objptr, void *dso);
void __cxa_finalize(void *f);

void __cxa_pure_virtual();

};

#endif//ICXXABI_H_