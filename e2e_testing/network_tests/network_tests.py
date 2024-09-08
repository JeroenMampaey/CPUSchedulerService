#!/usr/bin/python3
import sys
from pathlib import Path
sys.path.append("../../python_lib")

from vm_tests import VmTests

import time
import yaml
import subprocess
from typing import Tuple, List
import re
import math
from scapy.all import *
import subprocess
import socket

class NetworkTests(VmTests):
    UDP_HEADER_SIZE = 8
    MINIMAL_IPV4_HEADER_SIZE = 20
    MTU = 1500
    NUM_PACKET_BUFFERS = 1000
    RCT_TIMER_FREQUENCY = 2
    MAX_TIMER_COUNTER = 10
    NUM_TX_DESCRIPTORS = 8

    @classmethod
    def setUpClass(self) -> None:
        super().setUpClass()

        with open(Path(__file__).parent.absolute() / "../config.yaml", 'r') as file:
            config = yaml.safe_load(file)

        self.MAC = config["required_testing_info"]["mac"]
        self.os_ip = config['operating_system']["ip"]
        self.os_gateway_ip = config['operating_system']["gateway_ip"]
        self.client_ip = config["required_testing_info"]["client_ip"]

        self.socket_timeout = 20

    def setUp(self) -> None:
        super().setUp()
        
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock.bind((self.client_ip, 9000))
        self.sock.settimeout(self.socket_timeout)

        self.vm.start()
    
    def tearDown(self) -> None:
        self.vm.stop()

        self.sock.close()

    def get_logged_ipv4_datagram(self) -> List[Tuple[int, int, int]]:
        retval = []
        try:
            for line in self.vm.follow_logfile(marker="NMT:"):
                if len(line)==len("NMT: new datagram begin") and line=="NMT: new datagram begin":
                    break
            for line in self.vm.follow_logfile(marker="NMT:"):
                if len(line)==len("NMT: new datagram end") and line=="NMT: new datagram end":
                    return retval
                elif len(line)>len("NMT: packet: ") and line[:len("NMT: packet: ")]=="NMT: packet: ":
                    values_from_line = re.findall(r'\d+', line)
                    self.assertTrue(len(values_from_line)==3, f"Encountered NMT log was unexpected: \"{line}\"")
                    retval.append((int(values_from_line[0]), int(values_from_line[1]), int(values_from_line[2])))
        except TimeoutError as e:
            self.fail(f"Timeout while waiting for network management task log entries")

    def test_logged_MAC_should_agree_with_vm_network_card_MAC(self) -> None:
        try:
            for line in self.vm.follow_logfile(marker="NCM:"):
                self.assertEqual(line, f"NCM: MAC: {self.MAC}", "Logged MAC value is wrong")
                break
        except TimeoutError as e:
            self.fail(f"Timeout while waiting for network card manager log entries")
    
    """
    TODO: this test used to work for QEMU, but nowadays this line fails:
    arp_request = Ether(dst="ff:ff:ff:ff:ff:ff") / ARP(pdst=self.os_ip)
    Not because the OS does not reply, but because the ARP request is not sent for some reason.

    def test_network_stack_should_be_initialized_correctly(self) -> None:
        try:
            for line in self.vm.follow_logfile(marker="NetworkStack:"):
                self.assertEqual(line, f"NetworkStack: initializing...", "Networkstack log message is unexpected")
                break
            for line in self.vm.follow_logfile(marker="NetworkStack:"):
                self.assertEqual(line, f"NetworkStack: initialization done", "Networkstack log message is unexpected")
                break
        except TimeoutError as e:
            self.fail(f"Timeout while waiting for network stack log entries")

        begin = time.perf_counter()
        while True:
            found_packet = False

            packets = self.vm.pcapfile_get_packets()
            for packet in packets:
                packet_src = None
                try:
                    packet_src = packet.src.upper()
                except Exception as e:
                    continue
                
                if packet_src==self.MAC:
                    self.assertEqual(packet.original[14:], b"Hello World!", f"First packet send by operating system was not the \"Hello World!\" packet, instead the following packet was found:\n{packet.original}")
                    
                    found_packet = True
                    break
            
            if found_packet:
                break

            end = time.perf_counter()
            self.assertTrue((end-begin) <= 10, "Waiting for \"Hello World!\" packet timed out after 10 seconds")
        
        try:
            data, address = self.sock.recvfrom(65507)
            self.assertEqual(address[0], self.os_ip, "Source IP in packet is wrong")
            self.assertEqual(address[1], 1000, "Source port in packet is wrong")
            self.assertEqual(data, b"Hello World!", "Payload in packet is wrong")
        except socket.timeout:
            self.fail(f"Waiting for UDP on the socket timed out after {self.socket_timeout} seconds")
        
        try:
            data, address = self.sock.recvfrom(65507)
            expected_data_size = int((NetworkTests.NUM_TX_DESCRIPTORS-1)*(NetworkTests.MTU-NetworkTests.MINIMAL_IPV4_HEADER_SIZE)-NetworkTests.UDP_HEADER_SIZE-((NetworkTests.MTU-NetworkTests.MINIMAL_IPV4_HEADER_SIZE)/2))
            expected_data = bytes((ord('0') + i % 10) for i in range(expected_data_size))
            self.assertEqual(address[0], self.os_ip, "Source IP in packet is wrong")
            self.assertEqual(address[1], 2000, "Source port in packet is wrong")
            self.assertEqual(data, expected_data, "Payload in packet is wrong")
        except socket.timeout:
            self.fail(f"Waiting for UDP on the socket timed out after {self.socket_timeout} seconds")
        
        arp_request = Ether(dst="ff:ff:ff:ff:ff:ff") / ARP(pdst=self.os_ip)
        srp(arp_request)

        begin = time.perf_counter()
        while True:
            found_packet = False

            packets = self.vm.pcapfile_get_packets()
            for packet in packets:
                arp_packet = None
                try:
                    arp_packet = packet["ARP"]
                except Exception as e:
                    continue

                if arp_packet.op == 2 and arp_packet.psrc==self.os_ip:
                    self.assertEqual(arp_packet.hwsrc, self.MAC.lower(), "MAC in ARP response is wrong")
                    self.assertEqual(arp_packet.psrc, self.os_ip, "responder source IP in ARP response is wrong")
                    self.assertTrue(arp_packet.hwdst is not None and arp_packet.pdst is not None, "ARP response does not seem to be complete")
                    
                    found_packet = True
                    break
            
            if found_packet:
                break

            end = time.perf_counter()
            self.assertTrue((end-begin) <= 10, "Waiting for ARP response timed out after 10 seconds")"""
    
    def test_network_stack_udp_packets_should_be_correctly_received(self) -> None:
        try:
            for line in self.vm.follow_logfile(marker="NetworkStack:"):
                self.assertEqual(line, f"NetworkStack: initializing...", "Networkstack log message is unexpected")
                break
            for line in self.vm.follow_logfile(marker="NetworkStack:"):
                self.assertEqual(line, f"NetworkStack: initialization done", "Networkstack log message is unexpected")
                break
        except TimeoutError as e:
            self.fail(f"Timeout while waiting for network stack log entries")
        
        message = "Hello World From The Host!"
        udp_packet = IP(dst=self.os_ip, id=1)/UDP(sport=54321, dport=6000)/Raw(load=message.encode())
        send(udp_packet)
        datagram = self.get_logged_ipv4_datagram()
        self.assertEqual(len(datagram), 1, f"Num packets in datagram was unexpected: {len(datagram)}")
        self.assertEqual(datagram[0], (0, 0, len(message)+NetworkTests.UDP_HEADER_SIZE), f"Datagram packet is unexpected: {datagram[0]}")

        message = "Hello World!"*1000
        udp_packet = IP(dst=self.os_ip, id=2)/UDP(sport=54321, dport=6000)/Raw(load=message.encode())
        fragments = fragment(udp_packet)
        for frag in fragments:
            send(frag)
        datagram = self.get_logged_ipv4_datagram()
        expected_num_packet = int(math.ceil((len(message)+NetworkTests.UDP_HEADER_SIZE)/(NetworkTests.MTU-NetworkTests.MINIMAL_IPV4_HEADER_SIZE)))
        self.assertEqual(len(datagram), expected_num_packet, f"Num packets in datagram was unexpected: {len(datagram)}")
        for i, packet in enumerate(datagram):
            if i==len(datagram)-1:
                remaining_size = (len(message)+NetworkTests.UDP_HEADER_SIZE) % (NetworkTests.MTU-NetworkTests.MINIMAL_IPV4_HEADER_SIZE)
                if remaining_size==0:
                    remaining_size = NetworkTests.MTU-NetworkTests.MINIMAL_IPV4_HEADER_SIZE
                self.assertEqual(packet, ((NetworkTests.MTU-NetworkTests.MINIMAL_IPV4_HEADER_SIZE)*i, 0, remaining_size), f"Datagram packet {i} is unexpected: {packet}")
            else:
                self.assertEqual(packet, ((NetworkTests.MTU-NetworkTests.MINIMAL_IPV4_HEADER_SIZE)*i, 1, NetworkTests.MTU-NetworkTests.MINIMAL_IPV4_HEADER_SIZE), f"Datagram packet {i} is unexpected: {packet}")

        message = "1"*(NetworkTests.MTU-NetworkTests.MINIMAL_IPV4_HEADER_SIZE-NetworkTests.UDP_HEADER_SIZE)
        udp_packet = IP(dst=self.os_ip, id=3)/UDP(sport=54321, dport=6000)/Raw(load=message.encode())
        send(udp_packet)
        datagram = self.get_logged_ipv4_datagram()
        self.assertEqual(len(datagram), 1, f"Num packets in datagram was unexpected: {len(datagram)}")
        self.assertEqual(datagram[0], (0, 0, len(message)+NetworkTests.UDP_HEADER_SIZE), f"Datagram packet is unexpected: {datagram[0]}")

        message = "1"*(10*(NetworkTests.MTU-NetworkTests.MINIMAL_IPV4_HEADER_SIZE)-NetworkTests.UDP_HEADER_SIZE)
        udp_packet = IP(dst=self.os_ip, id=4)/UDP(sport=54321, dport=6000)/Raw(load=message.encode())
        fragments = fragment(udp_packet)
        for frag in fragments:
            send(frag)
        datagram = self.get_logged_ipv4_datagram()
        expected_num_packet = int(math.ceil((len(message)+NetworkTests.UDP_HEADER_SIZE)/(NetworkTests.MTU-NetworkTests.MINIMAL_IPV4_HEADER_SIZE)))
        self.assertEqual(len(datagram), expected_num_packet, f"Num packets in datagram was unexpected: {len(datagram)}")
        for i, packet in enumerate(datagram):
            if i==len(datagram)-1:
                remaining_size = (len(message)+NetworkTests.UDP_HEADER_SIZE) % (NetworkTests.MTU-NetworkTests.MINIMAL_IPV4_HEADER_SIZE)
                if remaining_size==0:
                    remaining_size = NetworkTests.MTU-NetworkTests.MINIMAL_IPV4_HEADER_SIZE
                self.assertEqual(packet, ((NetworkTests.MTU-NetworkTests.MINIMAL_IPV4_HEADER_SIZE)*i, 0, remaining_size), f"Datagram packet {i} is unexpected: {packet}")
            else:
                self.assertEqual(packet, ((NetworkTests.MTU-NetworkTests.MINIMAL_IPV4_HEADER_SIZE)*i, 1, NetworkTests.MTU-NetworkTests.MINIMAL_IPV4_HEADER_SIZE), f"Datagram packet {i} is unexpected: {packet}")
            
        message = "1"*(2*(NetworkTests.MTU-NetworkTests.MINIMAL_IPV4_HEADER_SIZE)-NetworkTests.UDP_HEADER_SIZE)
        udp_packet = IP(dst=self.os_ip, id=5)/UDP(sport=54321, dport=6000)/Raw(load=message.encode())
        fragments = fragment(udp_packet)
        for frag in fragments:
            send(frag)
        datagram = self.get_logged_ipv4_datagram()
        expected_num_packet = int(math.ceil((len(message)+NetworkTests.UDP_HEADER_SIZE)/(NetworkTests.MTU-NetworkTests.MINIMAL_IPV4_HEADER_SIZE)))
        self.assertEqual(len(datagram), expected_num_packet, f"Num packets in datagram was unexpected: {len(datagram)}")
        for i, packet in enumerate(datagram):
            if i==len(datagram)-1:
                remaining_size = (len(message)+NetworkTests.UDP_HEADER_SIZE) % (NetworkTests.MTU-NetworkTests.MINIMAL_IPV4_HEADER_SIZE)
                if remaining_size==0:
                    remaining_size = NetworkTests.MTU-NetworkTests.MINIMAL_IPV4_HEADER_SIZE
                self.assertEqual(packet, ((NetworkTests.MTU-NetworkTests.MINIMAL_IPV4_HEADER_SIZE)*i, 0, remaining_size), f"Datagram packet {i} is unexpected: {packet}")
            else:
                self.assertEqual(packet, ((NetworkTests.MTU-NetworkTests.MINIMAL_IPV4_HEADER_SIZE)*i, 1, NetworkTests.MTU-NetworkTests.MINIMAL_IPV4_HEADER_SIZE), f"Datagram packet {i} is unexpected: {packet}")
    
    def test_network_stack_fragmented_udp_should_be_correctly_handled(self) -> None:
        try:
            for line in self.vm.follow_logfile(marker="NetworkStack:"):
                self.assertEqual(line, f"NetworkStack: initializing...", "Networkstack log message is unexpected")
                break
            for line in self.vm.follow_logfile(marker="NetworkStack:"):
                self.assertEqual(line, f"NetworkStack: initialization done", "Networkstack log message is unexpected")
                break
        except TimeoutError as e:
            self.fail(f"Timeout while waiting for network stack log entries")
        
        message = "1"*5000
        udp_packet = IP(dst=self.os_ip, id=1) / UDP(sport=54321, dport=6000) / Raw(load=message.encode())
        fragments = fragment(udp_packet)

        for i, frag in enumerate(fragments):
            if (i%2)==0:
                send(frag)

        in_between_message = "Hello World From The Host!"
        udp_packet = IP(dst=self.os_ip, id=2)/UDP(sport=54321, dport=6000)/Raw(load=in_between_message.encode())
        send(udp_packet)
        datagram = self.get_logged_ipv4_datagram()
        self.assertEqual(len(datagram), 1, f"Num packets in datagram was unexpected: {len(datagram)}")
        self.assertEqual(datagram[0], (0, 0, len(in_between_message)+NetworkTests.UDP_HEADER_SIZE), f"Datagram packet is unexpected: {datagram[0]}")

        for i, frag in enumerate(fragments):
            if (i%2)!=0:
                send(frag)
        
        datagram = self.get_logged_ipv4_datagram()
        expected_num_packet = int(math.ceil((len(message)+NetworkTests.UDP_HEADER_SIZE)/(NetworkTests.MTU-NetworkTests.MINIMAL_IPV4_HEADER_SIZE)))
        self.assertEqual(len(datagram), expected_num_packet, f"Num packets in datagram was unexpected: {len(datagram)}")
        for i, packet in enumerate(datagram):
            if i==len(datagram)-1:
                remaining_size = (len(message)+NetworkTests.UDP_HEADER_SIZE) % (NetworkTests.MTU-NetworkTests.MINIMAL_IPV4_HEADER_SIZE)
                if remaining_size==0:
                    remaining_size = NetworkTests.MTU-NetworkTests.MINIMAL_IPV4_HEADER_SIZE
                self.assertEqual(packet, ((NetworkTests.MTU-NetworkTests.MINIMAL_IPV4_HEADER_SIZE)*i, 0, remaining_size), f"Datagram packet {i} is unexpected: {packet}")
            else:
                self.assertEqual(packet, ((NetworkTests.MTU-NetworkTests.MINIMAL_IPV4_HEADER_SIZE)*i, 1, NetworkTests.MTU-NetworkTests.MINIMAL_IPV4_HEADER_SIZE), f"Datagram packet {i} is unexpected: {packet}")

    def test_network_stack_fragmented_udp_should_be_correctly_handled2(self) -> None:
        try:
            for line in self.vm.follow_logfile(marker="NetworkStack:"):
                self.assertEqual(line, f"NetworkStack: initializing...", "Networkstack log message is unexpected")
                break
            for line in self.vm.follow_logfile(marker="NetworkStack:"):
                self.assertEqual(line, f"NetworkStack: initialization done", "Networkstack log message is unexpected")
                break
        except TimeoutError as e:
            self.fail(f"Timeout while waiting for network stack log entries")
        
        message = "1"*5000
        udp_packet = IP(dst=self.os_ip, id=1) / UDP(sport=54321, dport=6000) / Raw(load=message.encode())
        fragments = fragment(udp_packet)

        for i, frag in enumerate(fragments):
            if (i%2)==0:
                send(frag)

        in_between_message = "2"*6000
        udp_packet = IP(dst=self.os_ip, id=2)/UDP(sport=54321, dport=6000)/Raw(load=in_between_message.encode())
        in_between_fragments = fragment(udp_packet)
        for frag in in_between_fragments:
            send(frag)
        datagram = self.get_logged_ipv4_datagram()
        expected_num_packet = int(math.ceil((len(in_between_message)+NetworkTests.UDP_HEADER_SIZE)/(NetworkTests.MTU-NetworkTests.MINIMAL_IPV4_HEADER_SIZE)))
        self.assertEqual(len(datagram), expected_num_packet, f"Num packets in datagram was unexpected: {len(datagram)}")
        for i, packet in enumerate(datagram):
            if i==len(datagram)-1:
                remaining_size = (len(in_between_message)+NetworkTests.UDP_HEADER_SIZE) % (NetworkTests.MTU-NetworkTests.MINIMAL_IPV4_HEADER_SIZE)
                if remaining_size==0:
                    remaining_size = NetworkTests.MTU-NetworkTests.MINIMAL_IPV4_HEADER_SIZE
                self.assertEqual(packet, ((NetworkTests.MTU-NetworkTests.MINIMAL_IPV4_HEADER_SIZE)*i, 0, remaining_size), f"Datagram packet {i} is unexpected: {packet}")
            else:
                self.assertEqual(packet, ((NetworkTests.MTU-NetworkTests.MINIMAL_IPV4_HEADER_SIZE)*i, 1, NetworkTests.MTU-NetworkTests.MINIMAL_IPV4_HEADER_SIZE), f"Datagram packet {i} is unexpected: {packet}")

        for i, frag in enumerate(fragments):
            if (i%2)!=0:
                send(frag)
        
        datagram = self.get_logged_ipv4_datagram()
        expected_num_packet = int(math.ceil((len(message)+NetworkTests.UDP_HEADER_SIZE)/(NetworkTests.MTU-NetworkTests.MINIMAL_IPV4_HEADER_SIZE)))
        self.assertEqual(len(datagram), expected_num_packet, f"Num packets in datagram was unexpected: {len(datagram)}")
        for i, packet in enumerate(datagram):
            if i==len(datagram)-1:
                remaining_size = (len(message)+NetworkTests.UDP_HEADER_SIZE) % (NetworkTests.MTU-NetworkTests.MINIMAL_IPV4_HEADER_SIZE)
                if remaining_size==0:
                    remaining_size = NetworkTests.MTU-NetworkTests.MINIMAL_IPV4_HEADER_SIZE
                self.assertEqual(packet, ((NetworkTests.MTU-NetworkTests.MINIMAL_IPV4_HEADER_SIZE)*i, 0, remaining_size), f"Datagram packet {i} is unexpected: {packet}")
            else:
                self.assertEqual(packet, ((NetworkTests.MTU-NetworkTests.MINIMAL_IPV4_HEADER_SIZE)*i, 1, NetworkTests.MTU-NetworkTests.MINIMAL_IPV4_HEADER_SIZE), f"Datagram packet {i} is unexpected: {packet}")

    def test_network_stack_uncomplete_fragmented_udp_datagram_should_timeout(self) -> None:
        try:
            for line in self.vm.follow_logfile(marker="NetworkStack:"):
                self.assertEqual(line, f"NetworkStack: initializing...", "Networkstack log message is unexpected")
                break
            for line in self.vm.follow_logfile(marker="NetworkStack:"):
                self.assertEqual(line, f"NetworkStack: initialization done", "Networkstack log message is unexpected")
                break
        except TimeoutError as e:
            self.fail(f"Timeout while waiting for network stack log entries")
        
        message = "1"*(6*(NetworkTests.MTU-NetworkTests.MINIMAL_IPV4_HEADER_SIZE)-NetworkTests.UDP_HEADER_SIZE)
        udp_packet = IP(dst=self.os_ip, id=1) / UDP(sport=54321, dport=6000) / Raw(load=message.encode())
        fragments = fragment(udp_packet)

        for i, frag in enumerate(fragments):
            if (i%2)==0:
                send(frag)

        time.sleep(1 + (NetworkTests.MAX_TIMER_COUNTER+2)/NetworkTests.RCT_TIMER_FREQUENCY)

        # Send new fragmented packet with same id, if old packet is not removed then fragments 
        # will be merged together and packet will not be received correctly
        message = "1"*(2*(NetworkTests.MTU-NetworkTests.MINIMAL_IPV4_HEADER_SIZE)-NetworkTests.UDP_HEADER_SIZE)
        udp_packet = IP(dst=self.os_ip, id=1) / UDP(sport=54321, dport=6000) / Raw(load=message.encode())
        fragments = fragment(udp_packet)

        for i, frag in enumerate(fragments):
            send(frag)

        datagram = self.get_logged_ipv4_datagram()
        expected_num_packet = int(math.ceil((len(message)+NetworkTests.UDP_HEADER_SIZE)/(NetworkTests.MTU-NetworkTests.MINIMAL_IPV4_HEADER_SIZE)))
        self.assertEqual(len(datagram), expected_num_packet, f"Num packets in datagram was unexpected: {len(datagram)}")
        for i, packet in enumerate(datagram):
            if i==len(datagram)-1:
                remaining_size = (len(message)+NetworkTests.UDP_HEADER_SIZE) % (NetworkTests.MTU-NetworkTests.MINIMAL_IPV4_HEADER_SIZE)
                if remaining_size==0:
                    remaining_size = NetworkTests.MTU-NetworkTests.MINIMAL_IPV4_HEADER_SIZE
                self.assertEqual(packet, ((NetworkTests.MTU-NetworkTests.MINIMAL_IPV4_HEADER_SIZE)*i, 0, remaining_size), f"Datagram packet {i} is unexpected: {packet}")
            else:
                self.assertEqual(packet, ((NetworkTests.MTU-NetworkTests.MINIMAL_IPV4_HEADER_SIZE)*i, 1, NetworkTests.MTU-NetworkTests.MINIMAL_IPV4_HEADER_SIZE), f"Datagram packet {i} is unexpected: {packet}")