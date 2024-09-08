#include "network_stack_handler_tests_fixture.h"

unsigned char NetworkStackHandlerTests::osMac[6] = {};
unsigned int NetworkStackHandlerTests::osIP = 0;
unsigned char NetworkStackHandlerTests::clientMac[6] = {};
unsigned int NetworkStackHandlerTests::clientIP = 0;
unsigned char NetworkStackHandlerTests::client2Mac[6] = {};
unsigned int NetworkStackHandlerTests::client2IP = 0;
unsigned char NetworkStackHandlerTests::gatewayMac[6] = {};
unsigned int NetworkStackHandlerTests::gatewayIP = 0;
unsigned int NetworkStackHandlerTests::networkMask = 0;

// Mocking the atomics.h functions
extern "C" void atomicStore(unsigned int* destination, unsigned int value){
    *destination = value;
}

extern "C" bool conditionalExchange(unsigned int* destination, unsigned int expected, unsigned int desired){
    if(*destination == expected){
        *destination = desired;
        return true;
    }

    return false;
}

// Mocking the memCopy function
void memCopy(unsigned char* from, unsigned char* to, unsigned int numBytes){
    for(int i = 0; i < numBytes; i++){
        to[i] = from[i];
    }
}

// Mocking the memClear function
void memClear(unsigned char* addr, unsigned int numBytes){
    for(int i = 0; i < numBytes; i++){
        addr[i] = 0;
    }
}

void NetworkStackHandlerTests::SetUpTestCase(){
    // Exact values are not very important
    for(int i=0; i<6; i++){
        osMac[i] = i;
    }
    osIP = (((unsigned int)192) << 24) + (((unsigned int)168) << 16) + (((unsigned int)0) << 8) + ((unsigned int)10);
    
    for(int i=0; i<6; i++){
        clientMac[i] = i+6;
    }
    clientIP = (((unsigned int)192) << 24) + (((unsigned int)168) << 16) + (((unsigned int)0) << 8) + ((unsigned int)11);

    for(int i=0; i<6; i++){
        client2Mac[i] = 2*i+6;
    }
    client2IP = (((unsigned int)191) << 24) + (((unsigned int)168) << 16) + (((unsigned int)0) << 8) + ((unsigned int)10);

    for(int i=0; i<6; i++){
        gatewayMac[i] = 200-i;
    }
    gatewayIP = (((unsigned int)192) << 24) + (((unsigned int)168) << 16) + (((unsigned int)0) << 8) + ((unsigned int)1);

    networkMask = 24;
}

void NetworkStackHandlerTests::SetUp(){
    pNetworkStackHandler = new NetworkStackHandler<NUM_PACKET_BUFFERS, ARP_HASH_TABLE_SIZE, ARP_HASH_ENTRY_LIST_SIZE>(osMac, osIP, gatewayIP, networkMask);
}

void NetworkStackHandlerTests::TearDown(){
    delete pNetworkStackHandler;
}

std::tuple<unsigned int, unsigned int, unsigned int> NetworkStackHandlerTests::getBuffersFullness(){
    DoublyLinkedListElement<IPv4Packet>* iterator = pNetworkStackHandler->unusedPacketsHead;
    unsigned int i=0;
    for(; iterator!=nullptr; i++, iterator=iterator->next);

    iterator = pNetworkStackHandler->incompleteFragmentedDatagramsHead;
    unsigned int j=0;
    for(; iterator!=nullptr; j++, iterator=iterator->next);

    iterator = pNetworkStackHandler->readyToReadPacketsHead;
    unsigned int k=0;
    for(; iterator!=nullptr; k++, iterator=iterator->next);

    return {i, j, k};
}

unsigned short NetworkStackHandlerTests::ipv4Checksum(void *b, int len){    
    unsigned short *buf = (unsigned short *)b;
    unsigned int sum = 0;
    unsigned short result;

    for (sum = 0; len > 1; len -= 2)
        sum += *buf++;
    if (len == 1)
        sum += *(unsigned char *)buf;
    sum = (sum >> 16) + (sum & 0xFFFF);
    sum += (sum >> 16);
    result = ~sum;
    return result;
}

