#!/usr/bin/python3
import sys
from pathlib import Path
sys.path.append("../../python_lib")

from makefile_helper import MakefileHelper

from vm_tests import VmTests

import os
import yaml
import time
import random

import asyncio
from aiocoap import Context, Message, GET, PUT, DELETE, ContentFormat

from enum import Enum

class OSMgmtResponse:
    def __init__(self, timed_out: bool, response: Message) -> None:
        self.timed_out = timed_out
        self.response = response

class OSManagementTaskTests(VmTests):
    OS_MGMT_PORT = 5683
    USER_TASK_PROCESS_ENTRY_ADDRESS = 0x2024000

    DELETED_REPONSE_CODE = 0x42
    CREATED_RESPONSE_CODE = 0x41
    CHANGED_RESPONSE_CODE = 0x44
    CONTENT_RESPONSE_CODE = 0x45
    FORBIDDEN_RESPONSE_CODE = 0x83

    MAX_RUNNING_TASKS = (5+2)
    MAX_TASKS = 2*MAX_RUNNING_TASKS

    COAP_TIMEOUT = 30

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
    
    async def coap_request_with_timeout(self, request: Message, context: Context, timeout: float):
        try:
            response = await asyncio.wait_for(context.request(request, handle_blockwise=False).response, timeout=timeout)
            return OSMgmtResponse(False, response)
        except asyncio.TimeoutError:
            return OSMgmtResponse(True, None)
        except Exception:
            return OSMgmtResponse(True, None)

    def create_new_task(self, id: int) -> OSMgmtResponse:
        async def add_task(ip: str, port: int, id: int) -> OSMgmtResponse:
            context = await Context.create_client_context()
            uri = f"coap://{ip}:{port}/tasks/{id}"
            request = Message(code=PUT, uri=uri)
            return await self.coap_request_with_timeout(request, context, OSManagementTaskTests.COAP_TIMEOUT)

        return asyncio.run(add_task(self.os_ip, OSManagementTaskTests.OS_MGMT_PORT, id))

    def set_task_code(self, task_name: str, id: int) -> OSMgmtResponse:
        async def set_task_image(ip: str, port: int, image_path: str, id: int) -> OSMgmtResponse:
            context = await Context.create_client_context()
            address = OSManagementTaskTests.USER_TASK_PROCESS_ENTRY_ADDRESS
            uri = f"coap://{ip}:{port}/tasks/{id}/data/{address}"
            with open(image_path, 'rb') as file:
                payload = file.read()
            request = Message(code=PUT, uri=uri, content_format=ContentFormat.OCTETSTREAM, payload=payload)
            return await self.coap_request_with_timeout(request, context, OSManagementTaskTests.COAP_TIMEOUT)

        image_path = Path(__file__).parent.absolute() / f"tasks/{task_name}/task.bin"
        return asyncio.run(set_task_image(self.os_ip, OSManagementTaskTests.OS_MGMT_PORT, image_path, id))

    def start_user_task(self, id: int) -> OSMgmtResponse:
        async def start_task(ip: str, port: int, id: int) -> OSMgmtResponse:
            context = await Context.create_client_context()
            uri = f"coap://{ip}:{port}/tasks/{id}/state"
            request = Message(code=PUT, uri=uri, content_format=ContentFormat.TEXT, payload=b"Running")
            return await self.coap_request_with_timeout(request, context, OSManagementTaskTests.COAP_TIMEOUT)

        return asyncio.run(start_task(self.os_ip, OSManagementTaskTests.OS_MGMT_PORT, id))

    def check_task_state(self, id: int) -> OSMgmtResponse:
        async def check_task_state(ip: str, port: int, id: int) -> OSMgmtResponse:
            context = await Context.create_client_context()
            uri = f"coap://{ip}:{port}/tasks/{id}/state"
            request = Message(code=GET, uri=uri)
            return await self.coap_request_with_timeout(request, context, OSManagementTaskTests.COAP_TIMEOUT)

        return asyncio.run(check_task_state(self.os_ip, OSManagementTaskTests.OS_MGMT_PORT, id))

    def remove_user_task(self, id: int) -> OSMgmtResponse:
        async def remove_task(ip: str, port: int, id: int) -> OSMgmtResponse:
            context = await Context.create_client_context()
            uri = f"coap://{ip}:{port}/tasks/{id}"
            request = Message(code=DELETE, uri=uri)
            return await self.coap_request_with_timeout(request, context, OSManagementTaskTests.COAP_TIMEOUT)

        return asyncio.run(remove_task(self.os_ip, OSManagementTaskTests.OS_MGMT_PORT, id))
    
    def test_adding_and_removing_two_tasks_should_behave_normally(self) -> None:
        # First, register task 1
        self.create_new_task(1)
        self.set_task_code("logint_1_task", 1)
        self.start_user_task(1)

        # Check if task 1 is running
        begin = time.perf_counter()
        while True:
            found_logentry = False

            try:
                for line in self.vm.follow_logfile(marker="LogInt:"):
                    logintValue = line.split(" ")[1]
                    if logintValue == "1":
                        found_logentry = True
                        break
            except TimeoutError as e:
                self.fail(f"Timout while waiting for log entry to occur")
            
            if found_logentry:
                break

            end = time.perf_counter()
            self.assertTrue((end-begin) <= 5, "Waiting for 'Logint: 1' timed out after 5 seconds")
        
        # Now check that LogInt: 10 from the main does not occur anymore
        begin = time.perf_counter()
        while True:
            try:
                for line in self.vm.follow_logfile(marker="LogInt:"):
                    logintValue = line.split(" ")[1]
                    self.assertEqual(logintValue, "1", "LogInt: 1 should be the only log entry but another log entry was found")
                    break
            except TimeoutError as e:
                self.fail(f"Timout while waiting for log entry to occur")
            
            end = time.perf_counter()
            if (end-begin) > 5:
                break
        
        # Now, register task 2
        self.create_new_task(2)
        self.set_task_code("logint_2_task", 2)
        self.start_user_task(2)

        # Check if task 2 is running
        begin = time.perf_counter()
        while True:
            found_logentry = False

            try:
                for line in self.vm.follow_logfile(marker="LogInt:"):
                    logintValue = line.split(" ")[1]
                    if logintValue == "2":
                        found_logentry = True
                        break
            except TimeoutError as e:
                self.fail(f"Timout while waiting for log entry to occur")
            
            if found_logentry:
                break

            end = time.perf_counter()
            self.assertTrue((end-begin) <= 5, "Waiting for 'Logint: 2' timed out after 5 seconds")
        
        # Now check that task 1 and task 2 alternate
        try:
            for line in self.vm.follow_logfile(marker="LogInt:"):
                logintValue = line.split(" ")[1]
                self.assertEqual(logintValue, "1", f"Expected value 1 but got {logintValue}")
                break
            for line in self.vm.follow_logfile(marker="LogInt:"):
                logintValue = line.split(" ")[1]
                self.assertEqual(logintValue, "2", f"Expected value 2 but got {logintValue}")
                break
            for line in self.vm.follow_logfile(marker="LogInt:"):
                logintValue = line.split(" ")[1]
                self.assertEqual(logintValue, "1", f"Expected value 1 but got {logintValue}")
                break
            for line in self.vm.follow_logfile(marker="LogInt:"):
                logintValue = line.split(" ")[1]
                self.assertEqual(logintValue, "2", f"Expected value 2 but got {logintValue}")
                break
        except TimeoutError as e:
            self.fail(f"Timout while waiting for log entry to occur")
        
        # Now remove task 1
        self.remove_user_task(1)

        # Check that only task 2 remains, by checking that it runs atleast 5 times in a row
        begin = time.perf_counter()
        while True:
            logint2_occured_5_times_in_a_row = True
            for i in range(5):
                try:
                    for line in self.vm.follow_logfile(marker="LogInt:"):
                        logintValue = line.split(" ")[1]
                        if logintValue != "2":
                            logint2_occured_5_times_in_a_row = False
                        break
                except TimeoutError as e:
                    self.fail(f"Timout while waiting for log entry to occur")
                
                if not logint2_occured_5_times_in_a_row:
                    break
            
            if logint2_occured_5_times_in_a_row:
                break
            
            self.assertTrue((end-begin) <= 5, "Waiting for 'Logint: 2' to occur 5 times in a row timed out after 5 seconds")

        # Now remove task 2
        self.remove_user_task(2)

        # Now add task 1 again
        self.create_new_task(1)
        self.set_task_code("logint_1_task", 1)
        self.start_user_task(1)

        # Check that only task 1 is running
        begin = time.perf_counter()
        while True:
            logint1_occured_5_times_in_a_row = True
            for i in range(5):
                try:
                    for line in self.vm.follow_logfile(marker="LogInt:"):
                        logintValue = line.split(" ")[1]
                        if logintValue != "1":
                            logint1_occured_5_times_in_a_row = False
                        break
                except TimeoutError as e:
                    self.fail(f"Timout while waiting for log entry to occur")
                
                if not logint1_occured_5_times_in_a_row:
                    break
            
            if logint1_occured_5_times_in_a_row:
                break
            
            self.assertTrue((end-begin) <= 5, "Waiting for 'Logint: 1' to occur 5 times in a row timed out after 5 seconds")
        
    def test_attempting_to_run_more_than_MAX_RUNNING_TASKS_tasks_should_throw_forbidden_response(self) -> None:
        # First make sure MAX_RUNNING_TASKS are running
        for i in range(2, OSManagementTaskTests.MAX_RUNNING_TASKS):
            self.create_new_task(i)
            self.set_task_code("do_nothing_task", i)
            response = self.start_user_task(i)
            self.assertEqual(response.response.code, OSManagementTaskTests.CHANGED_RESPONSE_CODE, f"Failed to start task {i}")
        
        # Now try to start another task
        self.create_new_task(OSManagementTaskTests.MAX_RUNNING_TASKS)
        self.set_task_code("do_nothing_task", OSManagementTaskTests.MAX_RUNNING_TASKS)
        response = self.start_user_task(OSManagementTaskTests.MAX_RUNNING_TASKS)
        self.assertEqual(response.response.code, OSManagementTaskTests.FORBIDDEN_RESPONSE_CODE, f"Expected {OSManagementTaskTests.FORBIDDEN_RESPONSE_CODE} but got {response.response.code}")

        # Now delete a task and try again to start another one
        random_task_id = random.randint(2, OSManagementTaskTests.MAX_RUNNING_TASKS-1) 
        self.remove_user_task(random_task_id)
        response = self.start_user_task(OSManagementTaskTests.MAX_RUNNING_TASKS)
        self.assertEqual(response.response.code, OSManagementTaskTests.CHANGED_RESPONSE_CODE, f"Failed to start task {OSManagementTaskTests.MAX_RUNNING_TASKS}")

        # Now check that starting another one should once again fail
        self.create_new_task(OSManagementTaskTests.MAX_RUNNING_TASKS+1)
        self.set_task_code("do_nothing_task", OSManagementTaskTests.MAX_RUNNING_TASKS+1)
        response = self.start_user_task(OSManagementTaskTests.MAX_RUNNING_TASKS+1)
        self.assertEqual(response.response.code, OSManagementTaskTests.FORBIDDEN_RESPONSE_CODE, f"Expected {OSManagementTaskTests.FORBIDDEN_RESPONSE_CODE} but got {response.response.code}")

    def test_attempting_to_create_more_than_MAX_TASKS_tasks_should_throw_forbidden_response(self) -> None:
        # First make sure MAX_TASKS are created
        for i in range(1, OSManagementTaskTests.MAX_TASKS+1):
            response = self.create_new_task(i)
            self.assertEqual(response.response.code, OSManagementTaskTests.CREATED_RESPONSE_CODE, f"Failed to create task {i}")
        
        # Now try to create another task
        response = self.create_new_task(OSManagementTaskTests.MAX_TASKS+1)
        self.assertEqual(response.response.code, OSManagementTaskTests.FORBIDDEN_RESPONSE_CODE, f"Expected {OSManagementTaskTests.FORBIDDEN_RESPONSE_CODE} but got {response.response.code}")

        # Now delete a task and try again to create another one
        random_task_id = random.randint(1, OSManagementTaskTests.MAX_TASKS)
        self.remove_user_task(random_task_id)
        response = self.create_new_task(OSManagementTaskTests.MAX_TASKS+1)
        self.assertEqual(response.response.code, OSManagementTaskTests.CREATED_RESPONSE_CODE, f"Failed to create task {OSManagementTaskTests.MAX_TASKS+1}")

        # Now check that creating another one should once again fail
        response = self.create_new_task(OSManagementTaskTests.MAX_TASKS+2)
        self.assertEqual(response.response.code, OSManagementTaskTests.FORBIDDEN_RESPONSE_CODE, f"Expected {OSManagementTaskTests.FORBIDDEN_RESPONSE_CODE} but got {response.response.code}")

    def test_tasks_throwing_exceptions_shouldnt_halt_the_operating_system(self) -> None:
        # Create task throwing general protection fault
        self.create_new_task(1)
        self.set_task_code("general_protection_fault_task", 1)
        self.start_user_task(1)
        
        # Check that the task is running
        try:
            for line in self.vm.follow_logfile(marker="Exception:"):
                exceptionValue = line.split(" ")[1]
                self.assertEqual(exceptionValue, "13", f"Expected value 13 but got {exceptionValue}")
                break
        except TimeoutError as e:
            self.fail(f"Timout while waiting for exception entry to occur")
        
        # Create task throwing page fault exception
        self.create_new_task(2)
        self.set_task_code("page_fault_task", 2)
        self.start_user_task(2)

        # Check that the task is running
        begin = time.perf_counter()
        while True:
            found_exceptionentry = False

            try:
                for line in self.vm.follow_logfile(marker="Exception:"):
                    exceptionValue = line.split(" ")[1]
                    if exceptionValue == "14":
                        found_exceptionentry = True
                        break
            except TimeoutError as e:
                self.fail(f"Timout while waiting for exception entry to occur")
            
            if found_exceptionentry:
                break

            end = time.perf_counter()
            self.assertTrue((end-begin) <= 5, "Waiting for 'Exception: 14' timed out after 5 seconds")
        
        # Create task logging LogInt: 1
        self.create_new_task(3)
        self.set_task_code("logint_1_task", 3)
        self.start_user_task(3)

        # Check that the task is running
        try:
            for line in self.vm.follow_logfile(marker="LogInt:"):
                logintValue = line.split(" ")[1]
                self.assertEqual(logintValue, "1", f"Expected value 1 but got {logintValue}")
                break
        except TimeoutError as e:
            self.fail(f"Timout while waiting for log entry to occur")
        
        # Now check that tasks alternate LogInt: 1 -> Exception: 14 -> Exception: 13
        try:
            for line in self.vm.follow_logfile(marker="Exception:"):
                exceptionValue = line.split(" ")[1]
                self.assertEqual(exceptionValue, "14", f"Expected value 14 but got {exceptionValue}")
                break
            for line in self.vm.follow_logfile(marker="Exception:"):
                exceptionValue = line.split(" ")[1]
                self.assertEqual(exceptionValue, "13", f"Expected value 13 but got {exceptionValue}")
                break
            for line in self.vm.follow_logfile(marker="LogInt:"):
                logintValue = line.split(" ")[1]
                self.assertEqual(logintValue, "1", f"Expected value 1 but got {logintValue}")
                break
            for line in self.vm.follow_logfile(marker="Exception:"):
                exceptionValue = line.split(" ")[1]
                self.assertEqual(exceptionValue, "14", f"Expected value 14 but got {exceptionValue}")
                break
            for line in self.vm.follow_logfile(marker="Exception:"):
                exceptionValue = line.split(" ")[1]
                self.assertEqual(exceptionValue, "13", f"Expected value 13 but got {exceptionValue}")
                break
            for line in self.vm.follow_logfile(marker="LogInt:"):
                logintValue = line.split(" ")[1]
                self.assertEqual(logintValue, "1", f"Expected value 1 but got {logintValue}")
                break
        except TimeoutError as e:
            self.fail(f"Timout while waiting for log entry or exception entry to occur")

    def test_removing_task_should_close_all_opened_sockets(self) -> None:
        # Create tasks which opens some sockets
        self.create_new_task(1)
        self.set_task_code("open_sockets_task", 1)
        self.start_user_task(1)

        # Check if all sockets opened successfully
        try:
            for line in self.vm.follow_logfile(marker="LogInt:"):
                logintValue = line.split(" ")[1]
                if logintValue == "123":
                    break
        except TimeoutError as e:
            self.fail(f"Timout while waiting for log entry to occur")

        # Add another tasks attempting to open the same sockets
        self.create_new_task(2)
        self.set_task_code("open_sockets_task", 2)
        self.start_user_task(2)

        # Check that opening the sockets failed for task 2
        try:
            for line in self.vm.follow_logfile(marker="LogInt:"):
                logintValue = line.split(" ")[1]
                self.assertEqual(logintValue, "456", f"Expected value 456 but got {logintValue}")
                break
        except TimeoutError as e:
            self.fail(f"Timout while waiting for log entry to occur")
        
        # Remove task 1
        self.remove_user_task(1)

        # Create once again a task opening the same sockets as before
        self.create_new_task(3)
        self.set_task_code("open_sockets_task", 3)
        self.start_user_task(3)
        
        # Check that opening the sockets succeeded for task 3
        try:
            for line in self.vm.follow_logfile(marker="LogInt:"):
                logintValue = line.split(" ")[1]
                self.assertEqual(logintValue, "123", f"Expected value 123 but got {logintValue}")
                break
        except TimeoutError as e:
            self.fail(f"Timout while waiting for log entry to occur")
    
    def test_tasks_without_yield_should_still_switch_correctly(self) -> None:
                # First, register task 1
        self.create_new_task(1)
        self.set_task_code("logint_1_task_no_yield", 1)
        self.start_user_task(1)

        # Check if task 1 is running
        begin = time.perf_counter()
        while True:
            found_logentry = False

            try:
                for line in self.vm.follow_logfile(marker="LogInt:"):
                    logintValue = line.split(" ")[1]
                    if logintValue == "1":
                        found_logentry = True
                        break
            except TimeoutError as e:
                self.fail(f"Timout while waiting for log entry to occur")
            
            if found_logentry:
                break

            end = time.perf_counter()
            self.assertTrue((end-begin) <= 5, "Waiting for 'Logint: 1' timed out after 5 seconds")
        
        # Now check that LogInt: 10 from the main does not occur anymore
        begin = time.perf_counter()
        while True:
            try:
                for line in self.vm.follow_logfile(marker="LogInt:"):
                    logintValue = line.split(" ")[1]
                    self.assertEqual(logintValue, "1", "LogInt: 1 should be the only log entry but another log entry was found")
                    break
            except TimeoutError as e:
                self.fail(f"Timout while waiting for log entry to occur")
            
            end = time.perf_counter()
            if (end-begin) > 5:
                break
        
        # Now, register task 2
        self.create_new_task(2)
        self.set_task_code("logint_2_task_no_yield", 2)
        self.start_user_task(2)

        # Check if task 2 is running
        begin = time.perf_counter()
        while True:
            found_logentry = False

            try:
                for line in self.vm.follow_logfile(marker="LogInt:"):
                    logintValue = line.split(" ")[1]
                    if logintValue == "2":
                        found_logentry = True
                        break
            except TimeoutError as e:
                self.fail(f"Timout while waiting for log entry to occur")
            
            if found_logentry:
                break

            end = time.perf_counter()
            self.assertTrue((end-begin) <= 5, "Waiting for 'Logint: 2' timed out after 5 seconds")
        
        # Now check that task 1 eventually runs again
        begin = time.perf_counter() 
        while True:
            found_logentry = False

            try:
                for line in self.vm.follow_logfile(marker="LogInt:"):
                    logintValue = line.split(" ")[1]
                    if logintValue == "1":
                        found_logentry = True
                        break
            except TimeoutError as e:
                self.fail(f"Timout while waiting for log entry to occur")

            if found_logentry:
                break

            end = time.perf_counter()
            self.assertTrue((end-begin) <= 5, "Waiting for 'Logint: 1' timed out after 5 seconds")
        
        # Now check that task 2 eventually runs again
        begin = time.perf_counter()
        while True:
            found_logentry = False

            try:
                for line in self.vm.follow_logfile(marker="LogInt:"):
                    logintValue = line.split(" ")[1]
                    if logintValue == "2":
                        found_logentry = True
                        break
            except TimeoutError as e:
                self.fail(f"Timout while waiting for log entry to occur")

            if found_logentry:
                break

            end = time.perf_counter()
            self.assertTrue((end-begin) <= 5, "Waiting for 'Logint: 2' timed out after 5 seconds")

        # Now check that task 1 eventually runs again
        begin = time.perf_counter()
        while True:
            found_logentry = False

            try:
                for line in self.vm.follow_logfile(marker="LogInt:"):
                    logintValue = line.split(" ")[1]
                    if logintValue == "1":
                        found_logentry = True
                        break
            except TimeoutError as e:
                self.fail(f"Timout while waiting for log entry to occur")

            if found_logentry:
                break

            end = time.perf_counter()
            self.assertTrue((end-begin) <= 5, "Waiting for 'Logint: 1' timed out after 5 seconds")

        # Now check that task 2 eventually runs again
        begin = time.perf_counter()
        while True:
            found_logentry = False

            try:
                for line in self.vm.follow_logfile(marker="LogInt:"):
                    logintValue = line.split(" ")[1]
                    if logintValue == "2":
                        found_logentry = True
                        break
            except TimeoutError as e:
                self.fail(f"Timout while waiting for log entry to occur")

            if found_logentry:
                break

            end = time.perf_counter()
            self.assertTrue((end-begin) <= 5, "Waiting for 'Logint: 2' timed out after 5 seconds")
        