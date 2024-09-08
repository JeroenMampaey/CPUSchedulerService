#pragma once

#include <gtest/gtest.h>
#include "../../operating_system/network_management_task/network_stack_handler.h"

#include <iostream>
#include <cstring>
#include <memory>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <netinet/ether.h>
#include <arpa/inet.h>
#include <tuple>

#define UDP_HDRLEN 8
#define IP4_HDRLEN 20
#define ETH_HDRLEN 14

#define NUM_PACKET_BUFFERS 50
#define ARP_HASH_TABLE_SIZE 10
#define ARP_HASH_ENTRY_LIST_SIZE 5

class NetworkStackHandlerTests : public ::testing::Test {
    public:
        // Mac and IP of the operating system which is handled by the network stack handler
        static unsigned char osMac[6];
        static unsigned int osIP;

        // Mac and IP of some client machine communicating with the operating system handled by the network stack handler
        static unsigned char clientMac[6];
        static unsigned int clientIP;

        // Mac and IP of some client machine communicating with the operating system handled by the network stack handler
        // Client is in different subnet as the operating system
        static unsigned char client2Mac[6];
        static unsigned int client2IP;

        // Mac and IP of the local gateway
        static unsigned char gatewayMac[6];
        static unsigned int gatewayIP;

        // Network mask
        static unsigned int networkMask;
        
    protected:
        NetworkStackHandler<NUM_PACKET_BUFFERS, ARP_HASH_TABLE_SIZE, ARP_HASH_ENTRY_LIST_SIZE>* pNetworkStackHandler = nullptr;

        static void SetUpTestCase();

        virtual void SetUp();

        virtual void TearDown();

        std::tuple<unsigned int, unsigned int, unsigned int> getBuffersFullness();

        unsigned short ipv4Checksum(void *b, int len);

        std::string generateRandomString(unsigned int length);

        std::vector<std::pair<std::unique_ptr<unsigned char[]>, unsigned int>> convertUDPToEthernetPackets(
            const unsigned char* src_mac,
            const unsigned char* dst_mac,
            unsigned int src_ip,
            unsigned int dst_ip,
            unsigned short src_port,
            unsigned short dst_port,
            const char* data,
            unsigned int data_len,
            unsigned int maxFragmentSize,
            unsigned short identification);
        
        ARPEntry getARPEntry(unsigned int IP);

        std::pair<std::unique_ptr<unsigned char[]>, unsigned int> createARPReply(unsigned char* replyMAC, unsigned char* arpRequest);

        std::pair<std::unique_ptr<unsigned char[]>, unsigned int> createARPRequest(unsigned char* senderMac, unsigned int senderIP, unsigned int requestedIP);
};