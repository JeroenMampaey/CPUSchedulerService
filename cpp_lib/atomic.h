#pragma once

extern "C" void atomicStore(unsigned int* destination, unsigned int value);

extern "C" bool conditionalExchange(unsigned int* destination, unsigned int expected, unsigned int desired);