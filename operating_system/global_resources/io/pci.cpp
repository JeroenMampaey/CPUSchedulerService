#include "pci.h"
#include "ports.h"

#define PCI_CONFIG_ADDRESS 0xCF8
#define PCI_CONFIG_DATA 0xCFC

unsigned int pciReadDword(unsigned char bus, unsigned char slot, unsigned char func, unsigned char offset){
	unsigned int lbus  = (unsigned int)bus;
    unsigned int lslot = (unsigned int)slot;
    unsigned int lfunc = (unsigned int)func;
	unsigned int address = (unsigned int)((lbus << 16) | (lslot << 11) | (lfunc << 8) | (offset & 0xfc) | ((unsigned int)0x80000000));
	portDwordOut((unsigned short)PCI_CONFIG_ADDRESS, address);
	unsigned int retval = portDwordIn((unsigned short)PCI_CONFIG_DATA);
	return retval;
}

void pciWriteDword(unsigned char bus, unsigned char slot, unsigned char func, unsigned char offset, unsigned int dword){
	unsigned int lbus  = (unsigned int)bus;
    unsigned int lslot = (unsigned int)slot;
    unsigned int lfunc = (unsigned int)func;
    unsigned int address = (unsigned int)((lbus << 16) | (lslot << 11) | (lfunc << 8) | (offset & 0xfc) | ((unsigned int)0x80000000));
    portDwordOut((unsigned short)PCI_CONFIG_ADDRESS, address);
    portDwordOut((unsigned short)PCI_CONFIG_DATA, dword);
}