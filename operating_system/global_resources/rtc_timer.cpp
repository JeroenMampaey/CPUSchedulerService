#include "rtc_timer.h"
#include "io/ports.h"
#include "../../cpp_lib/placement_new.h"
#include "screen.h"
#include "../cpu_core/cpu_core.h"
#include "../../cpp_lib/atomic.h"

#define REGISTER_NUMBER_SPECIFIER_PORT 0x70
#define REGISTER_READER_WRITER_PORT 0x71

void rtcTimerInterruptHandler(unsigned int interruptParam, unsigned int eax){
    RTCTimer* pRTCTimer = (RTCTimer*)interruptParam;
    Runnable* pRunnable = pRTCTimer->pRunnable;
    if(pRunnable != nullptr){
        pRunnable->run();
    }

    pRTCTimer->allowNextInterrupt();
}

RTCTimer* RTCTimer::pRTCTimer = nullptr;

RTCTimer* RTCTimer::getRTCTimer(){
    return pRTCTimer;
}

void RTCTimer::initialize(MemoryManager* pMemoryManager){
    unsigned char* rtcTimerAddr = pMemoryManager->allocate(alignof(RTCTimer), sizeof(RTCTimer));
    if(rtcTimerAddr == nullptr){
        Screen* pScreen = Screen::getScreen();
        pScreen->printk((char*)"Failed to allocate memory for RTC timer!\n");
        while(true);
    }

    pRTCTimer = new(rtcTimerAddr) RTCTimer();
}

RTCTimer::RTCTimer()
    :
    interruptsEnabled(false),
    pRunnable(nullptr)
{
    portByteOut(REGISTER_NUMBER_SPECIFIER_PORT, 0x8B);
    unsigned char registerBValue = portByteIn(REGISTER_READER_WRITER_PORT);
    portByteOut(REGISTER_NUMBER_SPECIFIER_PORT, 0x8B);
    portByteOut(REGISTER_READER_WRITER_PORT, registerBValue | 0x40);
}

void RTCTimer::setTimerFrequency(unsigned int frequency){
    // Only allowed frequencies are (0x8000 >> (rate-1))
    // Rate here is a 4 bit value and must be bigger than or equal to 3
    // Thus 3 <= rate <= 15
    // Thus 2^1 <= frequency <= 2^13
    // And also frequency must be a power of 2
    if(frequency>8192) return;

    if(frequency<2) return;

    unsigned int rate = 0;
    for(int i=8192, iPower=13; i>=2; i/=2, iPower-=1){
        if(frequency & i){
            // log2(frequency) = 15-(rate-1)
            // iPower = 15-(rate-1)
            // rate = 16-iPower
            rate = 16-iPower;
            break;
        }
    }

    // RTCTimer interrupts will be handled by the CPU core 0
    CpuCore* pCpuCore = CpuCore::getCpuCore(0);
    if(pCpuCore == nullptr){
        Screen* pScreen = Screen::getScreen();
        pScreen->printk((char*)"Failed to get CPU core 0!\n");
        while(true);
    }

    class ChangeRTCTimerRate : public Runnable{
        private:
            unsigned int rate;

        public:
            ChangeRTCTimerRate(unsigned int rate)
                :
                rate(rate)
            {}

            void run() override{
                portByteOut(REGISTER_NUMBER_SPECIFIER_PORT, 0x8A);
                unsigned char registerAValue = portByteIn(REGISTER_READER_WRITER_PORT);
                portByteOut(REGISTER_NUMBER_SPECIFIER_PORT, 0x8A);
                portByteOut(REGISTER_READER_WRITER_PORT, (registerAValue & 0xF0) | rate);
            }
    };
    
    ChangeRTCTimerRate changeRTCTimerRate(rate);
    pCpuCore->withTaskSwitchingPaused(changeRTCTimerRate);
}

void RTCTimer::allowNextInterrupt(){
    portByteOut(REGISTER_NUMBER_SPECIFIER_PORT, 0x0C);
    portByteIn(REGISTER_READER_WRITER_PORT);
}

void RTCTimer::setTimerCallback(Runnable* pNewRunnable){
    atomicStore((unsigned int*)&pRunnable, (unsigned int)pNewRunnable);

    if(!interruptsEnabled){
        // RTCTimer interrupts will be handled by the CPU core 0
        CpuCore* pCpuCore = CpuCore::getCpuCore(0);
        if(pCpuCore == nullptr){
            Screen* pScreen = Screen::getScreen();
            pScreen->printk((char*)"Failed to get CPU core 0!\n");
            while(true);
        }

        pCpuCore->getInterruptHandlerManager()->setInterruptHandlerParam(InterruptType::RTCTimer, (unsigned int)this);
        pCpuCore->getInterruptHandlerManager()->setInterruptHandler(InterruptType::RTCTimer, rtcTimerInterruptHandler);

        interruptsEnabled = true;
        allowNextInterrupt();
    }
}