std::vector<std::pair<std::unique_ptr<unsigned char[]>, unsigned int>> NetworkStackHandlerTests::convertUDPToEthernetPackets(
    const unsigned char* src_mac,
    const unsigned char* dst_mac,
    unsigned int src_ip,
    unsigned int dst_ip,
    unsigned short src_port,
    unsigned short dst_port,
    const char* data,
    unsigned int data_len,
    unsigned int maxFragmentSize,
    unsigned short identification) {
    std::vector<std::pair<std::unique_ptr<unsigned char[]>, unsigned int>> fragments;
    unsigned int remainingIPv4Size = data_len + UDP_HDRLEN;

    while(remainingIPv4Size > 0){
        unsigned int fragmentSize = std::min(remainingIPv4Size, maxFragmentSize);
        std::unique_ptr<unsigned char[]> fragment(new unsigned char[fragmentSize + IP4_HDRLEN + ETH_HDRLEN]);
        memset(fragment.get(), 0, fragmentSize + IP4_HDRLEN + ETH_HDRLEN);

        // Ethernet header
        struct ether_header *eth = (struct ether_header *)fragment.get();
        memcpy(eth->ether_shost, src_mac, 6);
        memcpy(eth->ether_dhost, dst_mac, 6);
        eth->ether_type = htons(ETH_P_IP);

        // IP header
        struct iphdr *iph = (struct iphdr *)(fragment.get() + ETH_HDRLEN);
        iph->ihl = 5;
        iph->version = 4;
        iph->tos = 0;
        iph->tot_len = htons(IP4_HDRLEN + fragmentSize);
        iph->id = htons(identification);
        iph->frag_off = htons((data_len + UDP_HDRLEN - remainingIPv4Size) >> 3);  // Set fragment offset

        // Set more fragments flag for all fragments except the last one
        if (remainingIPv4Size > maxFragmentSize) {
            iph->frag_off |= htons(IP_MF);
        }

        iph->ttl = 255;
        iph->protocol = IPPROTO_UDP;
        iph->check = 0;  // Set to 0 before calculating checksum
        iph->saddr = htonl(src_ip);  // Source IP address
        iph->daddr = htonl(dst_ip);  // Destination IP address

        // IP checksum
        iph->check = ipv4Checksum((unsigned short *)(fragment.get() + ETH_HDRLEN), IP4_HDRLEN);

        bool isFirstFragment = (data_len + UDP_HDRLEN == remainingIPv4Size);
        
        // UDP header
        if(isFirstFragment){
            struct udphdr *udph = (struct udphdr *)(fragment.get() + ETH_HDRLEN + IP4_HDRLEN);
            udph->source = htons(src_port);
            udph->dest = htons(dst_port);
            udph->len = htons(UDP_HDRLEN + data_len);
            udph->check = 0;
        }

        // Copy the fragment data into the packet
        memcpy(fragment.get() + ETH_HDRLEN + IP4_HDRLEN + (isFirstFragment ? UDP_HDRLEN : 0), data + (data_len + (isFirstFragment ? UDP_HDRLEN : 0) - remainingIPv4Size), fragmentSize - (isFirstFragment ? UDP_HDRLEN : 0));

        // Add the fragment to the vector
        fragments.push_back({std::move(fragment), fragmentSize + IP4_HDRLEN + ETH_HDRLEN});

        remainingIPv4Size -= fragmentSize;
    } 

    return fragments;
}

std::string NetworkStackHandlerTests::generateRandomString(unsigned int length){
    std::string randomString = std::string(length, 0);
    for(int i=0; i<length; i++){
        unsigned int newCharOffset =  (unsigned int)(rand() % 10);
        char newChar = '0'+newCharOffset;
        randomString[i] = newChar;
    }

    return randomString;
}

ARPEntry NetworkStackHandlerTests::getARPEntry(unsigned int IP){
    ARPEntry retval;
    unsigned int hashedIP = IP % ARP_HASH_TABLE_SIZE;

    for(int i=0; i<ARP_HASH_ENTRY_LIST_SIZE; i++){
        if(pNetworkStackHandler->arpEntries[hashedIP*ARP_HASH_ENTRY_LIST_SIZE + i].IP == IP){
            retval = pNetworkStackHandler->arpEntries[hashedIP*ARP_HASH_ENTRY_LIST_SIZE + i];
            return retval;
        }
    }

    retval.IP = 0;
    retval.lastUsed = -1;
    retval.lastRequested = -1;
    retval.lastAnswered = -1;
    for(int i=0; i<6; i++) retval.MAC[i] = 0;

    return retval;
}

