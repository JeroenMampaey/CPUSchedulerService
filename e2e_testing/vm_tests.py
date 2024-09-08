import unittest
import sys

from qemu_vm import QemuVm
from vbox_vm import VboxVm

from optparse import OptionParser

import yaml
from pathlib import Path

class VmTests(unittest.TestCase):
    @classmethod
    def setUpClass(self) -> None:
        with open(Path(__file__).parent.absolute() / "config.yaml", 'r') as file:
            config = yaml.safe_load(file)
        
        parser = OptionParser()
        parser.add_option("--virtualizer", dest="virtualizer", help="0 for QEMU, 1 for VirtualBox", type="choice", 
            default="0", choices=["0", "1"])
        options, _ = parser.parse_args(sys.argv)
        virtualizerOption = int(options.virtualizer)

        self.vm = None

        if virtualizerOption==0:
            self.vm = QemuVm(config['qemu']['disk']['path'],
                config['qemu']['qemu_exe_path'],
                config['qemu']['cmd_params_path'],
                config['qemu']['serial_logfile']['path'],
                config['qemu']['pcap_file']['path'])
        elif virtualizerOption==1:
            self.vm = VboxVm(config['vbox']['disk']['path'],
                config['vbox']['disk']['actual_disk_start'],
                config['vbox']['vboxmanage_exe_path'],
                config['vbox']['name'],
                config['vbox']['serial_logfile']['path'],
                config['vbox']['pcap_file']['path'])
        else:
            print(f"Virtualizer param in setup_operating_system had some unexpected value: {virtualizerOption}")
            exit(1)
    
    def setUp(self) -> None:
        print("----------------------------------------------------------------------")
        print(f"Running test: {self.id()}")
        print("----------------------------------------------------------------------")