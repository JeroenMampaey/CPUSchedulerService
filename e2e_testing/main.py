#!/usr/bin/python3
import sys
from pathlib import Path
sys.path.append("../python_lib")

import unittest
from unittest.runner import TextTestResult

import yaml

from vbox_vm import VboxVm
from qemu_vm import QemuVm
from makefile_helper import MakefileHelper

from basic_tests.basic_tests import BasicTests
from kernel_memory_manager_tests.kernel_memory_manager_tests import KernelMemoryManagerTests
from network_tests.network_tests import NetworkTests
from task_privileges_and_virt_space_tests.task_privileges_and_virt_space_tests import TaskPrivilegesAndVirtSpaceTests
from os_management_task_tests.os_management_task_tests import OSManagementTaskTests
from syscall_tests.syscall_tests import SyscallTests

import os
from colorama import Fore, Back, Style

from optparse import OptionParser

import time

class CustomTextTestResult(TextTestResult):
    def __init__(self, *args, **kwargs):
        super(CustomTextTestResult, self).__init__(*args, **kwargs)
        self.successes = []
    def addSuccess(self, test) -> None:
        super(CustomTextTestResult, self).addSuccess(test)
        self.successes.append(test)

def setup_operating_system(virtualizerOption: int) -> None:
    with open(Path(__file__).parent.absolute() / "config.yaml", 'r') as file:
        config = yaml.safe_load(file)
    
    vm = None

    if virtualizerOption==0:
        # TODO: sometimes tests here fail also:
        #
        # test_tasks_send_packets_to_eachother_should_be_possible
        # test_tasks_throwing_exceptions_shouldnt_halt_the_operating_system
        vm = QemuVm(config['qemu']['disk']['path'],
            config['qemu']['qemu_exe_path'],
            config['qemu']['cmd_params_path'],
            config['qemu']['serial_logfile']['path'],
            config['qemu']['pcap_file']['path'])
    elif virtualizerOption==1:
        # TODO: investigate issue with VirtualBox and remove this or explain here why
        # Things which tend to fail with VirtualBox:
        #  - Network tests in general
        #  - Just task switching is general used to fail with VirtualBox also, but not reproducable anymore
        #
        # test_tasks_throwing_exceptions_shouldnt_halt_the_operating_system
        # test_task_sends_simple_packet_should_arrive_correctly
        # test_tasks_send_packets_to_eachother_should_be_possible
        # test_user_task_attempting_to_access_private_paging_structures_or_kernel_stack_should_throw_page_fault
        # test_task_sends_two_packets_should_be_handled_during_a_single_timeslot
        print(Fore.YELLOW+"----------------------------------------------------------------------")
        print("Warning: TestSuite does not always work with VirtualBox!!")
        print("----------------------------------------------------------------------"+Style.RESET_ALL)
        time.sleep(2)
        vm = VboxVm(config['vbox']['disk']['path'],
            config['vbox']['disk']['actual_disk_start'],
            config['vbox']['vboxmanage_exe_path'],
            config['vbox']['name'],
            config['vbox']['serial_logfile']['path'],
            config['vbox']['pcap_file']['path'])
    else:
        print(f"Virtualizer param in setup_operating_system had some unexpected value: {virtualizerOption}")
        exit(1)

    current_directory = os.path.dirname(os.path.abspath(__file__))
    makefile_helper = MakefileHelper(config['operating_system']["make_command"], os.path.join(current_directory, '../operating_system'))
    makefile_helper.make_clean()

    ip_string = config['operating_system']["ip"]
    gateway_ip_string = config['operating_system']["gateway_ip"]
    ip = sum([int(octet_string)*(2**(24-8*i)) for i, octet_string in enumerate(ip_string.split("."))])
    gateway_ip = sum([int(octet_string)*(2**(24-8*i)) for i, octet_string in enumerate(gateway_ip_string.split("."))])
    netmask = config['operating_system']["netmask"]
    client_ip_string = config['required_testing_info']["client_ip"]
    client_ip = sum([int(octet_string)*(2**(24-8*i)) for i, octet_string in enumerate(client_ip_string.split("."))])
    
    makefile_helper.make(f"CPPFLAGS=-DE2E_TESTING=1 -DTHIS_IP={ip} -DGATEWAY_IP={gateway_ip} -DNETWORK_MASK={netmask} -DTEST_CLIENT_IP={client_ip}")

    vm.copy_image_to_disk(os.path.join(current_directory, '../operating_system/os_image.bin'), 0)

def main():
    parser = OptionParser()
    parser.add_option("--virtualizer", dest="virtualizer", help="0 for QEMU, 1 for VirtualBox", type="choice", 
        default="0", choices=["0", "1"])
    options, _ = parser.parse_args(sys.argv)
    virtualizerOption = int(options.virtualizer)

    setup_operating_system(virtualizerOption)
    
    test_suite = unittest.TestSuite()
    loader = unittest.TestLoader()
    test_suite.addTest(loader.loadTestsFromTestCase(BasicTests))
    test_suite.addTest(loader.loadTestsFromTestCase(KernelMemoryManagerTests))
    if virtualizerOption!=1:
        test_suite.addTest(loader.loadTestsFromTestCase(NetworkTests))
        pass
    else:
        print(Fore.YELLOW+"----------------------------------------------------------------------")
        print("Warning: Since VirtualBox is being used, NetworkTests are excluded!!")
        print("----------------------------------------------------------------------"+Style.RESET_ALL)
        time.sleep(5)
    test_suite.addTest(loader.loadTestsFromTestCase(OSManagementTaskTests))
    test_suite.addTest(loader.loadTestsFromTestCase(SyscallTests))
    test_suite.addTest(loader.loadTestsFromTestCase(TaskPrivilegesAndVirtSpaceTests))
    # Example on how to run a single test:
    #test_suite.addTest(SyscallTests("test_setting_send_buffer_to_nullptr_should_succeed_if_size_zero_and_indicator_nullptr"))

    runner = unittest.TextTestRunner(resultclass=CustomTextTestResult)
    result = runner.run(test_suite)

    # Show the results in a clear manner
    print(f"{len(result.successes)}/{len(result.successes)+len(result.errors)+len(result.failures)} tests succeeded!")
    print(f"{len(result.errors)+len(result.failures)}/{len(result.successes)+len(result.errors)+len(result.failures)} tests failed!")
    print(Fore.GREEN+"----------------------------------------------------------------------")
    print("Successes:")
    print("----------------------------------------------------------------------"+Style.RESET_ALL)
    for t in result.successes:
        print(Fore.GREEN+"----------------------------------------------------------------------"+Style.RESET_ALL)
        print(t)
        print(Fore.GREEN+"----------------------------------------------------------------------"+Style.RESET_ALL)
    print(Fore.YELLOW+"----------------------------------------------------------------------")
    print("Errors:")
    print("----------------------------------------------------------------------"+Style.RESET_ALL)
    for t in result.errors:
        print(Fore.YELLOW+"----------------------------------------------------------------------"+Style.RESET_ALL)
        print(t[0])
        print(t[1])
        print(Fore.YELLOW+"----------------------------------------------------------------------"+Style.RESET_ALL)
    print(Fore.RED+"----------------------------------------------------------------------")
    print("Failures:")
    print("----------------------------------------------------------------------"+Style.RESET_ALL)
    for t in result.failures:
        print(Fore.RED+"----------------------------------------------------------------------"+Style.RESET_ALL)
        print(t[0])
        print(t[1])
        print(Fore.RED+"----------------------------------------------------------------------"+Style.RESET_ALL)

if __name__ == '__main__':
    main()