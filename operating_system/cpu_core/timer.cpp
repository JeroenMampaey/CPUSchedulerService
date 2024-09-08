#include "timer.h"
#include "cpu_core.h"
#include "../global_resources/io/ports.h"
#include "../../cpp_lib/atomic.h"
#include "../global_resources/screen.h"

void timerHandler(unsigned int interruptParam, unsigned int eax){
    Timer* pTimer = (Timer*)interruptParam;

    if(pTimer->pRunnable != nullptr){
        pTimer->pRunnable->run();
    }
}

Timer::Timer(unsigned int frequency)
    :
    frequency((frequency==0 || frequency>1193182) ? 1 : frequency),
    pRunnable(nullptr)
{}

void Timer::bind(){
    unsigned int dividerInt = 1193182 / frequency;
    unsigned short divider = (dividerInt>0xFFFF) ? 0xFFFF : (dividerInt & 0xFFFF);
    unsigned char low  = (unsigned char)(divider & 0xFF);
    unsigned char high = (unsigned char)( (divider >> 8) & 0xFF);

    portByteOut(0x43, 0x36);
    portByteOut(0x40, low);
    portByteOut(0x40, high);

    unsigned int cpuCoreId = CpuCore::getThisCpuCoreId();
    CpuCore* pCpuCore = CpuCore::getCpuCore(cpuCoreId);

    if(pCpuCore==nullptr){
        Screen* pScreen = Screen::getScreen();
        pScreen->printk((char*)"Tried to bind to timer while cpu core was not yet bound!\n");
        while(true);
    }

    // TODO: APIC will have to be used in multicore environment so that each core gets it's own timer interrupts
    pCpuCore->getInterruptHandlerManager()->setInterruptHandlerParam(InterruptType::PITTimer, (unsigned int)this);
    pCpuCore->getInterruptHandlerManager()->setInterruptHandler(InterruptType::PITTimer, timerHandler);
}

void Timer::setTimerCallback(Runnable* pNewRunnable){
    atomicStore((unsigned int*)&pRunnable, (unsigned int)pNewRunnable);
}
