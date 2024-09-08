from typing import Iterator, Tuple, List
import time
import scapy.all
import scapy.plist

from abc import ABC, abstractmethod

class Vm(ABC):
    def __init__(self, disk_path: str, disk_start: int, serial_logfile_path: str = None, pcap_file_path: str = None) -> None:
        self.disk_path = disk_path
        self.disk_start = disk_start
        
        self.serial_logfile_path = serial_logfile_path
        self.serial_logfile = None

        self.pcap_file_path = pcap_file_path

    def start(self) -> None:
        if self.serial_logfile_path is not None:
            # Empty the logfile
            open(self.serial_logfile_path, "w").close()

            # Open the logfile
            self.serial_logfile = open(self.serial_logfile_path, "r")

    def stop(self) -> None:
        if self.serial_logfile is not None:
            # Close the logfile
            self.serial_logfile.close()

    def follow_logfile(self, marker: str ="", timeout: int=30) -> Iterator[str]:
        if self.serial_logfile is None:
            raise ValueError("Serial log file is not open")

        begin = time.perf_counter()
        marker_length = len(marker)
        line = ""
        while True:
            tmp = self.serial_logfile.readline()
            if tmp != "":
                line += tmp
                if line.endswith("\n"):
                    if marker==line[:marker_length]:
                        yield line[:-1]
                    line = ''
            end = time.perf_counter()
            if (end-begin)>timeout:
                raise TimeoutError(f"Waiting for new line in logfile timed out after {timeout} seconds")
    
    def pcapfile_get_packets(self) -> scapy.plist.PacketList:
        if self.pcap_file_path is None:
            raise ValueError("PCAP file path is not set")

        packets = scapy.all.rdpcap(self.pcap_file_path)
        return packets

    def copy_image_to_disk(self, image_path: str, disk_start_offset: int) -> None:
        seek_index = self.disk_start+disk_start_offset
        with open(image_path, "rb") as in_file, open(self.disk_path, "r+b") as out_file:
            while(new_byte:=in_file.read(1)):
                out_file.seek(seek_index)
                out_file.write(new_byte)
                seek_index += 1