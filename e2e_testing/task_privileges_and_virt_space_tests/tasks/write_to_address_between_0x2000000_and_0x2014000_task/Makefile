C_SOURCES = $(wildcard ./*.cpp ../../../../cpp_lib/*.cpp)
HEADERS = $(wildcard ./*.h ../../../../cpp_lib/*.h)
ASMS = $(wildcard ./*.asm ../../../../cpp_lib/*.asm)

OBJ1 = ${C_SOURCES:.cpp=.o}
OBJ2 = ${ASMS:.asm=.o}

# RANDOM generates random number in the interval [0, 32767]
# We want to generate random number in the interval [0x2000000, 0x2014000[
# (0x14000-1)/32767 = 2.50004577776
# So, we multiply the random number by 5 and then devide it by 2 and then add 0x2000000
RANDOM_NUMBER := $(shell bash -c 'echo $$((RANDOM * 5 / 2 + 0x2000000))')

all: task.bin

task.bin: task_entry.o ${OBJ1} ${OBJ2}
	ld -m elf_i386 -o $@ -T ../link.ld --oformat binary $^

%.o : %.cpp ${HEADERS}
	g++ -DRANDOM=$(RANDOM_NUMBER) -O3 -fno-exceptions -fno-rtti -std=c++17 -fno-pie -ffreestanding -m32 -c $< -o $@

%.o : %.asm
	nasm $< -f elf -o $@

clean:
	rm -fr task.bin
	rm -fr *.o
	rm -fr ../../../../cpp_lib/*.o