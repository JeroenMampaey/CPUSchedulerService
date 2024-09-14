# What does this task do?

This task waits for receiving UDP packets on port 1000 and prints them to the screen.

# How to test?

On some remote machine, run the `send_packets.py` script, which you can find in this directory. This script will first ask for the IP address of the machine on which the CPUSchedulerService OS is running. Then it will ask for user input on which packets to send.

Next, on the machine with the CPUSchedulerService OS, launch the task from this directory. After launching the task, it should print `"Waiting for packets..."`.

Next, go back to the remote machine and send some packets, they should get printed on the screen of the CPUSchedulerService OS machine.