std::pair<std::unique_ptr<unsigned char[]>, unsigned int> NetworkStackHandlerTests::createARPReply(unsigned char* replyMAC, unsigned char* arpRequest){
    arpRequest += ETHERNET_SIMPLE_HEADER_SIZE;
    // Ethernet header size + ARP packet size
    unsigned int totalSize = ETHERNET_SIMPLE_HEADER_SIZE + 28; // Ethernet header is 14 bytes

    // Allocate memory for the ARP reply with Ethernet header
    std::unique_ptr<unsigned char[]> arpReply(new unsigned char[totalSize]);

    // Construct Ethernet header
    // Destination MAC (assuming ARP request sender's MAC is the target)
    for (int i = 0; i < 6; ++i) {
        arpReply[i] = arpRequest[8 + i]; // Copy sender MAC from ARP request to destination MAC in Ethernet header
    }
    // Source MAC
    for (int i = 0; i < 6; ++i) {
        arpReply[6 + i] = replyMAC[i];
    }
    // EtherType for ARP
    arpReply[12] = 0x08; // ARP EtherType high byte
    arpReply[13] = 0x06; // ARP EtherType low byte

    // Copy the ARP request into the reply buffer to start modifications, offset by Ethernet header size
    std::memcpy(arpReply.get() + ETHERNET_SIMPLE_HEADER_SIZE, arpRequest, 28);

    // Adjust indices for ARP fields due to the Ethernet header offset
    int arpOffset = ETHERNET_SIMPLE_HEADER_SIZE;

    // Set the operation to 2 (ARP reply)
    arpReply[arpOffset + 6] = 0x00;
    arpReply[arpOffset + 7] = 0x02;

    // Set the sender MAC address to replyMAC
    for (int i = 0; i < 6; ++i) {
        arpReply[arpOffset + 8 + i] = replyMAC[i];
    }

    // Swap sender and target hardware addresses in the ARP payload
    for (int i = 0; i < 6; ++i) {
        // Save the original sender MAC to the target MAC location
        arpReply[arpOffset + 18 + i] = arpReply[arpOffset + 8 + i];
        // Set the sender MAC to replyMAC
        arpReply[arpOffset + 8 + i] = replyMAC[i];
    }

    // Swap sender and target protocol addresses in the ARP payload
    for (int i = 0; i < 4; ++i) {
        // Temporary storage for the sender IP
        unsigned char temp = arpReply[arpOffset + 14 + i];
        // Set the sender IP to the target IP
        arpReply[arpOffset + 14 + i] = arpReply[arpOffset + 24 + i];
        // Set the target IP to the original sender IP
        arpReply[arpOffset + 24 + i] = temp;
    }

    // Return the ARP reply with its size
    return std::pair<std::unique_ptr<unsigned char[]>, unsigned int>(std::move(arpReply), totalSize);
}

std::pair<std::unique_ptr<unsigned char[]>, unsigned int> NetworkStackHandlerTests::createARPRequest(unsigned char* senderMac, unsigned int senderIP, unsigned int requestedIP){
    // Ethernet header size + ARP packet size
    unsigned int totalSize = ETHERNET_SIMPLE_HEADER_SIZE + 28; // Ethernet header is 14 bytes

    // Allocate memory for the ARP request with Ethernet header
    std::unique_ptr<unsigned char[]> arpRequest(new unsigned char[totalSize]);

    // Construct Ethernet header
    // Broadcast destination MAC (FF:FF:FF:FF:FF:FF)
    for (int i = 0; i < 6; ++i) {
        arpRequest[i] = 0xFF;
    }
    // Source MAC
    for (int i = 0; i < 6; ++i) {
        arpRequest[6 + i] = senderMac[i];
    }
    // EtherType for ARP
    arpRequest[12] = 0x08; // ARP EtherType high byte
    arpRequest[13] = 0x06; // ARP EtherType low byte

    // ARP packet starts here
    unsigned int arpOffset = ETHERNET_SIMPLE_HEADER_SIZE;
    // Hardware type (Ethernet)
    arpRequest[arpOffset] = 0x00;
    arpRequest[arpOffset + 1] = 0x01;
    // Protocol type (IP)
    arpRequest[arpOffset + 2] = 0x08;
    arpRequest[arpOffset + 3] = 0x00;
    // Hardware size
    arpRequest[arpOffset + 4] = 0x06;
    // Protocol size
    arpRequest[arpOffset + 5] = 0x04;
    // Opcode (request)
    arpRequest[arpOffset + 6] = 0x00;
    arpRequest[arpOffset + 7] = 0x01;
    // Sender MAC address
    for (int i = 0; i < 6; ++i) {
        arpRequest[arpOffset + 8 + i] = senderMac[i];
    }
    // Sender IP address
    arpRequest[arpOffset + 14] = (senderIP >> 24) & 0xFF;
    arpRequest[arpOffset + 15] = (senderIP >> 16) & 0xFF;
    arpRequest[arpOffset + 16] = (senderIP >> 8) & 0xFF;
    arpRequest[arpOffset + 17] = senderIP & 0xFF;
    // Target MAC address (zeros for request)
    for (int i = 0; i < 6; ++i) {
        arpRequest[arpOffset + 18 + i] = 0x00;
    }
    // Target IP address
    arpRequest[arpOffset + 24] = (requestedIP >> 24) & 0xFF;
    arpRequest[arpOffset + 25] = (requestedIP >> 16) & 0xFF;
    arpRequest[arpOffset + 26] = (requestedIP >> 8) & 0xFF;
    arpRequest[arpOffset + 27] = requestedIP & 0xFF;

    // Return the ARP request with its size
    return std::make_pair(std::move(arpRequest), totalSize);
}