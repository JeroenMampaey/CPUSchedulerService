import socket

if __name__=="__main__":
    # Ask for ip address and port as input
    ip = input("Enter the ip address of this machine: ")
    port = int(input("Enter the port number to listen on: "))

    # Create a socket object
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

    # Set a timeout for the socket, this ensures that we can stop this script using Ctrl+C
    s.settimeout(5)

    # Bind the socket to the ip address and port
    s.bind((ip, port))

    # Receive data from the socket
    while True:
        try:
            data, addr = s.recvfrom(1024)
            print(f"Received data: \"{data.decode()}\" from {addr}")
        except socket.timeout:
            continue