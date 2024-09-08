#pragma once

#include "../../cpp_lib/callback.h"

class Timer{    
        friend class CpuCore;
        friend void timerHandler(unsigned int interruptParam, unsigned int eax);

    private:
        Timer(unsigned int frequency);

        unsigned int frequency;
        Runnable* pRunnable;
    
    public:
        void bind();

        void setTimerCallback(Runnable* pNewRunnable);
};