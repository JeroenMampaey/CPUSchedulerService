#pragma once

#include "../../cpp_lib/memory_manager.h"
#include "../../cpp_lib/callback.h"

class RTCTimer{
        friend void rtcTimerInterruptHandler(unsigned int interruptParam, unsigned int eax);

    private:
        static RTCTimer* pRTCTimer;
        
        bool interruptsEnabled;
        Runnable* pRunnable;

        RTCTimer();

        void allowNextInterrupt();

    public:
        static RTCTimer* getRTCTimer();
        static void initialize(MemoryManager* pMemoryManager);
        
        // Frequency can range from 2 till 8192 (2 <= frequency <= 8192)
        // but should be a power of 2
        void setTimerFrequency(unsigned int frequency);

        void setTimerCallback(Runnable* pNewRunnable);
};