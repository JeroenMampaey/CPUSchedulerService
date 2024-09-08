#!/usr/bin/python3
import sys
from pathlib import Path
sys.path.append("../python_lib")

from makefile_helper import MakefileHelper
from typing import List
import subprocess
import yaml
import os

from colorama import Fore, Back, Style

def run_tests(tests_list: List[str]) -> None:
    with open(Path(__file__).parent.absolute() / "config.yaml", 'r') as file:
        config = yaml.safe_load(file)

    current_directory = os.path.dirname(os.path.abspath(__file__))
    makefile_helper = MakefileHelper(config["make_command"], current_directory)
    makefile_helper.make_clean()

    # Compile the tests
    for tests in tests_list:
        makefile_helper.make_target(f"{tests}.exe", f"CPPFLAGS=-DUNIT_TESTING=1")

    print(Fore.GREEN+"----------------------------------------------------------------------")
    print("Compilation done, now running tests...")
    print("----------------------------------------------------------------------"+Style.RESET_ALL)
    
    # Run the tests
    for tests in tests_list:
        current_directory = os.getcwd()
        os.chdir(current_directory)

        full_command = config["execute_command"].split() + [f"./{tests}.exe"]
        try:
            # Run the command
            process = subprocess.Popen(full_command, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)

            # Process output line by line
            for line in process.stdout:
                print(line, end='')

            # Process errors if there are any
            for line in process.stderr:
                print(line, end='', file=sys.stderr)

            # Wait for the process to complete
            process.wait()
            if process.returncode != 0:
                print(f"Command failed with return code {process.returncode}", file=sys.stderr)
        except Exception as e:
            print(f"Error running command: {e}", file=sys.stderr)
        
def main():
    tests_list = [
        "page_allocator_tests",
        "network_stack_handler_tests"
    ]

    run_tests(tests_list)

if __name__ == '__main__':
    main()