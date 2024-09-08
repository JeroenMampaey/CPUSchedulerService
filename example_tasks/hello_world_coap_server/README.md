# What does this task do?

This task runs a CoAP Server on port 6000, which just returns "Hello World!" to incoming GET requests.

# How to test?

On some machine in the same network as the machine where the CPUSchedulerService operating system is deployed, install the [`coap-client`](https://manpages.ubuntu.com/manpages/focal/man5/coap-client.5.html) application:

```
sudo apt-get install libcoap2-bin
```

Then deploy this task on the CPUSchedulerService OS, and from the other machine run:

```
coap-client -m get coap://{CPU_SCHEDULER_SERVICE_MACHINE_IP}:6000
```

Which should then return:

```
Hello World!
```