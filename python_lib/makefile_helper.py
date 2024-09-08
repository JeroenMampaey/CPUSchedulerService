import subprocess
import os
import time
import sys

class MakefileHelper:
    def __init__(self, make_command: str, build_directory_path: str) -> None:
        # Split make_command into a list of arguments
        self.make_command_args = make_command.split()
        self.build_directory_path = build_directory_path

    def _run_make_command(self, extra_args_list: list) -> None:
        # Change the current working directory to build_directory_path
        current_directory = os.getcwd()
        os.chdir(self.build_directory_path)
        
        # Construct the full command by combining make_command_args with extra_args_list
        full_command = self.make_command_args + extra_args_list
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
        
        # Return to the original directory after command execution
        os.chdir(current_directory)
        time.sleep(2)

    def make(self, extra_arg: str = None) -> None:
        if extra_arg is None:
            self._run_make_command([])
        else:
            self._run_make_command([extra_arg])

    def make_target(self, target: str, extra_arg: str = None) -> None:
        if extra_arg is None:
            self._run_make_command([target])
        else:
            self._run_make_command([target, extra_arg])

    def make_clean(self) -> None:
        self._run_make_command(["clean"])