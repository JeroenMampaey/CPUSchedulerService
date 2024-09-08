#!/usr/bin/python3
import sys
from pathlib import Path
sys.path.append("../../python_lib")

from vm_tests import VmTests

class BasicTests(VmTests):
    @classmethod
    def setUpClass(self) -> None:
        super().setUpClass()

    def setUp(self) -> None:
        super().setUp()
        
        self.vm.start()
    
    def tearDown(self) -> None:
        self.vm.stop()

    def test_cpu_core_log_should_show_constructor_and_while_loop_start(self) -> None:
        num_expected_logs = 2
        num_actual_logs = 0
        try:
            for line in self.vm.follow_logfile(marker="CpuCore:"):
                self.assertEqual(line, "CpuCore: constructor", "First cpu core log line was wrong")
                break
            num_actual_logs += 1
            for line in self.vm.follow_logfile(marker="Main:"):
                self.assertEqual(line, "Main: going into endless loop", "First main log line was wrong")
                break
            num_actual_logs += 1
        except TimeoutError as e:
            self.fail(f"Expected {num_expected_logs} logs in the cpu core but only encountered {num_actual_logs} actual logs")
        self.assertEqual(num_expected_logs, num_actual_logs, f"Expected {num_expected_logs} logs in the cpu core but only encountered {num_actual_logs} actual logs")