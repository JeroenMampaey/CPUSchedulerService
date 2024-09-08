#include "serial_log.h"
#include "io/ports.h"
#include "../../cpp_lib/string.h"
#include "../cpp/placement_new.h"
#include "screen.h"

#define SERIAL_LOG_PORT 0x3F8

SerialLog* SerialLog::pSerialLog = nullptr;

SerialLog* SerialLog::getSerialLog(){
    return pSerialLog;
}

void SerialLog::initialize(MemoryManager* pMemoryManager){
    unsigned char* serialLogAddr = pMemoryManager->allocate(alignof(SerialLog), sizeof(SerialLog));
    if(serialLogAddr == nullptr){
        Screen* pScreen = Screen::getScreen();
        pScreen->printk((char*)"Failed to allocate memory for SerialLog!\n");
        while(true);
    }

    pSerialLog = new(serialLogAddr) SerialLog();
}

SerialLog::SerialLog(){
    //https://wiki.osdev.org/Serial_Ports
    portByteOut(SERIAL_LOG_PORT + 1, 0x00);    // Disable all interrupts
    portByteOut(SERIAL_LOG_PORT + 3, 0x80);    // Enable DLAB (set baud rate divisor)
    portByteOut(SERIAL_LOG_PORT + 0, 12);      // Set divisor to 12 (lo byte) 9600 baud
    portByteOut(SERIAL_LOG_PORT + 1, 0x00);    //                  (hi byte)
    portByteOut(SERIAL_LOG_PORT + 3, 0x03);    // 8 bits, no parity, one stop bit
    portByteOut(SERIAL_LOG_PORT + 2, 0xC7);    // Enable FIFO, clear them, with 14-byte threshold
    portByteOut(SERIAL_LOG_PORT + 4, 0x0B);    // IRQs enabled, RTS/DSR set
    portByteOut(SERIAL_LOG_PORT + 4, 0x0F);    // Set in normal operation mode

    Screen* pScreen = Screen::getScreen();
    pScreen->printk((char*)"The LogManager was successfully initialized.\n");
}

void SerialLog::log(char* message){
    int i=0;
    while(message[i]!='\0'){
        // Check if the transmission buffer is empty
        while((portByteIn(SERIAL_LOG_PORT+5) & 0x20)==0);

        portByteOut(SERIAL_LOG_PORT, message[i]);
        i++;
    }
}

void SerialLog::log(int a){
    char intAsDecimalString[12];
    intToDecimalString(a, intAsDecimalString);
    log(intAsDecimalString);
}

void SerialLog::logHex(unsigned int a){
    char intAsHexadecimalString[9];
    intToHexadecimalString(a, intAsHexadecimalString);
    log(intAsHexadecimalString);
}

void SerialLog::logHex(unsigned long long a){
    unsigned int lowerA = (unsigned int)(a & 0xFFFFFFFF);
    unsigned int upperA = (unsigned int)((a >> 32) & 0xFFFFFFFF);

    char lowerAAsHexadecimalString[9];
    intToHexadecimalString(lowerA, lowerAAsHexadecimalString);

    if(upperA > 0){
        char upperAAsHexadecimalString[9];
        intToHexadecimalString(upperA, upperAAsHexadecimalString);
        log(upperAAsHexadecimalString);

        int numMissingZeros = 8-strlen(lowerAAsHexadecimalString);
        for(int i=0; i<numMissingZeros; i++){
            while((portByteIn(SERIAL_LOG_PORT+5) & 0x20)==0);

            portByteOut(SERIAL_LOG_PORT, '0');
        }
    }

    log(lowerAAsHexadecimalString);
}