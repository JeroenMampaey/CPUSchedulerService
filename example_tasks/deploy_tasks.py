#!/usr/bin/python3
import sys
from pathlib import Path
sys.path.append("../python_lib")

from makefile_helper import MakefileHelper

import yaml

from optparse import OptionParser

import asyncio
from aiocoap import Context, Message, GET, PUT, ContentFormat

USER_TASK_PROCESS_ENTRY_ADDRESS = 0x2024000

async def deploy_task(ip: str, port: int, image_path: str, id: int) -> None:
    context = await Context.create_client_context()

    uri = f"coap://{ip}:{port}/tasks/{id}"
    request = Message(code=PUT, uri=uri)
    try:
        response = await context.request(request).response
        print(f"Response code for task creation is: {response.code}")
        if response.payload:
            print(f"Response payload: {response.payload.decode()}")
    except Exception as e:
        print(f"Task creation failed: {e}")
    
    uri = f"coap://{ip}:{port}/tasks/{id}/state"
    request = Message(code=GET, uri=uri)
    try:
        response = await context.request(request).response
        print(f"Response code for task state get is: {response.code}")
        if response.payload:
            print(f"Response payload: {response.payload.decode()}")
    except Exception as e:
        print(f"Task state get failed: {e}")

    if response.payload.decode() == "Running":
        print("Task is already running")
        return
    
    address = USER_TASK_PROCESS_ENTRY_ADDRESS
    uri = f"coap://{ip}:{port}/tasks/{id}/data/{address}"
    with open(image_path, 'rb') as file:
        payload = file.read()
    request = Message(code=PUT, uri=uri, content_format=ContentFormat.OCTETSTREAM, payload=payload)
    try:
        response = await context.request(request, handle_blockwise=False).response
        print(f"Response code for task data set is: {response.code}")
        if response.payload:
            print(f"Response payload: {response.payload.decode()}")
    except Exception as e:
        print(f"Task data set failed: {e}")
    
    uri = f"coap://{ip}:{port}/tasks/{id}/state"
    request = Message(code=PUT, uri=uri, content_format=ContentFormat.TEXT, payload=b"Running")
    try:
        response = await context.request(request).response
        print(f"Response code for task state set is: {response.code}")
        if response.payload:
            print(f"Response payload: {response.payload.decode()}")
    except Exception as e:
        print(f"Task state set failed: {e}")


def main():
    parser = OptionParser()
    parser.add_option("--task_id", dest="task_id", help="Id of the task which is desired to be deployed", type="int")
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
            asyncio.run(deploy_task(ip, port, config['tasks'][task]['image_path'], task_id))
            exit(0)
    
    parser.error("Task id is not found in the config file")

if __name__=='__main__':
    main()