#include "syscalls.h"
#include "../operating_system/cpu_core/cpu_core.h"
#include "../operating_system/network_management_task/socket_manager.h"
#include "string.h"

void yield(){
    __asm__ __volatile__(
        ".intel_syntax noprefix;"
        "int 49;"
        ".att_syntax;"
    : : : );
}

int openSocket(unsigned short udpPort){
    OpenSocketSyscallArgs args;
    args.udpPort = udpPort;
    args.socketID = -1;
    unsigned int eax = (unsigned int)&args;
    __asm__ __volatile__(
        ".intel_syntax noprefix;"
        "int 50;"
        ".att_syntax;"
    : : "a"(eax) : "memory");
    return args.socketID;
}

int setReceiveBuffer(unsigned char socketID, unsigned char* buffer, unsigned int bufferSize){
    if(bufferSize >= RECEIVE_BUFFER_HEADER_SIZE && buffer != nullptr){
        for(int i=0; i<RECEIVE_BUFFER_HEADER_SIZE; i++){
            buffer[i] = 0;
        }
    }

    SetReceiveBufferSyscallArgs args;
    args.socketID = socketID;
    args.buffer = buffer;
    args.bufferSize = bufferSize;
    args.success = -1;
    unsigned int eax = (unsigned int)&args;
    __asm__ __volatile__(
        ".intel_syntax noprefix;"
        "int 51;"
        ".att_syntax;"
    : : "a"(eax) : "memory");
    return args.success;
}

int setSendBuffer(unsigned char socketID, unsigned char* buffer, unsigned int bufferSize, int* indicatorWhenFinished){
    if(indicatorWhenFinished != nullptr){
        *indicatorWhenFinished = 0;
    }

    SetSendBufferSyscallArgs args;
    args.socketID = socketID;
    args.buffer = buffer;
    args.bufferSize = bufferSize;
    args.indicatorWhenFinished = indicatorWhenFinished;
    args.success = -1;
    unsigned int eax = (unsigned int)&args;
    __asm__ __volatile__(
        ".intel_syntax noprefix;"
        "int 52;"
        ".att_syntax;"
    : : "a"(eax) : "memory");
    return args.success;
}

void closeSocket(unsigned char socketID){
    CloseSocketSyscallArgs args;
    args.socketID = socketID;
    unsigned int eax = (unsigned int)&args;
    __asm__ __volatile__(
        ".intel_syntax noprefix;"
        "int 53;"
        ".att_syntax;"
    : : "a"(eax) : "memory");
}

void print(char* string){
    PrintToScreenSyscallArgs args;
    args.stringSize = strlen(string);
    args.string = string;

    unsigned int eax = (unsigned int)&args;
    __asm__ __volatile__(
        ".intel_syntax noprefix;"
        "int 54;"
        ".att_syntax;"
    : : "a"(eax) : "memory");
}

unsigned int getTimerCounter(){
    GetTimerSyscallArgs args;
    unsigned int eax = (unsigned int)&args;
    __asm__ __volatile__(
        ".intel_syntax noprefix;"
        "int 55;"
        ".att_syntax;"
    : : "a"(eax) : "memory");
    return args.timerCounter;
}

void e2eTestingLog(int loggedValue){
    __asm__ __volatile__(
        ".intel_syntax noprefix;"
        "int 48;"
        ".att_syntax;"
    : : "a"((unsigned int)loggedValue) : "memory");
}