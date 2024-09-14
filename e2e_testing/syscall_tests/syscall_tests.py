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

import socket

import asyncio
from aiocoap import Context, Message, GET, PUT, DELETE, ContentFormat

from enum import Enum

class SyscallTests(VmTests):
    OS_MGMT_PORT = 5683
    USER_TASK_PROCESS_ENTRY_ADDRESS = 0x2024000

    @classmethod
    def setUpClass(self) -> None:
        super().setUpClass()

        with open(Path(__file__).parent.absolute() / "../config.yaml", 'r') as file:
            config = yaml.safe_load(file)

        self.os_ip_string = config['operating_system']["ip"]
        self.os_ip = sum([int(octet_string)*(2**(24-8*i)) for i, octet_string in enumerate(self.os_ip_string.split("."))])
        self.client_ip_string = config["required_testing_info"]["client_ip"]
        self.client_ip = sum([int(octet_string)*(2**(24-8*i)) for i, octet_string in enumerate(self.client_ip_string.split("."))])

        for tasks_child in os.listdir(Path(__file__).parent.absolute() / "tasks"):
            if tasks_child!="link.ld":
                current_directory = os.path.dirname(os.path.abspath(__file__))
                makefile_helper = MakefileHelper(config['operating_system']["make_command"], os.path.join(current_directory, f"tasks/{tasks_child}"))
                makefile_helper.make_clean()
                makefile_helper.make(f"CPPFLAGS=-DTEST_CLIENT_IP={self.client_ip} -DMY_IP={self.os_ip}")
        
        self.socket_timeout = 20
    
    def setUp(self) -> None:
        super().setUp()
        
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock.bind((self.client_ip_string, 10000))
        self.sock.settimeout(self.socket_timeout)

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

        self.sock.close()
    
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
            address = SyscallTests.USER_TASK_PROCESS_ENTRY_ADDRESS
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
            self.os_ip_string,
            SyscallTests.OS_MGMT_PORT, 
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
        
        result = asyncio.run(remove_task(self.os_ip_string, SyscallTests.OS_MGMT_PORT, id))
        return result
    
    def test_task_sends_simple_packet_should_arrive_correctly(self) -> None:
        success = self.deploy_user_task("send_simple_packet_task", 1)
        if not success:
            self.fail("Failed to deploy task")

        try:
            data, address = self.sock.recvfrom(65507)
            self.assertEqual(address[0], self.os_ip_string, "Source IP in packet is wrong")
            self.assertEqual(address[1], 1000, "Source port in packet is wrong")
            self.assertEqual(data, b"hello world!", "Payload in packet is wrong")
        except socket.timeout:
            self.fail(f"Waiting for UDP on the socket timed out after {self.socket_timeout} seconds")
        
        try:
            for line in self.vm.follow_logfile(marker="LogInt:"):
                logintValue = line.split(" ")[1]
                if logintValue == "99":
                    found_logentry = True
                    break
        except TimeoutError as e:
            self.fail(f"Timeout while waiting for log entry")
    
    def test_task_sends_fragmented_packet_should_arrive_correctly(self) -> None:
        success = self.deploy_user_task("send_fragmented_packet_task", 2)
        if not success:
            self.fail("Failed to deploy task")

        expected_data = bytes(i%10 for i in range(5000))
        try:
            data, address = self.sock.recvfrom(65507)
            self.assertEqual(address[0], self.os_ip_string, "Source IP in packet is wrong")
            self.assertEqual(address[1], 1000, "Source port in packet is wrong")
            self.assertEqual(data, expected_data, "Payload in packet is wrong")
        except socket.timeout:
            self.fail(f"Waiting for UDP on the socket timed out after {self.socket_timeout} seconds")
        
        try:
            for line in self.vm.follow_logfile(marker="LogInt:"):
                logintValue = line.split(" ")[1]
                if logintValue == "99":
                    found_logentry = True
                    break
        except TimeoutError as e:
            self.fail(f"Timeout while waiting for log entry")
    
    def test_task_sends_multiple_packets_should_arrive_correctly(self) -> None:
        success = self.deploy_user_task("send_multiple_packets_task", 3)
        if not success:
            self.fail("Failed to deploy task")

        expected_data1 = b'hello world!'
        expected_data2 = bytes((ord('0') + (i % 10)) for i in range(3000))
        expected_data3 = bytes(ord('1') for i in range(1000))
        expected_data4 = bytes(ord('2') for i in range(2000))

        try:
            data, address = self.sock.recvfrom(65507)
            self.assertEqual(address[0], self.os_ip_string, "Source IP in packet is wrong")
            self.assertEqual(address[1], 1000, "Source port in packet is wrong")
            self.assertEqual(data, expected_data1, "Payload in packet is wrong")
        except socket.timeout:
            self.fail(f"Waiting for UDP on the socket timed out after {self.socket_timeout} seconds")

        try:
            data, address = self.sock.recvfrom(65507)
            self.assertEqual(address[0], self.os_ip_string, "Source IP in packet is wrong")
            self.assertEqual(address[1], 1000, "Source port in packet is wrong")
            self.assertEqual(data, expected_data2, "Payload in packet is wrong")
        except socket.timeout:
            self.fail(f"Waiting for UDP on the socket timed out after {self.socket_timeout} seconds")

        try:
            data, address = self.sock.recvfrom(65507)
            self.assertEqual(address[0], self.os_ip_string, "Source IP in packet is wrong")
            self.assertEqual(address[1], 1000, "Source port in packet is wrong")
            self.assertEqual(data, expected_data3, "Payload in packet is wrong")
        except socket.timeout:
            self.fail(f"Waiting for UDP on the socket timed out after {self.socket_timeout} seconds")

        try:
            data, address = self.sock.recvfrom(65507)
            self.assertEqual(address[0], self.os_ip_string, "Source IP in packet is wrong")
            self.assertEqual(address[1], 1000, "Source port in packet is wrong")
            self.assertEqual(data, expected_data4, "Payload in packet is wrong")
        except socket.timeout:
            self.fail(f"Waiting for UDP on the socket timed out after {self.socket_timeout} seconds")

        try:
            for line in self.vm.follow_logfile(marker="LogInt:"):
                logintValue = line.split(" ")[1]
                if logintValue == "99":
                    found_logentry = True
                    break
        except TimeoutError as e:
            self.fail(f"Timeout while waiting for log entry")
    
    def test_task_sends_two_packets_should_be_handled_during_a_single_timeslot(self) -> None:
        success = self.deploy_user_task("send_two_packets_in_a_timeslot_task", 1)
        if not success:
            self.fail("Failed to deploy task")

        expected_data1 = b'hello world!'
        expected_data2 = bytes((ord('0') + (i % 10)) for i in range(3000))

        try:
            data, address = self.sock.recvfrom(65507)
            self.assertEqual(address[0], self.os_ip_string, "Source IP in packet is wrong")
            self.assertEqual(address[1], 1000, "Source port in packet is wrong")
            self.assertEqual(data, expected_data1, "Payload in packet is wrong")
        except socket.timeout:
            self.fail(f"Waiting for UDP on the socket timed out after {self.socket_timeout} seconds")

        try:
            data, address = self.sock.recvfrom(65507)
            self.assertEqual(address[0], self.os_ip_string, "Source IP in packet is wrong")
            self.assertEqual(address[1], 1000, "Source port in packet is wrong")
            self.assertEqual(data, expected_data2, "Payload in packet is wrong")
        except socket.timeout:
            self.fail(f"Waiting for UDP on the socket timed out after {self.socket_timeout} seconds")

        try:
            for line in self.vm.follow_logfile(marker="LogInt:"):
                logintValue = line.split(" ")[1]
                if logintValue == "99":
                    found_logentry = True
                    break
        except TimeoutError as e:
            self.fail(f"Timeout while waiting for log entry")
    
    def test_task_sends_packet_before_its_virt_addr_space_should_fail(self) -> None:
        success = self.deploy_user_task("send_packet_under_0x2014000_task", 1)
        if not success:
            self.fail("Failed to deploy task")

        try:
            for line in self.vm.follow_logfile(marker="LogInt:"):
                logintValue = line.split(" ")[1]
                if logintValue == "199":
                    break
        except TimeoutError as e:
            self.fail(f"Timeout while waiting for log entry")
    
    def test_task_sends_packet_after_its_virt_addr_space_should_fail(self) -> None:
        success = self.deploy_user_task("send_packet_above_0x2800000_task", 1)
        if not success:
            self.fail("Failed to deploy task")

        try:
            for line in self.vm.follow_logfile(marker="LogInt:"):
                logintValue = line.split(" ")[1]
                if logintValue == "199":
                    break
        except TimeoutError as e:
            self.fail(f"Timeout while waiting for log entry")
    
    def test_task_receives_simple_packet_should_arrive_correctly(self) -> None:
        success = self.deploy_user_task("receive_simple_packet_task", 1)
        if not success:
            self.fail("Failed to deploy task")

        self.sock.sendto(b"hello world!", (self.os_ip_string, 1000))

        try:
            for line in self.vm.follow_logfile(marker="LogInt:"):
                logintValue = line.split(" ")[1]
                if logintValue == "299":
                    found_logentry = True
                    break
                elif logintValue == "309":
                    self.fail("Task did not receive the packet correctly")
        except TimeoutError as e:
            self.fail(f"Timeout while waiting for log entry")
    
    def test_task_receives_fragmented_packet_should_arrive_correctly(self) -> None:
        success = self.deploy_user_task("receive_fragmented_packet_task", 1)
        if not success:
            self.fail("Failed to deploy task")

        expected_data = bytes((ord('0') + (i % 10)) for i in range(6200))
        self.sock.sendto(expected_data, (self.os_ip_string, 1000))

        try:
            for line in self.vm.follow_logfile(marker="LogInt:"):
                logintValue = line.split(" ")[1]
                if logintValue == "299":
                    found_logentry = True
                    break
                elif logintValue == "309":
                    self.fail("Task did not receive the packet correctly")
        except TimeoutError as e:
            self.fail(f"Timeout while waiting for log entry")
    
    def test_task_receives_multiple_packets_should_arrive_correctly(self) -> None:
        success = self.deploy_user_task("receive_multiple_packets_task", 1)
        if not success:
            self.fail("Failed to deploy task")

        expected_data1 = b'hello world!'
        expected_data2 = bytes((ord('0') + (i % 10)) for i in range(3000))
        expected_data3 = bytes(ord('1') for i in range(1000))
        expected_data4 = bytes(ord('2') for i in range(2000))

        self.sock.sendto(expected_data1, (self.os_ip_string, 1000))
        self.sock.sendto(expected_data2, (self.os_ip_string, 1000))
        self.sock.sendto(expected_data3, (self.os_ip_string, 1000))
        self.sock.sendto(expected_data4, (self.os_ip_string, 1000))

        try:
            for line in self.vm.follow_logfile(marker="LogInt:"):
                logintValue = line.split(" ")[1]
                if logintValue == "299":
                    found_logentry = True
                    break
                elif logintValue == "309":
                    self.fail("Task did not receive the packet correctly")
        except TimeoutError as e:
            self.fail(f"Timeout while waiting for log entry")
    
    def test_two_tasks_opening_sockets_on_same_ports_should_fail(self) -> None:
        success = self.deploy_user_task("open_socket_99_task", 1)
        if not success:
            self.fail("Failed to deploy task")

        try:
            for line in self.vm.follow_logfile(marker="LogInt:"):
                logintValue = line.split(" ")[1]
                if logintValue == "998":
                    break
        except TimeoutError as e:
            self.fail(f"Timeout while waiting for log entry")

        success = self.deploy_user_task("open_socket_99_task", 2)
        if not success:
            self.fail("Failed to deploy task")

        try:
            for line in self.vm.follow_logfile(marker="LogInt:"):
                logintValue = line.split(" ")[1]
                if logintValue == "999":
                    break
        except TimeoutError as e:
            self.fail(f"Timeout while waiting for log entry")
    
    def test_task_uses_socket_after_other_task_closed_it_should_succeed(self) -> None:
        success = self.deploy_user_task("open_and_close_socket_99_task", 1)
        if not success:
            self.fail("Failed to deploy task")

        try:
            for line in self.vm.follow_logfile(marker="LogInt:"):
                logintValue = line.split(" ")[1]
                if logintValue == "998":
                    break
        except TimeoutError as e:
            self.fail(f"Timeout while waiting for log entry")

        success = self.deploy_user_task("open_and_close_socket_99_task", 2)
        if not success:
            self.fail("Failed to deploy task")

        try:
            for line in self.vm.follow_logfile(marker="LogInt:"):
                logintValue = line.split(" ")[1]
                if logintValue == "998":
                    break
        except TimeoutError as e:
            self.fail(f"Timeout while waiting for log entry")
    
    def test_task_sends_packet_to_itself_should_be_possible(self) -> None:
        success = self.deploy_user_task("send_hello_world_to_self_task", 1)
        if not success:
            self.fail("Failed to deploy task")

        found_logentry = False
        begin = time.perf_counter()
        while True:
            try:
                for line in self.vm.follow_logfile(marker="LogInt:"):
                    logintValue = line.split(" ")[1]
                    if logintValue == "400":
                        found_logentry = True
                    elif logintValue == "409":
                        self.fail("Task did not receive the packet correctly")
                    break
            except TimeoutError as e:
                self.fail(f"Timeout while waiting for log entry")
            
            if found_logentry:
                break
            
            end = time.perf_counter()
            self.assertTrue((end-begin) <= 20, "Waiting for log entry timed out after 20 seconds")

    def test_tasks_send_packets_to_eachother_should_be_possible(self) -> None:
        success = self.deploy_user_task("receive_fragmented_from_self_and_send_ack_task", 1)
        if not success:
            self.fail("Failed to deploy task")
        
        success = self.deploy_user_task("send_fragmented_to_self_and_await_ack_task", 2)
        if not success:
            self.fail("Failed to deploy task")
        
        try:
            for line in self.vm.follow_logfile(marker="LogInt:"):
                logintValue = line.split(" ")[1]
                if logintValue == "1033":
                    break
        except TimeoutError as e:
            self.fail(f"Timeout while waiting for log entry")
        
        try:
            for line in self.vm.follow_logfile(marker="LogInt:"):
                logintValue = line.split(" ")[1]
                if logintValue == "1034":
                    break
                elif logintValue == "1035":
                    self.fail("Task1 did not receive the packet correctly")
        except TimeoutError as e:
            self.fail(f"Timeout while waiting for log entry")
        
        found_1036 = False
        found_1038 = False
        begin = time.perf_counter()
        while True:
            try:
                for line in self.vm.follow_logfile(marker="LogInt:"):
                    logintValue = line.split(" ")[1]
                    if logintValue == "1036":
                        found_1036 = True
                    elif logintValue == "1038":
                        found_1038 = True
                    elif logintValue == "1037":
                        self.fail("Task2 did not receive the packet correctly")
                    break
            except TimeoutError as e:
                self.fail(f"Timeout while waiting for log entry")
            
            if found_1036 and found_1038:
                break
            
            end = time.perf_counter()
            self.assertTrue((end-begin) <= 20, f"Waiting for log entry timed out after 20 seconds, found_1036: {found_1036}, found_1038: {found_1038}")

    def wait_10_seconds_using_timer_counter_syscall_should_be_approximately_correct(self) -> None:
        ALLOWABLE_ERROR = 0.2

        success = self.deploy_user_task("wait_10s_task", 1)
        if not success:
            self.fail("Failed to deploy task")
        
        try:
            for line in self.vm.follow_logfile(marker="LogInt:"):
                logintValue = line.split(" ")[1]
                if logintValue == "1998":
                    break
        except TimeoutError as e:
            self.fail(f"Timeout while waiting for log entry")
        
        begin = time.perf_counter()

        try:
            for line in self.vm.follow_logfile(marker="LogInt:"):
                logintValue = line.split(" ")[1]
                if logintValue == "1999":
                    break
        except TimeoutError as e:
            self.fail(f"Timeout while waiting for log entry")

        end = time.perf_counter()
        passed_time = end-begin

        self.assertTrue(passed_time >= 10*(1-ALLOWABLE_ERROR), f"Task finished already after {passed_time} seconds")
        self.assertTrue(passed_time <= 10*(1+ALLOWABLE_ERROR), f"Task only finished after {passed_time} seconds")
    
    def test_setting_receive_buffer_to_nullptr_should_succeed_if_size_also_zero(self) -> None:
        success = self.deploy_user_task("set_receive_buffer_to_nullptr_task", 1)
        if not success:
            self.fail("Failed to deploy task")
        
        try:
            for line in self.vm.follow_logfile(marker="LogInt:"):
                logintValue = line.split(" ")[1]
                if logintValue == "88":
                    self.fail("First test failed")
                elif logintValue == "89":
                    break
            for line in self.vm.follow_logfile(marker="LogInt:"):
                logintValue = line.split(" ")[1]
                if logintValue == "90":
                    self.fail("Second test failed")
                elif logintValue == "91":
                    break
            for line in self.vm.follow_logfile(marker="LogInt:"):
                logintValue = line.split(" ")[1]
                if logintValue == "92":
                    break
                elif logintValue == "93":
                    self.fail("Third test failed")
            for line in self.vm.follow_logfile(marker="LogInt:"):
                logintValue = line.split(" ")[1]
                if logintValue == "94":
                    break
                elif logintValue == "95":
                    self.fail("Fourth test failed")
        except TimeoutError as e:
            self.fail(f"Timeout while waiting for log entry")
    
    def test_setting_send_buffer_to_nullptr_should_succeed_if_size_zero_and_indicator_nullptr(self) -> None:
        success = self.deploy_user_task("set_send_buffer_to_nullptr_task", 1)
        if not success:
            self.fail("Failed to deploy task")
        
        try:
            for line in self.vm.follow_logfile(marker="LogInt:"):
                logintValue = line.split(" ")[1]
                if logintValue == "88":
                    self.fail("First test failed")
                elif logintValue == "89":
                    break
            for line in self.vm.follow_logfile(marker="LogInt:"):
                logintValue = line.split(" ")[1]
                if logintValue == "90":
                    self.fail("Second test failed")
                elif logintValue == "91":
                    break
            for line in self.vm.follow_logfile(marker="LogInt:"):
                logintValue = line.split(" ")[1]
                if logintValue == "92":
                    self.fail("Second test failed")
                elif logintValue == "93":
                    break
            for line in self.vm.follow_logfile(marker="LogInt:"):
                logintValue = line.split(" ")[1]
                if logintValue == "94":
                    break
                elif logintValue == "95":
                    self.fail("Third test failed")
            for line in self.vm.follow_logfile(marker="LogInt:"):
                logintValue = line.split(" ")[1]
                if logintValue == "96":
                    break
                elif logintValue == "97":
                    self.fail("Third test failed")
            for line in self.vm.follow_logfile(marker="LogInt:"):
                logintValue = line.split(" ")[1]
                if logintValue == "98":
                    break
                elif logintValue == "99":
                    self.fail("Third test failed")
        except TimeoutError as e:
            self.fail(f"Timeout while waiting for log entry")