# About

This project tries to create a minimal operating system with only a task scheduler and a network driver. The idea of this operating system is that it offers a [CoAP](https://en.wikipedia.org/wiki/Constrained_Application_Protocol) API which can be used to deploy tasks to the operating system. Tasks run in main memory and can use system calls to interact with the network, enabling them to access resources, return results, and communicate with other systems. An operating system like this could be used to deploy simple server applications, disregarding the fact that there are however currently still a lot of limitations.

# Features

- A 32-bit OS for x86 CPUs
- A round-robin task scheduler with fully logically separated tasks running in ring 3 with paging
- A [CoAP](https://en.wikipedia.org/wiki/Constrained_Application_Protocol) API for deploying tasks to the OS
- An [E1000](https://www.intel.com/content/dam/doc/manual/pci-pci-x-family-gbe-controllers-software-dev-manual.pdf) network driver
- System calls for UDP reception and transmission, along with minor calls for text output and timer counter retrieval
- Tested on [QEMU](https://www.qemu.org/) and [VirtualBox](https://www.virtualbox.org/).
- Automated end-to-end testing and unit testing

# Documentation

[Mkdocs](https://www.mkdocs.org/) has been used for documentation on how to run this operating system, code architecture, limitations,... Thus to get access to this documentation, first install mkdocs.

```
CPU_SCHEDULER_SERVICE> pip install mkdocs
CPU_SCHEDULER_SERVICE> pip install mkdocs-same-dir
CPU_SCHEDULER_SERVICE> pip install mkdocs-redirects
```

And then run the mkdocs server:

```
CPU_SCHEDULER_SERVICE> mkdocs serve
INFO    -  Building documentation...
INFO    -  Cleaning site directory
INFO    -  The following pages exist in the docs directory, but are not included in the "nav" configuration:
             - README.md
             - operating_system\boot\memory_layout.md
             - operating_system\main\memory_layout.md
INFO    -  Documentation built in 0.70 seconds
INFO    -  [14:00:24] Watching paths for changes: '.', 'mkdocs.yml'
INFO    -  [14:00:24] Serving on http://127.0.0.1:8000/
```