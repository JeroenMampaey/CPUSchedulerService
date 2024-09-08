# Unit Tests

For Unit Testing, we are using the [GoogleTest](https://github.com/google/googletest) C++ framework. Follow the **Build with CMake** instructions [here](https://github.com/google/googletest/blob/main/googletest/README.md). During this, the following is important:

- For the git clone, use the following release: `https://github.com/google/googletest -b v1.14.x`
- When executing the `cmake` command, add the following flags also: `-DCMAKE_CXX_FLAGS=-m32`

As mentioned on the [Launching The OS And Deploying Tasks](launching-the-os-and-deploying-tasks.md) page, on Windows we are using WSL, on Linux just use regular command line. I've also found that it is important here to have gcc-multilib installed: `sudo apt-get install gcc-multilib g++-multilib`. Additionally also it might be needed to add a `-DCMAKE_CXX_COMPILER` flag to g++, which is often `/usr/bin/g++`.

Then, in the unit testing folder, we need to define a `config.yaml`:

<div style="font-family: Arial, sans-serif; max-width: 100%; overflow-x: auto;">
    <pre id="file-content" style="background: #f4f4f4; border-radius: 5px; white-space: pre; overflow-x: auto;">Loading...</pre>
    <script>
        fetch('/unit_testing/config.yaml')
            .then(response => response.text())
            .then(text => {
                document.getElementById('file-content').textContent = text;
            })
            .catch(err => {
                document.getElementById('file-content').textContent = 'Error loading file.';
            });
    </script>
</div>

This config file has 2 parameters:

- `"make_command"`: Specifies how to invoke `make`, see [Launching The OS And Deploying Tasks](launching-the-os-and-deploying-tasks.md#operating_system).
- `"execute_command"`: Specifies how to run the executables which are generated. On Linux this can be just an empty string `""`, on Windows this should be for example `"wsl -d Ubuntu-20.04"`.

Then, make sure all python dependencies are installed:

```
CPU_SCHEDULER_SERVICE/unit_testing> pip install -r requirements.txt
```

And finally, run the tests:

```
CPU_SCHEDULER_SERVICE/unit_testing> python ./main.py
```

# Preparing End-to-End Tests

To run the End-to-End tests, go to the e2e testing folder and configure the `config.yaml`:

<div style="font-family: Arial, sans-serif; max-width: 100%; overflow-x: auto;">
    <pre id="file-content2" style="background: #f4f4f4; border-radius: 5px; white-space: pre; overflow-x: auto;">Loading...</pre>
    <script>
        fetch('/e2e_testing/config.yaml')
            .then(response => response.text())
            .then(text => {
                document.getElementById('file-content2').textContent = text;
            })
            .catch(err => {
                document.getElementById('file-content2').textContent = 'Error loading file.';
            });
    </script>
</div>

This config file has 4 main sections:

- `operating_system`
- `vbox`
- `qemu`
- `required_testing_info`

The `operating_system` section must be configured first. Depending on whether you want to compile for QEMU or VirtualBox, you can optionally fill in the other sections.

### operating_system

Read [this operating_system section](launching-the-os-and-deploying-tasks.md#operating_system) first because the `operating_system` section for End-to-End testing is exactly the same.

### vbox

The vbox section has 6 required parameters:

- `"vboxmanage_exe_path"`: A path to the `VBoxManage.exe` executable.
- `"name"`: The name of the machine created during the [VirtualBox setup](qemu-virtualbox-setup.md#virtualbox)
- `"serial_logfile: path"`: The serial logfile path.
- `"pcap_file: path"`: The packet capture path.
- `"disk: path/actual_disk_start"`: See [this vbox section](launching-the-os-and-deploying-tasks.md#vbox)

### qemu

The qemu section has 5 required parameters:

- `"qemu_exe_path"`: A path to the `qemu-system-x86_64.exe` executable.
- `"cmd_params_path"`: The path to the qemu config file.
- `"serial_logfile: path"`: The serial logfile path.
- `"pcap_file: path"`: The packet capture path.
- `"disk: path"`: See [this qemu section](launching-the-os-and-deploying-tasks.md#qemu)

### required_testing_info

Here there are two important parameters. `"mac"` should be the MAC address of the virtual machine which you should remember from the [setup](qemu-virtualbox-setup.md). `"ip"` should be the IP of the computer running the tests, in the network where the virtual machine is running. Thus `"required_testing_info: ip"` differs from `"operating_system: ip"`, but they should be in the same network.

# Running End-to-End Tests

Go to the e2e testing folder and make sure all python dependencies are installed:

```
CPU_SCHEDULER_SERVICE/e2e_testing> pip install -r requirements.txt
```

To run tests for QEMU:

```
CPU_SCHEDULER_SERVICE/e2e_testing> python ./main.py --virtualizer=0
```

Or for VirtualBox:

```
CPU_SCHEDULER_SERVICE/e2e_testing> python ./main.py --virtualizer=1
```

