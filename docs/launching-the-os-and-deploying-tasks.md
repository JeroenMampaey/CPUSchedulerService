# Preparing Compilation

To compile the operating system, go to the operating system folder. There is a `config.yaml` file that needs to be correctly configured. An example of this file looks like this:

<div style="font-family: Arial, sans-serif; max-width: 100%; overflow-x: auto;">
    <pre id="file-content" style="background: #f4f4f4; border-radius: 5px; white-space: pre; overflow-x: auto;">Loading...</pre>
    <script>
        fetch('/operating_system/config.yaml')
            .then(response => response.text())
            .then(text => {
                document.getElementById('file-content').textContent = text;
            })
            .catch(err => {
                document.getElementById('file-content').textContent = 'Error loading file.';
            });
    </script>
</div>

This config file has 3 main sections:

- `operating_system`
- `vbox`
- `qemu`

The `operating_system` section must be configured first. Depending on whether you want to compile for QEMU or VirtualBox, you can optionally fill in the other sections.

### operating_system

To compile the operating system, as outlined in the [Makefile](os-makefile.md), you need `g++`, `nasm`, and `ld`. Compiling on Linux is straightforward, while on Windows, it’s slightly more involved but feasible with [WSL](https://learn.microsoft.com/en-us/windows/wsl).

The first parameter in the `operating_system` section, `"make_command"`, specifies how to invoke `make`. On Linux, this should typically be `"make"`. On Windows, you should use WSL to call `make`, so it should be `"wsl -d Ubuntu-20.04 make"` (you can check available Linux distributions with `wsl --list`; it doesn’t have to be Ubuntu-20.04).

The second parameter, `"ip"`, indicates the static IP address that the operating system should use. The values for gateway IP and netmask are self-explanatory.

### vbox

The vbox section requires two parameters: `"disk: path"` and `"disk: actual_disk_start"`. These values should be the disk file path and the disk start offset, which were noted during the [VirtualBox setup](qemu-virtualbox-setup.md#virtualbox).

### qemu

The qemu section requires the `"disk: path"` parameter. This value should be the disk file path, which was noted during the [QEMU setup](qemu-virtualbox-setup.md#qemu).

# Compiling

### Dependencies

First, go to the python_lib folder and install required python dependencies:

```
CPU_SCHEDULER_SERVICE/python_lib> pip install -r requirements.txt
```

Then whether on linux or WSL, make sure the following is installed:

- `g++`: `sudo apt-get install g++`
- `nasm`: `sudo apt-get install nasm`
- `ld`: `sudo apt-get install binutils`

### Compiling

Go to the operating system folder and execute the following command to compile for QEMU:

```
CPU_SCHEDULER_SERVICE/operating_system> python ./build_OS_and_deploy.py --virtualizer=0
```

Or for VirtualBox:

```
CPU_SCHEDULER_SERVICE/operating_system> python ./build_OS_and_deploy.py --virtualizer=1
```

# Running Tasks

### API Overview

The operating system includes a CoAP server running on port 5683, providing the following API endpoints for managing tasks:

- **Create a Task**
    - **Endpoint:** `PUT /tasks/{id}`
    - **Description:** Create a new task with the specified ID.

- **Get Task State**
    - **Endpoint:** `GET /tasks/{id}/state`
    - **Description:** Retrieve the current state of the task. Returns "Running" or "Not Running".

- **Set Task Data**
    - **Endpoint:** `PUT /tasks/{id}/data/{address}`
    - **Description:** Set data at the specified address for the task. The data is sent as an octet-stream payload.

- **Update Task State**
    - **Endpoint:** `PUT /tasks/{id}/state`
    - **Description:** Update the task's state. The state can only be set to "Running" using a plain-text payload.

- **Delete a Task**
    - **Endpoint:** `DELETE /tasks/{id}`
    - **Description:** Delete the task with the specified ID.

There are two ways in which CoAP can send packets which exceed the [MTU](https://en.wikipedia.org/wiki/Maximum_transmission_unit) size:

- [Block wise transfers](https://www.rfc-editor.org/rfc/rfc7959)
- Using IPv4 fragmentation

IPv4 fragmentation is generally a poor idea and not used, however it is the only method that the CoAP server currently supports. It does not support block wise transfers.

### Task Address Space

Tasks can access the address space from `0x2014000` to `0x2800000`. Once a task is set to the running state, the operating system will set it's instruction pointer to `0x2024000`, with the stack pointer set to `0x2023FFF`. Basically this means for your task you have to:

- **Executable Binary:** Load your binary executable starting at address `0x2024000`.
- **Stack Space:** Avoid accessing the address range between `0x2014000` and `0x2024000` as it is reserved for the stack.
- **Free Space:** You can use the address space above your binary executable and below `0x2800000` as needed.

Accessing address space which you are not allowed to of course leads to a [General Protection Fault](https://en.wikipedia.org/wiki/General_protection_fault). For tasks it might seem they are all accessing the same address space but this is of course not true since this is virtual address space, not physical address space.

The `cpp_lib` folder contains useful code for writing tasks. Key files include:

- [`syscalls.h`](syscalls-header.md)
- [`syscalls.cpp`](syscalls-cpp.md)

These files detail the system calls available to tasks.

### Example Tasks

It might be easier to understand how to create and deploy tasks by reviewing the examples from the example tasks folder first. Ensure you correctly configure the `config.yaml` file:

<div style="font-family: Arial, sans-serif; max-width: 100%; overflow-x: auto;">
    <pre id="file-content2" style="background: #f4f4f4; border-radius: 5px; white-space: pre; overflow-x: auto;">Loading...</pre>
    <script>
        fetch('/example_tasks/config.yaml')
            .then(response => response.text())
            .then(text => {
                document.getElementById('file-content2').textContent = text;
            })
            .catch(err => {
                document.getElementById('file-content2').textContent = 'Error loading file.';
            });
    </script>
</div>

Configure the `"operating_system: ip"` parameter with the static IP address of the operating system (see the [previous section](#operating_system)).

The `"tasks"` list in `config.yaml` should include:

- `"make_command"`: Command for executing `make`
- `"make_directory_path"`: Path to the directory containing the Makefile for the task
- `"image_path"`: Path to the compiled binary
- `"id"`: Identifier for the task (used with the CoAP API)

In the example tasks folder, the python files `build_tasks.py`, `deploy_tasks.py` and `remove_tasks.py` are not too difficult to read and show how to build the code for the tasks, how to deploy tasks to the operating system using the CoAP API and how to remove tasks from the operating system using the CoAP API. Notice for example the usage of `handle_blockwise=False` in the `deploy_tasks.py` script, which is used since the operating system does not currently support block wise transfers of large packets.

Some example tasks which are already available (see the subdirectories in the example tasks folder):

- `hello_world_coap_server`: [README.md](../example_tasks/hello_world_coap_server/README.md)
- `print_hello_world0`: [README.md](../example_tasks/print_hello_world0/README.md)
- `print_hello_world1`: [README.md](../example_tasks/print_hello_world1/README.md)

Their source code is straightforward and worth looking into. The `task_entry.asm` file initializes the binary and jumps to `main.cpp`. The `link.ld` file specifies the binary’s execution address.

**Pip install the python dependencies:**
```
CPU_SCHEDULER_SERVICE/example_tasks> pip install -r requirements.txt
```

**To compile tasks specified in `config.yaml`:**
```
CPU_SCHEDULER_SERVICE/example_tasks> python ./build_tasks.py
```

**To deploy a task with `"id"`=1:**
```
CPU_SCHEDULER_SERVICE/example_tasks> python ./deploy_tasks.py --task_id=1
```

**To remove a task with `"id"`=1:**
```
CPU_SCHEDULER_SERVICE/example_tasks> python ./remove_tasks.py --task_id=1
```