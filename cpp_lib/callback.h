#pragma once

class Runnable{
    public:
        virtual ~Runnable() = default;

        virtual void run() = 0;

        static void operator delete (void *p){
            return;
        }
};

template<class T>
class Callable{
    public:
        virtual ~Callable() = default;

        virtual void call(T t) = 0;

        static void operator delete (void *p){
            return;
        }
};