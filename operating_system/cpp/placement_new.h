#pragma once

#if __GNUC__
#if __x86_64__ || __ppc64__
// This is just to keep the IDE happy, since we are compiling for 32-bit, this will not be used
typedef unsigned long int size_t;
#else
typedef unsigned int size_t;
#define ENVIRONMENT32
#endif
#endif

inline void *operator new(size_t size, void *p)  { return p; };
inline void *operator new[](size_t size, void *p)   { return p; };
inline void  operator delete  (void *, void *) { };
inline void  operator delete[](void *, void *)  { };