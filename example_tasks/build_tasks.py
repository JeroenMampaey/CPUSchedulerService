#!/usr/bin/python3
import sys
from pathlib import Path
sys.path.append("../python_lib")

from makefile_helper import MakefileHelper

import yaml

from optparse import OptionParser

def build_task(make_command: str, make_directory_path: str) -> None:
    makefile_helper = MakefileHelper(make_command, make_directory_path)
    makefile_helper.make_clean()
    makefile_helper.make()

def main():
    parser = OptionParser()

    with open(Path(__file__).parent.absolute() / "config.yaml", 'r') as file:
        config = yaml.safe_load(file)

    for task in config['tasks']:
        build_task(config['tasks'][task]['make_command'], config['tasks'][task]['make_directory_path'])

if __name__=='__main__':
    main()