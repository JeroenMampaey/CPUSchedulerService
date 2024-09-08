# Current Limitations

- This OS was written in relatively short time for fun, it is likely full of bugs and vulnerabilities. Don't expect this to work in some production environment.
- The OS only offers syscalls for sending and receiving UDP packets, writing a TCP stack is incredibly difficult.
- The OS currently only runs on a single CPU core, and only supports the x86 architecture.
- The code has only been tested with QEMU and VirtualBox.
- The network driver used by the OS is specifically for e1000 network cards
- The OS runs in 32-bit mode.
- The code is still full with TODOs
- Tasks can only use `0x800000` bytes of memory, it is currently impossible to allow tasks to allocate more memory.
- The API for managing tasks on the operating system is very simplistic. Cannot check if a task has crashed or if a task is done etc. Additionally, the API for uploading tasks does not use encryption or authentication.
- The code for the e1000 network card especially is created with a lot of help from the [OSdev wiki](https://wiki.osdev.org/Intel_Ethernet_i217), never fully read the manual, only parts of it