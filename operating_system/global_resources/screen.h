#pragma once

#include "../../cpp_lib/memory_manager.h"

class Screen{
        friend void initializeGlobalResourcesHelper();

    private:
        static Screen* pScreen;

        Screen();

    public:
        static Screen* getScreen();
        static void initialize(MemoryManager* pMemoryManager);

        void printk(char* message);
        void printk(int a);
        void printkHex(unsigned int a);
        void printkHex(unsigned long long a);
        void clearScreen();
};