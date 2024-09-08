C_SOURCES = $(wildcard cpp/*.cpp cpu_core/*.cpp global_resources/*.cpp global_resources/io/*.cpp ../cpp_lib/*.cpp main/*.cpp network_management_task/*.cpp os_management_task/*.cpp paging/*.cpp)
HEADERS = $(wildcard cpp/*.h cpu_core/*.h global_resources/*.h global_resources/io/*.h ../cpp_lib/*.h main/*.h network_management_task/*.h os_management_task/*.h paging/*.h)
ASMS = $(wildcard cpp/*.asm cpu_core/*.asm global_resources/*.asm global_resources/io/*.asm ../cpp_lib/*.asm main/*.asm network_management_task/*.asm os_management_task/*.asm paging/*.asm)

OBJ1 = ${C_SOURCES:.cpp=.o}
OBJ2 = ${ASMS:.asm=.o}

CPPFLAGS=-DTHIS_IP=167772687 -DGATEWAY_IP=167772674

all: os_image.bin

os_image.bin: boot/boot_sect.bin kernel.bin
	cat $^ > os_image.bin

kernel.bin: main/kernel_entry.o ${OBJ1} ${OBJ2}
	ld -m elf_i386 -o $@ -T link.ld --oformat binary $^

%.o : %.cpp ${HEADERS}
	g++ $(CPPFLAGS) -O3 -fno-exceptions -fno-rtti -std=c++17 -fno-pie -ffreestanding -m32 -c $< -o $@

%.o : %.asm
	nasm $< -f elf -o $@

%.bin : %.asm
	nasm -f bin -o $@ $<

clean:
	rm -fr *.bin
	rm -fr boot/*.bin
	rm -fr cpp/*.o
	rm -fr cpu_core/*.o
	rm -fr global_resources/*.o
	rm -fr global_resources/io/*.o
	rm -fr ../cpp_lib/*.o
	rm -fr main/*.o
	rm -fr network_management_task/*.o
	rm -fr os_management_task/*.o
	rm -fr paging/*.o