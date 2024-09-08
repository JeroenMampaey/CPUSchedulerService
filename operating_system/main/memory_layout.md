# Memory Layout Used By Operating System (according to the virtual addresses described by the kernel page directory and kernel page tables)

| Address Range | Content |
|-|-|
| 0x0 - 0x400 | Interrupt Vector Table |
| 0x400 - 0x500 | BIOS Data Area |
| 0x600 - 0x800 | Bootsector Code |
| 0x800 - ? <sup>1</sup> | Memory Map |
| 0x1000 - ? | Kernel |
| 0x9fc00 - 0xA0000 | Extended BIOS Data Area |
| 0xA0000 - 0xC0000 | Video Memory |
| 0xC0000 - 0x100000 | BIOS |
| 0x100000 - 0x110000 | Kernel stack used during main method |
| 0x110000 - 0x2000000 | Free space for the kernel, managed by MemoryManager |
| 0x2000000 - 0xFFC00000 <sup>2</sup> | Free memory space managed by the PageAllocator, used mainly as address space for user tasks |

---
**NOTES**

<sup>1</sup> Memory map is not very big, normally around 196 bytes (0xC4 bytes).

<sup>2</sup> 0xFFC00000 - 0xFFFFFFFF is inaccessible because paging uses these virtual addresses for some tricks allowing software to translate virtual to physical addresses

---