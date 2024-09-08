#pragma once

unsigned char portByteIn(unsigned short port);
void portByteOut(unsigned short port, unsigned char data);
unsigned short portWordIn(unsigned short port);
void portWordOut(unsigned short port, unsigned short data);
unsigned int portDwordIn(unsigned short port);
void portDwordOut(unsigned short port, unsigned int data);