#include "screen.h"
#include "../../cpp_lib/mem.h"
#include "../../cpp_lib/string.h"
#include "io/ports.h"

#include "../../cpp_lib/placement_new.h"

#define VIDEO_BUFFER_ADDRESS 0xb8000
#define REG_SCREEN_CTRL 0x3d4
#define REG_SCREEN_DATA 0x3d5

#define MAX_ROWS 25
#define MAX_COLS 80

static volatile unsigned char* videoBuffer = (unsigned char*)VIDEO_BUFFER_ADDRESS;

Screen* Screen::pScreen = nullptr;

Screen* Screen::getScreen(){
    return pScreen;
}

void Screen::initialize(MemoryManager* pMemoryManager){
    unsigned char* screenAddr = pMemoryManager->allocate(alignof(Screen), sizeof(Screen));
    if(screenAddr == nullptr){
        while(true);
    }

    pScreen = new(screenAddr) Screen();
}

int getCursorOffset(){
    portByteOut(REG_SCREEN_CTRL, 14);
    int offset = portByteIn(REG_SCREEN_DATA) << 8;
    portByteOut(REG_SCREEN_CTRL, 15);
    offset += portByteIn(REG_SCREEN_DATA);
    return 2*offset;
}

void setCursorOffset(int offset){
    offset /= 2;
    portByteOut(REG_SCREEN_CTRL, 14);
    portByteOut(REG_SCREEN_DATA, (unsigned char)(offset >> 8));
    portByteOut(REG_SCREEN_CTRL, 15);
    portByteOut(REG_SCREEN_DATA, (unsigned char)(offset & 0xff));
}

unsigned int printChar(int offset, char c){
    if(c == '\n'){
        offset = 2*(offset/(2*MAX_COLS)+1)*MAX_COLS;
    }
    else{
        videoBuffer[offset] = c;
        videoBuffer[offset+1] = 0x0f;
        offset += 2;
    }

    if (offset >= 2*MAX_ROWS*MAX_COLS){
        memCopy(2*MAX_COLS + (unsigned char*)videoBuffer, (unsigned char*)videoBuffer, 2*(MAX_ROWS-1)*(MAX_COLS));
        
        memClear(2*(MAX_ROWS-1)*MAX_COLS + (unsigned char*)videoBuffer, 2*MAX_COLS);

        offset -= 2*MAX_COLS;
    }

    setCursorOffset(offset);
    videoBuffer[offset+1] = 0x0f;

    return offset;
}

Screen::Screen(){
    // Empty the video buffer
    memClear((unsigned char*)VIDEO_BUFFER_ADDRESS, 2*MAX_COLS*MAX_ROWS);
    portByteOut(REG_SCREEN_CTRL, 14);
    portByteOut(REG_SCREEN_DATA, (unsigned char)0);
    portByteOut(REG_SCREEN_CTRL, 15);
    portByteOut(REG_SCREEN_DATA, (unsigned char)0);
    ((unsigned char*)VIDEO_BUFFER_ADDRESS)[1] = 0x0f;
}

void Screen::clearScreen(){
    memClear((unsigned char*)videoBuffer, 2*MAX_COLS*MAX_ROWS);
    setCursorOffset(0);
    videoBuffer[1] = 0x0f;
}

void Screen::printk(char* message){
    int offset = getCursorOffset();

    int i=0;
    while(message[i]!='\0'){
        offset = printChar(offset, message[i]);
        i++;
    }
}

void Screen::printk(int a){
    char intAsDecimalString[12];
    intToDecimalString(a, intAsDecimalString);
    printk(intAsDecimalString);
}

void Screen::printkHex(unsigned int a){
    char intAsHexadecimalString[9];
    intToHexadecimalString(a, intAsHexadecimalString);
    printk(intAsHexadecimalString);
}

void Screen::printkHex(unsigned long long a){
    unsigned int lowerA = (unsigned int)(a & 0xFFFFFFFF);
    unsigned int upperA = (unsigned int)((a >> 32) & 0xFFFFFFFF);

    char lowerAAsHexadecimalString[9];
    intToHexadecimalString(lowerA, lowerAAsHexadecimalString);

    if(upperA > 0){
        char upperAAsHexadecimalString[9];
        intToHexadecimalString(upperA, upperAAsHexadecimalString);
        printk(upperAAsHexadecimalString);


        int numMissingZeros = 8-strlen(lowerAAsHexadecimalString);
        int offset = getCursorOffset();
        for(int i=0; i<numMissingZeros; i++){
            offset = printChar(offset, '0');
        }
    }

    printk(lowerAAsHexadecimalString);
}