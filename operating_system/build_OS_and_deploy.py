#!/usr/bin/python3
import sys
from pathlib import Path
sys.path.append("../python_lib")

from vbox_vm import VboxVm
from qemu_vm import QemuVm
from makefile_helper import MakefileHelper

import yaml
import os

from optparse import OptionParser

def setup_operating_system(virtualizerOption: int) -> None:
    with open(Path(__file__).parent.absolute() / "config.yaml", 'r') as file:
        config = yaml.safe_load(file)
    
    current_directory = os.path.dirname(os.path.abspath(__file__))
    makefile_helper = MakefileHelper(config['operating_system']["make_command"], current_directory)
    makefile_helper.make_clean()

    ip_string = config['operating_system']["ip"]
    gateway_ip_string = config['operating_system']["gateway_ip"]
    ip = sum([int(octet_string)*(2**(24-8*i)) for i, octet_string in enumerate(ip_string.split("."))])
    gateway_ip = sum([int(octet_string)*(2**(24-8*i)) for i, octet_string in enumerate(gateway_ip_string.split("."))])
    netmask = config['operating_system']["netmask"]

    makefile_helper.make(f"CPPFLAGS=-DTHIS_IP={ip} -DGATEWAY_IP={gateway_ip} -DNETWORK_MASK={netmask}")

    vm = None
    
    if virtualizerOption==0:
        vm = QemuVm(config['qemu']['disk']['path'])
    elif virtualizerOption==1:
        vm = VboxVm(config['vbox']['disk']['path'],
            config['vbox']['disk']['actual_disk_start'])
    else:
        print(f"Virtualizer param in setup_operating_system had some unexpected value: {virtualizerOption}")
        exit(1)
    
    vm.copy_image_to_disk(os.path.join(current_directory, 'os_image.bin'), 0)

def main():
    parser = OptionParser()
    parser.add_option("--virtualizer", dest="virtualizer", help="0 for QEMU, 1 for VirtualBox", type="choice", 
        default="0", choices=["0", "1"])
    options, _ = parser.parse_args(sys.argv)
    virtualizerOption = int(options.virtualizer)

    setup_operating_system(virtualizerOption)

if __name__=='__main__':
    main()