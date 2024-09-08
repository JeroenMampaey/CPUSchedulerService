#!/usr/bin/python3
import sys
from pathlib import Path
sys.path.append("../../python_lib")

from makefile_helper import MakefileHelper

from vm_tests import VmTests

import os
import yaml
import time

import asyncio
from aiocoap import Context, Message, GET, PUT, DELETE, ContentFormat

class TaskPrivilegesAndVirtSpaceTests(VmTests):
    OS_MGMT_PORT = 5683
    USER_TASK_PROCESS_ENTRY_ADDRESS = 0x2024000

    @classmethod
    def setUpClass(self) -> None:
        super().setUpClass()

        with open(Path(__file__).parent.absolute() / "../config.yaml", 'r') as file:
            config = yaml.safe_load(file)

        self.os_ip = config['operating_system']["ip"]

        for tasks_child in os.listdir(Path(__file__).parent.absolute() / "tasks"):
            if tasks_child!="link.ld":
                current_directory = os.path.dirname(os.path.abspath(__file__))
                makefile_helper = MakefileHelper(config['operating_system']["make_command"], os.path.join(current_directory, f"tasks/{tasks_child}"))
                makefile_helper.make_clean()
                makefile_helper.make()
    
    def setUp(self) -> None:
        super().setUp()
        
        self.vm.start()

        try:
            for line in self.vm.follow_logfile(marker="OSMgmt:"):
                if line!="OSMgmt: starting CoAPServer run()":
                    self.fail("First OSMgmt log entry was wrong")
                else:
                    break
        except TimeoutError as e:
            self.fail(f"Timeout while waiting for os management task log entry")
    
    def tearDown(self) -> None:
        self.vm.stop()

    def deploy_user_task(self, task_name: str, id: int, timeout: int = 30) -> bool:
        async def deploy_task(ip: str, port: int, image_path: str, id: int) -> bool:
            context = await Context.create_client_context()

            # Send the initial PUT request to deploy the task
            uri = f"coap://{ip}:{port}/tasks/{id}"
            request = Message(code=PUT, uri=uri)
            try:
                response = await asyncio.wait_for(context.request(request).response, timeout=timeout)
            except asyncio.TimeoutError:
                print(f"Request timed out for URI: {uri}")
                return False
            except Exception as e:
                print(f"Exception: {e}")
                return False

            # Check if task is already running
            uri = f"coap://{ip}:{port}/tasks/{id}/state"
            request = Message(code=GET, uri=uri)
            try:
                response = await asyncio.wait_for(context.request(request).response, timeout=timeout)
            except asyncio.TimeoutError:
                print(f"Request timed out for URI: {uri}")
                return False
            except Exception as e:
                print(f"Exception: {e}")
                return False

            if response.payload.decode() == "Running":
                return False

            # Upload the image file
            address = TaskPrivilegesAndVirtSpaceTests.USER_TASK_PROCESS_ENTRY_ADDRESS
            uri = f"coap://{ip}:{port}/tasks/{id}/data/{address}"
            with open(image_path, 'rb') as file:
                payload = file.read()

            request = Message(code=PUT, uri=uri, content_format=ContentFormat.OCTETSTREAM, payload=payload)
            try:
                response = await asyncio.wait_for(context.request(request, handle_blockwise=False).response, timeout=timeout)
            except asyncio.TimeoutError:
                print(f"Request timed out for URI: {uri}")
                return False
            except Exception as e:
                print(f"Exception: {e}")
                return False

            # Set the task state to "Running"
            uri = f"coap://{ip}:{port}/tasks/{id}/state"
            request = Message(code=PUT, uri=uri, content_format=ContentFormat.TEXT, payload=b"Running")
            try:
                response = await asyncio.wait_for(context.request(request).response, timeout=timeout)
            except asyncio.TimeoutError:
                print(f"Request timed out for URI: {uri}")
                return False
            except Exception as e:
                print(f"Exception: {e}")
                return False

            return True
        
        # Execute the task deployment
        result = asyncio.run(deploy_task(
            self.os_ip, 
            TaskPrivilegesAndVirtSpaceTests.OS_MGMT_PORT, 
            Path(__file__).parent.absolute() / f"tasks/{task_name}/task.bin", 
            id
        ))
        return result


    def remove_user_task(self, id: int) -> bool:
        async def remove_task(ip: str, port: int, id: int) -> bool:
            context = await Context.create_client_context()

            uri = f"coap://{ip}:{port}/tasks/{id}"
            request = Message(code=DELETE, uri=uri)
            try:
                response = await context.request(request).response
            except Exception as e:
                return False

            return True
        
        result = asyncio.run(remove_task(self.os_ip, TaskPrivilegesAndVirtSpaceTests.OS_MGMT_PORT, id))
        return result
        

    def test_user_task_attempting_priviliged_instructions_should_throw_general_protection_fault(self) -> None:        
        success = self.deploy_user_task("execute_privileged_ins_task", 1)
        if not success:
            self.fail("Failed to deploy task")

        try:
            for line in self.vm.follow_logfile(marker="Exception:"):
                self.assertEqual(line, "Exception: 13", "Expected a General Protection Fault exception but got another exception instead")
                break
        except TimeoutError as e:
            self.fail(f"Timout while waiting for exception to occur")
        
    def test_user_task_attempting_priviliged_instructions_should_throw_general_protection_fault2(self) -> None:        
        success = self.deploy_user_task("execute_privileged_ins_task2", 1)
        if not success:
            self.fail("Failed to deploy task")

        try:
            for line in self.vm.follow_logfile(marker="Exception:"):
                self.assertEqual(line, "Exception: 13", "Expected a General Protection Fault exception but got another exception instead")
                break
        except TimeoutError as e:
            self.fail(f"Timout while waiting for exception to occur")
    
    def test_user_task_attempting_to_access_kernel_space_should_throw_page_fault(self) -> None:        
        success = self.deploy_user_task("write_to_address_under_0x2000000_task", 1)
        if not success:
            self.fail("Failed to deploy task")

        accessed_address = 0
        try:
            for line in self.vm.follow_logfile(marker="LogInt:"):
                accessed_address = line.split(" ")[1]
                break
        except TimeoutError as e:
            self.fail(f"Timout while waiting for log entry to occur")
        
        try:
            for line in self.vm.follow_logfile(marker="Exception:"):
                self.assertEqual(line, "Exception: 14", f"Expected a Page Fault exception but got another exception instead, accessed address was {accessed_address}")
                break
        except TimeoutError as e:
            self.fail(f"Timout while waiting for exception to occur, accessed address was {accessed_address}")

    def test_user_task_attempting_to_access_private_paging_structures_or_kernel_stack_should_throw_page_fault(self) -> None:
        success = self.deploy_user_task("write_to_address_between_0x2000000_and_0x2014000_task", 1)
        if not success:
            self.fail("Failed to deploy task")

        accessed_address = 0
        try:
            for line in self.vm.follow_logfile(marker="LogInt:"):
                accessed_address = line.split(" ")[1]
                break
        except TimeoutError as e:
            self.fail(f"Timout while waiting for log entry to occur")
        
        try:
            for line in self.vm.follow_logfile(marker="Exception:"):
                self.assertEqual(line, "Exception: 14", f"Expected a Page Fault exception but got another exception instead, accessed address was {accessed_address}")
                break
        except TimeoutError as e:
            self.fail(f"Timout while waiting for exception to occur, accessed address was {accessed_address}")
        
    def test_user_task_attempting_to_access_user_space_should_not_throw_exceptions(self) -> None:
        success = self.deploy_user_task("write_to_address_between_0x2014000_and_0x2800000_task", 1)
        if not success:
            self.fail("Failed to deploy task")

        accessed_address = 0
        try:
            for line in self.vm.follow_logfile(marker="LogInt:"):
                accessed_address = line.split(" ")[1]
                break
        except TimeoutError as e:
            self.fail(f"Timout while waiting for log entry to occur")
        
        try:
            for line in self.vm.follow_logfile(marker="Exception:", timeout=5):
                self.fail(f"Exception was thrown by task which is unexepected, accessed address was {accessed_address}")
        except TimeoutError as e:
            pass
    
    def test_user_task_attempting_to_access_above_user_space_should_throw_page_fault(self) -> None:
        success = self.deploy_user_task("write_to_address_above_0x2800000_task", 1)
        if not success:
            self.fail("Failed to deploy task")

        accessed_address = 0
        try:
            for line in self.vm.follow_logfile(marker="LogInt:"):
                accessed_address = line.split(" ")[1]
                break
        except TimeoutError as e:
            self.fail(f"Timout while waiting for log entry to occur")

        try:
            for line in self.vm.follow_logfile(marker="Exception:"):
                self.assertEqual(line, "Exception: 14", f"Expected a Page Fault exception but got another exception instead, accessed address was {accessed_address}")
                break
        except TimeoutError as e:
            self.fail(f"Timout while waiting for exception to occur, accessed address was {accessed_address}")
    
    def test_tasks_writing_to_same_virtual_addresses_should_not_interface_in_physical_addr_space(self) -> None:
        success = self.deploy_user_task("clear_0x2200000_and_check_task", 1)
        if not success:
            self.fail("Failed to deploy task")
        success = self.deploy_user_task("set_0x2200000_and_check_task", 2)
        if not success:
            self.fail("Failed to deploy task")

        # First wait for logint 1001
        begin = time.perf_counter()
        while True:
            found_logentry = False

            try:
                for line in self.vm.follow_logfile(marker="LogInt:"):
                    logintValue = line.split(" ")[1]
                    if logintValue == "1001":
                        found_logentry = True
                        break
            except TimeoutError as e:
                self.fail(f"Timout while waiting for log entry to occur")
            
            if found_logentry:
                break

            end = time.perf_counter()
            self.assertTrue((end-begin) <= 5, "Waiting for 'Logint: 1001' timed out after 5 seconds")
        
        # Then wait for logint 2001
        begin = time.perf_counter()
        while True:
            found_logentry = False

            try:
                for line in self.vm.follow_logfile(marker="LogInt:"):
                    logintValue = line.split(" ")[1]
                    if logintValue == "2001":
                        found_logentry = True
                        break
            except TimeoutError as e:
                self.fail(f"Timout while waiting for log entry to occur")
            
            if found_logentry:
                break

            end = time.perf_counter()
            self.assertTrue((end-begin) <= 5, "Waiting for 'Logint: 2001' timed out after 5 seconds")

        # Now check that values alternate between 1001 and 2001
        try:
            for line in self.vm.follow_logfile(marker="LogInt:"):
                logintValue = line.split(" ")[1]
                if logintValue == "1001":
                    break
                else:
                    self.fail(f"Expected value 1001 but got {logintValue}")
            for line in self.vm.follow_logfile(marker="LogInt:"):
                logintValue = line.split(" ")[1]
                if logintValue == "2001":
                    break
                else:
                    self.fail(f"Expected value 2001 but got {logintValue}")
            for line in self.vm.follow_logfile(marker="LogInt:"):
                logintValue = line.split(" ")[1]
                if logintValue == "1001":
                    break
                else:
                    self.fail(f"Expected value 1001 but got {logintValue}")
            for line in self.vm.follow_logfile(marker="LogInt:"):
                logintValue = line.split(" ")[1]
                if logintValue == "2001":
                    break
                else:
                    self.fail(f"Expected value 2001 but got {logintValue}")
        except TimeoutError as e:
            self.fail(f"Timout while waiting for log entry to occur")
        
        self.remove_user_task(1)
        self.remove_user_task(2)

        success = self.deploy_user_task("clear_0x2500000_and_check_task", 1)
        if not success:
            self.fail("Failed to deploy task")
        success = self.deploy_user_task("set_0x2500000_and_check_task", 2)
        if not success:
            self.fail("Failed to deploy task")

        # First wait for logint 3001
        begin = time.perf_counter()
        while True:
            found_logentry = False

            try:
                for line in self.vm.follow_logfile(marker="LogInt:"):
                    logintValue = line.split(" ")[1]
                    if logintValue == "3001":
                        found_logentry = True
                        break
            except TimeoutError as e:
                self.fail(f"Timout while waiting for log entry to occur")
            
            if found_logentry:
                break

            end = time.perf_counter()
            self.assertTrue((end-begin) <= 5, "Waiting for 'Logint: 3001' timed out after 5 seconds")
        
        # Then wait for logint 4001
        begin = time.perf_counter()
        while True:
            found_logentry = False

            try:
                for line in self.vm.follow_logfile(marker="LogInt:"):
                    logintValue = line.split(" ")[1]
                    if logintValue == "4001":
                        found_logentry = True
                        break
            except TimeoutError as e:
                self.fail(f"Timout while waiting for log entry to occur")
            
            if found_logentry:
                break

            end = time.perf_counter()
            self.assertTrue((end-begin) <= 5, "Waiting for 'Logint: 4001' timed out after 5 seconds")
        
        # Now check that values alternate between 3001 and 4001
        try:
            for line in self.vm.follow_logfile(marker="LogInt:"):
                logintValue = line.split(" ")[1]
                if logintValue == "3001":
                    break
                else:
                    self.fail(f"Expected value 3001 but got {logintValue}")
            for line in self.vm.follow_logfile(marker="LogInt:"):
                logintValue = line.split(" ")[1]
                if logintValue == "4001":
                    break
                else:
                    self.fail(f"Expected value 4001 but got {logintValue}")
            for line in self.vm.follow_logfile(marker="LogInt:"):
                logintValue = line.split(" ")[1]
                if logintValue == "3001":
                    break
                else:
                    self.fail(f"Expected value 3001 but got {logintValue}")
            for line in self.vm.follow_logfile(marker="LogInt:"):
                logintValue = line.split(" ")[1]
                if logintValue == "4001":
                    break
                else:
                    self.fail(f"Expected value 4001 but got {logintValue}")
        except TimeoutError as e:
            self.fail(f"Timout while waiting for log entry to occur")