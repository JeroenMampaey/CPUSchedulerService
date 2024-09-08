#!/usr/bin/python3
import sys
from pathlib import Path
sys.path.append("../python_lib")

from makefile_helper import MakefileHelper

import yaml

from optparse import OptionParser

import asyncio
from aiocoap import Context, Message, DELETE
import random

async def remove_task(ip: str, port: int, id: int) -> None:
    context = await Context.create_client_context()

    uri = f"coap://{ip}:{port}/tasks/{id}"
    request = Message(code=DELETE, uri=uri)
    try:
        response = await context.request(request).response
        print(f"Response code for task deletion is: {response.code}")
        if response.payload:
            print(f"Response payload: {response.payload.decode()}")
    except Exception as e:
        print(f"Task deletion failed: {e}")


def main():
    parser = OptionParser()
    parser.add_option("--task_id", dest="task_id", help="Id of the task which is desired to be removed", type="int")
    options, _ = parser.parse_args(sys.argv)
    if options.task_id is None:
        parser.error("Task id is required")
    task_id = options.task_id

    with open(Path(__file__).parent.absolute() / "config.yaml", 'r') as file:
        config = yaml.safe_load(file)
    
    ip = config['operating_system']["ip"]
    port = 5683

    for task in config['tasks']:
        if config['tasks'][task]['id']==task_id:
            asyncio.run(remove_task(ip, port, task_id))
            exit(0)
    
    parser.error("Task id is not found in the config file")

if __name__=='__main__':
    main()