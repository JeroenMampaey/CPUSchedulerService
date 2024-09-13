# What does this task do?

This task sends four UDP packets to some remote machine with messages: "Hello", "To", "The", "World".

# How to test?

Well, first it is needed to specify to the task which remote machine it needs to send packets towards. In the `main.cpp` file for this task, there are two macros:

```
#define REMOTE_IP "10.0.2.3"
#define REMOTE_PORT 9000
```

Change these with the IP of the remote machine and some port which is available on this machine.

On this remote machine, run the `receive_packets.py` script which you can find in this directory. This script will first ask once again the same `REMOTE_IP` and `REMOTE_PORT` parameters. Then it will listen to incoming packets.

Next, on the machine with the CPUSchedulerService OS, launch the task from this directory. After launching the task, there should be four incoming UDP packets on the remote machine, with the following payloads:

- "Hello"
- "To"
- "The"
- "World"

*Note: Make sure on the remote machine that a firewall is not blocking the incoming UDP packets*