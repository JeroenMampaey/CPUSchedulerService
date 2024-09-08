#pragma once

#include "../../cpp_lib/memory_manager.h"

class SerialLog{
    private:
        static SerialLog* pSerialLog;

        SerialLog();

    public:
        static SerialLog* getSerialLog();
        static void initialize(MemoryManager* pMemoryManager);

        void log(char* message);
        void log(int a);
        void logHex(unsigned int a);
        void logHex(unsigned long long a);
};