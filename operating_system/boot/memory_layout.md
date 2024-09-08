# Memory Layout After Boot

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
| 0x110000 - 0x120000 | Stack |

---
**NOTES**

<sup>1</sup> Memory map is not very big, normally around 196 bytes (0xC4 bytes).

---