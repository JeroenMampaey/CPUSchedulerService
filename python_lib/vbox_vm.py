import subprocess

import time

from vm import Vm

class VboxVm(Vm):
    def __init__(self, disk_path: str, disk_start: int, vboxmanage_exe_path: str = None, name: str = None, serial_logfile_path: str = None, pcap_file_path: str = None) -> None:
        super().__init__(disk_path, disk_start, serial_logfile_path, pcap_file_path)

        self.vboxmanage_exe_path = vboxmanage_exe_path
        self.name = name

    def start(self) -> None:
        if self.vboxmanage_exe_path is None or self.name is None:
            raise Exception("vboxmanage_exe_path and/or name is not set")
    
        super().start()

        # Start the VM
        start_vm_command = subprocess.Popen([self.vboxmanage_exe_path, "startvm", self.name])
        start_vm_command.wait()
    
    def stop(self) -> None:
        if self.vboxmanage_exe_path is None or self.name is None:
            raise Exception("vboxmanage_exe_path and/or name is not set")

        super().stop()

        # Stop the VM
        stop_vm_command = subprocess.Popen([self.vboxmanage_exe_path, "controlvm", self.name, "poweroff"])
        stop_vm_command.wait()

        # Wait for VM to completely shut down
        # For some reason, command exits before machine is fully unlocked, it's not so obvious how to wait for full shutdown
        # ("not so obvious", a.k.a: tried some things -> didn't work -> gave up)
        time.sleep(2)