import socket

if __name__ == "__main__":
    # Ask for ip address as input
    ip = input("Enter the ip address of the target, running the CPUSchedulerService OS: ")
    
    # Create a socket object (UDP)
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

    # Loop to send messages
    while True:
        message_to_send = input("Enter the message to send (or use Ctrl+C to quit): ")
        
        # Send the message to the specified IP and port
        s.sendto(message_to_send.encode(), (ip, 1000))
    
    s.close()