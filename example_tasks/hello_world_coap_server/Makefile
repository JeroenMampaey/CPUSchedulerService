C_SOURCES = $(wildcard ./*.cpp ../../cpp_lib/*.cpp)
HEADERS = $(wildcard ./*.h ../../cpp_lib/*.h)
ASMS = $(wildcard ./*.asm ../../cpp_lib/*.asm)

OBJ1 = ${C_SOURCES:.cpp=.o}
OBJ2 = ${ASMS:.asm=.o}

all: task.bin

task.bin: task_entry.o ${OBJ1} ${OBJ2}
	ld -m elf_i386 -o $@ -T ../link.ld --oformat binary $^

%.o : %.cpp ${HEADERS}
	g++ -O3 -fno-exceptions -fno-rtti -std=c++17 -fno-pie -ffreestanding -m32 -c $< -o $@

%.o : %.asm
	nasm $< -f elf -o $@

clean:
	rm -fr task.bin
	rm -fr *.o
	rm -fr ../../cpp_lib/*.o