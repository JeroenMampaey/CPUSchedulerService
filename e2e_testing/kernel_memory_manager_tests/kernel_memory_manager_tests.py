#!/usr/bin/python3
import sys
from pathlib import Path
sys.path.append("../../python_lib")

from vm_tests import VmTests

import numpy as np
import re

from typing import Tuple, List

class KernelMemoryManagerTests(VmTests):
    LAST_ACCESSIBLE_PAGE = 1023*1024*4096
    FIRST_ALLOCATABLE_ADDR = 0x2000000
    NUM_PAGE_TABLES_FOR_KERNEL = 8

    @classmethod
    def setUpClass(self) -> None:
        super().setUpClass()

    def setUp(self) -> None:
        super().setUp()
        
        self.vm.start()
    
    def tearDown(self) -> None:
        self.vm.stop()

    def getMemoryLayout(self) -> Tuple[List[dict], int, int]:
        bios_map_entries = []
        kernel_start = None
        kernel_end = None

        try:
            for line in self.vm.follow_logfile(marker="Main:"):
                if len(line)>17 and line[:17]=="Main: kernelstart":
                    integersFromLine = re.findall(r'(0x[A|B|C|D|E|F|\d]+|\d+)', line)
                    kernel_start = int(integersFromLine[0],0)
                    break
            for line in self.vm.follow_logfile(marker="Main:"):
                if len(line)>15 and line[:15]=="Main: kernelend":
                    integersFromLine = re.findall(r'(0x[A|B|C|D|E|F|\d]+|\d+)', line)
                    kernel_end = int(integersFromLine[0],0)
                    break
            for line in self.vm.follow_logfile(marker="BM:"):
                if len(line)==13 and line=="BM: log begin":
                    break
            for line in self.vm.follow_logfile(marker="BM:"):
                if len(line)==11 and line=="BM: log end":
                    break
                else:
                    # Some integers are hexadecimals thus that needs to be taken into account
                    integersFromLine = re.findall(r'(0x[A|B|C|D|E|F|\d]+|\d+)', line)

                    bios_map_entry = {
                        "region_addr": int(integersFromLine[0],0),
                        "region_length": int(integersFromLine[1],0),
                        "region_type": int(integersFromLine[2],0),
                        "extended_attributes": int(integersFromLine[3],0)
                    }

                    bios_map_entries.append(bios_map_entry)
        except TimeoutError as e:
            self.fail("getMemoryLayout timed out while waiting for a log message")

        return bios_map_entries, kernel_start, kernel_end

    def getKmmState(self) -> Tuple[np.ndarray, List[List[int]]]:
        page_table_fullness = np.zeros(1023)
        allocated_regions = []
        try:
            for line in self.vm.follow_logfile(marker="KMM:"):
                if len(line)==19 and line=="KMM: logstate begin":
                    break
            for line in self.vm.follow_logfile(marker="KMM:"):
                if line[:12]!="KMM: pagedir":
                    # There will definitely be atleast one regions allocated: the kernel or bios regions for example
                    self.assertEqual(line[:16], "KMM: regionbegin")
                    integersFromLine = re.findall(r'\d+', line)
                    allocated_regions.append([int(integersFromLine[0])])
                    break
                else:
                    integersFromLine = re.findall(r'\d+', line)
                    page_table_fullness[int(integersFromLine[0])] = int(integersFromLine[1])
            for line in self.vm.follow_logfile(marker="KMM:"):
                # KMM: logstate end
                if len(line)==17 and line=="KMM: logstate end":
                    break
                elif len(line)>16 and line[:16]=="KMM: regionbegin":
                    integersFromLine = re.findall(r'\d+', line)
                    allocated_regions.append([int(integersFromLine[0])])
                else:
                    integersFromLine = re.findall(r'\d+', line)
                    allocated_regions[len(allocated_regions)-1].append(int(integersFromLine[0]))
        except TimeoutError as e:
            self.fail("getKmmState timed out while waiting for a KMM log message")
        
        return page_table_fullness, allocated_regions
    
    def checkRegionIncludedInKmmAllocatedRegionsList(self, allocated_regions: List[List[int]], region: List[int]) -> bool:
        # allocated regions should be a sorted list
        for allocated_region in allocated_regions:
            if allocated_region[0] > region[0]:
                return False
            elif allocated_region[1]>=region[1]:
                return (allocated_region[0]<=region[0])
        return False

    def check_page_table_fullness_correct(self, page_table_fullness, allocated_regions) -> None:
        num_allocated_page_table_pages = np.zeros(1023)

        for allocated_region in allocated_regions:
            for i in range(allocated_region[0], allocated_region[1]+1):
                page_table_index = i//1024
                num_allocated_page_table_pages[page_table_index] += 1
        
        page_table_fullness_is_correct = (
            ((num_allocated_page_table_pages==0) & (page_table_fullness==0)) | 
            (((num_allocated_page_table_pages>0) & (num_allocated_page_table_pages<1024)) & (page_table_fullness==1)) | 
            ((num_allocated_page_table_pages==1024) & (page_table_fullness==2)))
        page_table_fullness_is_correct_for_all = page_table_fullness_is_correct.all()
        if not page_table_fullness_is_correct_for_all:
            index_where_false = np.argwhere(page_table_fullness_is_correct==False)[0, 0]
            self.fail(f"Page table {index_where_false} has {num_allocated_page_table_pages[index_where_false]} "
                f"allocated pages but fullness code is {page_table_fullness[index_where_false]}")

    def test_kmm_constructor_kmm_should_be_initialized_correctly(self) -> None:
        bios_map_entries, kernel_start, kernel_end = self.getMemoryLayout()
        page_table_fullness, allocated_regions = self.getKmmState()

        self.check_page_table_fullness_correct(page_table_fullness, allocated_regions)

        for bios_map_entry in bios_map_entries:
            if bios_map_entry["region_type"]==1 or bios_map_entry["region_type"]==3 or (bios_map_entry["extended_attributes"] & 0x1)==0:
                continue
            
            first_page = bios_map_entry["region_addr"]

            if first_page > KernelMemoryManagerTests.LAST_ACCESSIBLE_PAGE:
                continue
            
            last_page = (bios_map_entry["region_addr"]+bios_map_entry["region_length"]-1)

            if last_page > KernelMemoryManagerTests.LAST_ACCESSIBLE_PAGE:
                last_page = KernelMemoryManagerTests.LAST_ACCESSIBLE_PAGE
            
            bios_map_entry_included_in_allocated_regions = self.checkRegionIncludedInKmmAllocatedRegionsList(allocated_regions, 
                [first_page//4096, last_page//4096])
            
            if not bios_map_entry_included_in_allocated_regions:
                region_addr = hex(bios_map_entry["region_addr"])
                region_length = hex(bios_map_entry["region_length"])
                region_type = bios_map_entry["region_type"]
                extended_attributes = bios_map_entry["extended_attributes"]
                self.fail(f"Some BIOS map entry was not allocated according to the kernel memory manager: " 
                    f"{region_addr} {region_length} {region_type} {extended_attributes}")
        
        for i in range(KernelMemoryManagerTests.NUM_PAGE_TABLES_FOR_KERNEL):
            kernel_space_included_in_allocated_regions = self.checkRegionIncludedInKmmAllocatedRegionsList(allocated_regions, 
                [(i*0x400000)//4096, ((i+1)*0x400000-1)//4096])
            self.assertTrue(kernel_space_included_in_allocated_regions, f"Kernel space page table {i} was not allocated according to the kernel memory manager")