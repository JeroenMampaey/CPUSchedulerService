import subprocess

import time

from vm import Vm

import signal

import re

class QemuVm(Vm):
    def __init__(self, disk_path: str, qemu_exe_path: str = None, cmd_params_path: str = None, serial_logfile_path: str = None, pcap_file_path: str = None) -> None:
        super().__init__(disk_path, 0, serial_logfile_path, pcap_file_path)

        self.qemu_exe_path = qemu_exe_path

        if cmd_params_path is not None:
            with open(cmd_params_path, "r") as file:
                self.cmd_params = file.read().splitlines()
        else:
            self.cmd_params = None

        self.vm_process = None

    def start(self) -> None:
        if self.qemu_exe_path is None or self.cmd_params is None:
            raise Exception("qemu_exe_path and/or cmd_params is not set")
    
        super().start()

        # Start the VM
        self.vm_process = subprocess.Popen([self.qemu_exe_path]+self.cmd_params, stdin=subprocess.PIPE)
    
    def stop(self) -> None:
        if self.qemu_exe_path is None or self.cmd_params is None:
            raise Exception("qemu_exe_path and/or cmd_params is not set")

        super().stop()

        # Stop the VM
        self.vm_process.terminate()

        # Wait for VM to completely shut down
        time.sleep(0.5)