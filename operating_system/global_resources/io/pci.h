#pragma once

unsigned int pciReadDword(unsigned char bus, unsigned char slot, unsigned char func, unsigned char offset);
void pciWriteDword(unsigned char bus, unsigned char slot, unsigned char func, unsigned char offset, unsigned int dword);