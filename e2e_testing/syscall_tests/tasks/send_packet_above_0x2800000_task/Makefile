C_SOURCES = $(wildcard ./*.cpp ../../../../cpp_lib/*.cpp)
HEADERS = $(wildcard ./*.h ../../../../cpp_lib/*.h)
ASMS = $(wildcard ./*.asm ../../../../cpp_lib/*.asm)

OBJ1 = ${C_SOURCES:.cpp=.o}
OBJ2 = ${ASMS:.asm=.o}

# RANDOM generates random number in the interval [0, 32767]
# We want to generate random number in the interval [0x2800000, 0xFFFFFFFF]
# (0xFFFFFFFF-0x2800000)/32767 = 129795.961028
# So, we multiply the random number by 129795 and then add 0x2800000
RANDOM_NUMBER := $(shell bash -c 'echo $$((RANDOM * 129795 + 0x2800000))')

CPPFLAGS=

all: task.bin

task.bin: task_entry.o ${OBJ1} ${OBJ2}
	ld -m elf_i386 -o $@ -T ../link.ld --oformat binary $^

%.o : %.cpp ${HEADERS}
	g++ -DRANDOM=$(RANDOM_NUMBER) $(CPPFLAGS) -O3 -fno-exceptions -fno-rtti -std=c++17 -fno-pie -ffreestanding -m32 -c $< -o $@

%.o : %.asm
	nasm $< -f elf -o $@

clean:
	rm -fr task.bin
	rm -fr *.o
	rm -fr ../../../../cpp_lib/*.o