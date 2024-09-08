#include "network_stack_handler_tests_fixture.h"
#include <algorithm>
#include <random>
#include <chrono>
#include <iostream>

#define MAX_IPV4_FRAGMENT_SIZE 1480
#define UDP_IPV4_PROTOCOL 17
#define UDP_HEADER_SIZE 8

TEST_F(NetworkStackHandlerTests, NormalPacket_ShouldBeGettable){
    std::tuple<unsigned int, unsigned int, unsigned int> buffersFullness = getBuffersFullness();
    ASSERT_EQ(buffersFullness, std::make_tuple(NUM_PACKET_BUFFERS, 0, 0));

    int src_port = 54321;
    int dst_port = 6000;
    const char* data = "Hello World!";
    int data_len = strlen(data);
    unsigned int maxFragmentSize = MAX_IPV4_FRAGMENT_SIZE;
    unsigned short identification = 1;

    std::vector<std::pair<std::unique_ptr<unsigned char[]>, unsigned int>> packets = convertUDPToEthernetPackets(NetworkStackHandlerTests::clientMac, 
        NetworkStackHandlerTests::osMac,
        NetworkStackHandlerTests::clientIP,
        NetworkStackHandlerTests::osIP,
        src_port, dst_port, data, data_len, maxFragmentSize, identification);

    PacketType packetType = pNetworkStackHandler->handleIncomingEthernetPacket(packets[0].first.get(), ETH_HDRLEN + IP4_HDRLEN + UDP_HDRLEN + data_len);
    ASSERT_EQ(static_cast<int>(packetType), static_cast<int>(PacketType::IPv4Packet));

    buffersFullness = getBuffersFullness();
    ASSERT_EQ(buffersFullness, std::make_tuple(NUM_PACKET_BUFFERS-1, 0, 1));

    IPv4Packet* pIPv4Packet = pNetworkStackHandler->getLatestIPv4Packet();
    ASSERT_EQ(pIPv4Packet->dataSize, data_len + UDP_HDRLEN);
    ASSERT_EQ(memcmp(pIPv4Packet->pData->data + UDP_HDRLEN, data, data_len), 0);
    pNetworkStackHandler->popLatestIPv4Packet();

    buffersFullness = getBuffersFullness();
    ASSERT_EQ(buffersFullness, std::make_tuple(NUM_PACKET_BUFFERS, 0, 0));
}

TEST_F(NetworkStackHandlerTests, MultipleNormalPackets_ShouldBeGettableInOrder){
    int src_port = 54321;
    int dst_port = 6000;
    unsigned int maxFragmentSize = MAX_IPV4_FRAGMENT_SIZE;
    unsigned short identification = 1;

    std::vector<std::pair<std::unique_ptr<unsigned char[]>, unsigned int>> packets;
    unsigned int numPackets = 20;

    for(int i=0; i<numPackets; i++){
        int dataLen = (rand() % (maxFragmentSize-UDP_HDRLEN)) + 1;
        std::string data = generateRandomString(dataLen);

        std::vector<std::pair<std::unique_ptr<unsigned char[]>, unsigned int>> newPacket = convertUDPToEthernetPackets(NetworkStackHandlerTests::clientMac, 
            NetworkStackHandlerTests::osMac,
            NetworkStackHandlerTests::clientIP,
            NetworkStackHandlerTests::osIP,
            src_port, dst_port, data.c_str(), data.size(), maxFragmentSize, identification);
        
        packets.insert(packets.end(),
            std::make_move_iterator(newPacket.begin()),
            std::make_move_iterator(newPacket.end())
        );
    }

    ASSERT_EQ(packets.size(), numPackets);

    for(int i=0; i<numPackets; i++){
        PacketType packetType = pNetworkStackHandler->handleIncomingEthernetPacket(packets[i].first.get(), packets[i].second);
        ASSERT_EQ(static_cast<int>(packetType), static_cast<int>(PacketType::IPv4Packet));

        std::tuple<unsigned int, unsigned int, unsigned int> buffersFullness = getBuffersFullness();
        ASSERT_EQ(buffersFullness, std::make_tuple(NUM_PACKET_BUFFERS-(i+1), 0, i+1));
    }

    for(int i=0; i<numPackets; i++){
        IPv4Packet* pIPv4Packet = pNetworkStackHandler->getLatestIPv4Packet();
        ASSERT_EQ(pIPv4Packet->dataSize, packets[i].second - ETH_HDRLEN - IP4_HDRLEN);
        ASSERT_EQ(memcmp(pIPv4Packet->pData->data + UDP_HDRLEN, packets[i].first.get() + ETH_HDRLEN + IP4_HDRLEN + UDP_HDRLEN, pIPv4Packet->dataSize - UDP_HDRLEN), 0);
        pNetworkStackHandler->popLatestIPv4Packet();

        std::tuple<unsigned int, unsigned int, unsigned int> buffersFullness = getBuffersFullness();
        ASSERT_EQ(buffersFullness, std::make_tuple(NUM_PACKET_BUFFERS-(numPackets-i-1), 0, (numPackets-i-1)));
    }
}

TEST_F(NetworkStackHandlerTests, NormalFragmentedPacket_ShouldBeGettable){
    int src_port = 54321;
    int dst_port = 6000;
    unsigned int maxFragmentSize = MAX_IPV4_FRAGMENT_SIZE;
    unsigned short identification = 1;

    for(int i=1; i<20; i++){
        std::tuple<unsigned int, unsigned int, unsigned int> buffersFullness = getBuffersFullness();
        ASSERT_EQ(buffersFullness, std::make_tuple(NUM_PACKET_BUFFERS, 0, 0));

        std::string data = generateRandomString((MAX_IPV4_FRAGMENT_SIZE/2)*i-UDP_HDRLEN);

        std::vector<std::pair<std::unique_ptr<unsigned char[]>, unsigned int>> packets = convertUDPToEthernetPackets(NetworkStackHandlerTests::clientMac, 
            NetworkStackHandlerTests::osMac,
            NetworkStackHandlerTests::clientIP,
            NetworkStackHandlerTests::osIP,
            src_port, dst_port, data.c_str(), data.size(), maxFragmentSize, identification);

        ASSERT_EQ(packets.size(), (i+1)/2);

        for(int j=0; j<packets.size(); j++){
            PacketType packetType = pNetworkStackHandler->handleIncomingEthernetPacket(packets[j].first.get(), packets[j].second);
            ASSERT_EQ(static_cast<int>(packetType), static_cast<int>(PacketType::IPv4Packet));

            std::tuple<unsigned int, unsigned int, unsigned int> buffersFullness = getBuffersFullness();

            if(j==packets.size()-1){
                ASSERT_EQ(buffersFullness, std::make_tuple(NUM_PACKET_BUFFERS-1-j, 0, 1));
            }
            else{
                ASSERT_EQ(buffersFullness, std::make_tuple(NUM_PACKET_BUFFERS-1-j, 1, 0));
            }
        }

        IPv4Packet* pIPv4Packet = pNetworkStackHandler->getLatestIPv4Packet();
        unsigned int numIPv4Fragments = 0;
        IPv4Packet* pCurrentIPv4Packet = pIPv4Packet;
        while(pCurrentIPv4Packet->nextFragment!=nullptr){
            ASSERT_EQ(pCurrentIPv4Packet->fragmentOffset, MAX_IPV4_FRAGMENT_SIZE*numIPv4Fragments);
            ASSERT_EQ(pCurrentIPv4Packet->dataSize, MAX_IPV4_FRAGMENT_SIZE);
            
            unsigned int firstFragmentCompensation = numIPv4Fragments==0 ? UDP_HDRLEN : 0;
            ASSERT_EQ(memcmp(pCurrentIPv4Packet->pData->data + firstFragmentCompensation, 
                data.c_str() + MAX_IPV4_FRAGMENT_SIZE*numIPv4Fragments - (UDP_HDRLEN-firstFragmentCompensation), 
                pCurrentIPv4Packet->dataSize - firstFragmentCompensation), 0);
            
            numIPv4Fragments++;
            pCurrentIPv4Packet = pCurrentIPv4Packet->nextFragment;
        }

        ASSERT_EQ(pCurrentIPv4Packet->fragmentOffset, MAX_IPV4_FRAGMENT_SIZE*numIPv4Fragments);
        ASSERT_EQ(pCurrentIPv4Packet->dataSize, 
            data.size() - (MAX_IPV4_FRAGMENT_SIZE*numIPv4Fragments-UDP_HDRLEN));
        
        unsigned int firstFragmentCompensation = numIPv4Fragments==0 ? UDP_HDRLEN : 0;
        ASSERT_EQ(memcmp(pCurrentIPv4Packet->pData->data + firstFragmentCompensation, 
                data.c_str() + MAX_IPV4_FRAGMENT_SIZE*numIPv4Fragments - (UDP_HDRLEN-firstFragmentCompensation), 
                pCurrentIPv4Packet->dataSize - firstFragmentCompensation), 0);
        
        numIPv4Fragments++;
        ASSERT_EQ(numIPv4Fragments, (i+1)/2);

        pNetworkStackHandler->popLatestIPv4Packet();
    }

    std::tuple<unsigned int, unsigned int, unsigned int> buffersFullness = getBuffersFullness();
    ASSERT_EQ(buffersFullness, std::make_tuple(NUM_PACKET_BUFFERS, 0, 0));
}

TEST_F(NetworkStackHandlerTests, PacketWithOutOfOrderArrivingFragment_ShouldBeGettableAndBackInOrder){
    int src_port = 54321;
    int dst_port = 6000;
    unsigned int maxFragmentSize = MAX_IPV4_FRAGMENT_SIZE;
    unsigned short identification = 1;

    for(int i=3; i<20; i++){
        std::tuple<unsigned int, unsigned int, unsigned int> buffersFullness = getBuffersFullness();
        ASSERT_EQ(buffersFullness, std::make_tuple(NUM_PACKET_BUFFERS, 0, 0));

        std::string data = generateRandomString((MAX_IPV4_FRAGMENT_SIZE/2)*i-UDP_HDRLEN);

        std::vector<std::pair<std::unique_ptr<unsigned char[]>, unsigned int>> packets = convertUDPToEthernetPackets(NetworkStackHandlerTests::clientMac, 
            NetworkStackHandlerTests::osMac,
            NetworkStackHandlerTests::clientIP,
            NetworkStackHandlerTests::osIP,
            src_port, dst_port, data.c_str(), data.size(), maxFragmentSize, identification);

        ASSERT_EQ(packets.size(), (i+1)/2);

        std::vector<size_t> indices(packets.size());
        std::iota(indices.begin(), indices.end(), 0);
        std::shuffle(indices.begin(), indices.end(), std::default_random_engine(std::chrono::system_clock::now().time_since_epoch().count()));

        for(int j=0; j<packets.size(); j++){
            unsigned int index = indices[j];

            PacketType packetType = pNetworkStackHandler->handleIncomingEthernetPacket(packets[index].first.get(), packets[index].second);
            ASSERT_EQ(static_cast<int>(packetType), static_cast<int>(PacketType::IPv4Packet));

            std::tuple<unsigned int, unsigned int, unsigned int> buffersFullness = getBuffersFullness();

            if(j==packets.size()-1){
                ASSERT_EQ(buffersFullness, std::make_tuple(NUM_PACKET_BUFFERS-1-j, 0, 1));
            }
            else{
                ASSERT_EQ(buffersFullness, std::make_tuple(NUM_PACKET_BUFFERS-1-j, 1, 0));
            }
        }

        IPv4Packet* pIPv4Packet = pNetworkStackHandler->getLatestIPv4Packet();
        unsigned int numIPv4Fragments = 0;
        IPv4Packet* pCurrentIPv4Packet = pIPv4Packet;
        while(pCurrentIPv4Packet->nextFragment!=nullptr){
            ASSERT_EQ(pCurrentIPv4Packet->fragmentOffset, MAX_IPV4_FRAGMENT_SIZE*numIPv4Fragments);
            ASSERT_EQ(pCurrentIPv4Packet->dataSize, MAX_IPV4_FRAGMENT_SIZE);
            
            unsigned int firstFragmentCompensation = numIPv4Fragments==0 ? UDP_HDRLEN : 0;
            ASSERT_EQ(memcmp(pCurrentIPv4Packet->pData->data + firstFragmentCompensation, 
                data.c_str() + MAX_IPV4_FRAGMENT_SIZE*numIPv4Fragments - (UDP_HDRLEN-firstFragmentCompensation), 
                pCurrentIPv4Packet->dataSize - firstFragmentCompensation), 0);
            
            numIPv4Fragments++;
            pCurrentIPv4Packet = pCurrentIPv4Packet->nextFragment;
        }

        ASSERT_EQ(pCurrentIPv4Packet->fragmentOffset, MAX_IPV4_FRAGMENT_SIZE*numIPv4Fragments);
        ASSERT_EQ(pCurrentIPv4Packet->dataSize, 
            data.size() - (MAX_IPV4_FRAGMENT_SIZE*numIPv4Fragments-UDP_HDRLEN));
        
        unsigned int firstFragmentCompensation = numIPv4Fragments==0 ? UDP_HDRLEN : 0;
        ASSERT_EQ(memcmp(pCurrentIPv4Packet->pData->data + firstFragmentCompensation, 
                data.c_str() + MAX_IPV4_FRAGMENT_SIZE*numIPv4Fragments - (UDP_HDRLEN-firstFragmentCompensation), 
                pCurrentIPv4Packet->dataSize - firstFragmentCompensation), 0);
        
        numIPv4Fragments++;
        ASSERT_EQ(numIPv4Fragments, (i+1)/2);

        pNetworkStackHandler->popLatestIPv4Packet();
    }

    std::tuple<unsigned int, unsigned int, unsigned int> buffersFullness = getBuffersFullness();
    ASSERT_EQ(buffersFullness, std::make_tuple(NUM_PACKET_BUFFERS, 0, 0));
}

TEST_F(NetworkStackHandlerTests, PacketWithReversedOrderArrivingFragment_ShouldBeGettableAndBackInOrder){
    int src_port = 54321;
    int dst_port = 6000;
    unsigned int maxFragmentSize = MAX_IPV4_FRAGMENT_SIZE;
    unsigned short identification = 1;

    for(int i=3; i<20; i++){
        std::tuple<unsigned int, unsigned int, unsigned int> buffersFullness = getBuffersFullness();
        ASSERT_EQ(buffersFullness, std::make_tuple(NUM_PACKET_BUFFERS, 0, 0));

        std::string data = generateRandomString((MAX_IPV4_FRAGMENT_SIZE/2)*i-UDP_HDRLEN);

        std::vector<std::pair<std::unique_ptr<unsigned char[]>, unsigned int>> packets = convertUDPToEthernetPackets(NetworkStackHandlerTests::clientMac, 
            NetworkStackHandlerTests::osMac,
            NetworkStackHandlerTests::clientIP,
            NetworkStackHandlerTests::osIP,
            src_port, dst_port, data.c_str(), data.size(), maxFragmentSize, identification);

        ASSERT_EQ(packets.size(), (i+1)/2);

        for(int j=0; j<packets.size(); j++){
            unsigned int index = packets.size()-1-j;
            
            PacketType packetType = pNetworkStackHandler->handleIncomingEthernetPacket(packets[index].first.get(), packets[index].second);
            ASSERT_EQ(static_cast<int>(packetType), static_cast<int>(PacketType::IPv4Packet));

            std::tuple<unsigned int, unsigned int, unsigned int> buffersFullness = getBuffersFullness();

            if(j==packets.size()-1){
                ASSERT_EQ(buffersFullness, std::make_tuple(NUM_PACKET_BUFFERS-1-j, 0, 1));
            }
            else{
                ASSERT_EQ(buffersFullness, std::make_tuple(NUM_PACKET_BUFFERS-1-j, 1, 0));
            }
        }

        IPv4Packet* pIPv4Packet = pNetworkStackHandler->getLatestIPv4Packet();
        unsigned int numIPv4Fragments = 0;
        IPv4Packet* pCurrentIPv4Packet = pIPv4Packet;
        while(pCurrentIPv4Packet->nextFragment!=nullptr){
            ASSERT_EQ(pCurrentIPv4Packet->fragmentOffset, MAX_IPV4_FRAGMENT_SIZE*numIPv4Fragments);
            ASSERT_EQ(pCurrentIPv4Packet->dataSize, MAX_IPV4_FRAGMENT_SIZE);
            
            unsigned int firstFragmentCompensation = numIPv4Fragments==0 ? UDP_HDRLEN : 0;
            ASSERT_EQ(memcmp(pCurrentIPv4Packet->pData->data + firstFragmentCompensation, 
                data.c_str() + MAX_IPV4_FRAGMENT_SIZE*numIPv4Fragments - (UDP_HDRLEN-firstFragmentCompensation), 
                pCurrentIPv4Packet->dataSize - firstFragmentCompensation), 0);
            
            numIPv4Fragments++;
            pCurrentIPv4Packet = pCurrentIPv4Packet->nextFragment;
        }

        ASSERT_EQ(pCurrentIPv4Packet->fragmentOffset, MAX_IPV4_FRAGMENT_SIZE*numIPv4Fragments);
        ASSERT_EQ(pCurrentIPv4Packet->dataSize, 
            data.size() - (MAX_IPV4_FRAGMENT_SIZE*numIPv4Fragments-UDP_HDRLEN));
        
        unsigned int firstFragmentCompensation = numIPv4Fragments==0 ? UDP_HDRLEN : 0;
        ASSERT_EQ(memcmp(pCurrentIPv4Packet->pData->data + firstFragmentCompensation, 
                data.c_str() + MAX_IPV4_FRAGMENT_SIZE*numIPv4Fragments - (UDP_HDRLEN-firstFragmentCompensation), 
                pCurrentIPv4Packet->dataSize - firstFragmentCompensation), 0);
        
        numIPv4Fragments++;
        ASSERT_EQ(numIPv4Fragments, (i+1)/2);

        pNetworkStackHandler->popLatestIPv4Packet();
    }

    std::tuple<unsigned int, unsigned int, unsigned int> buffersFullness = getBuffersFullness();
    ASSERT_EQ(buffersFullness, std::make_tuple(NUM_PACKET_BUFFERS, 0, 0));
}

TEST_F(NetworkStackHandlerTests, MultipleFragmentedPacketsArrivingAtTheSameTime_ShouldAllBeGettable){
    int src_port = 54321;
    int dst_port = 6000;
    unsigned int maxFragmentSize = MAX_IPV4_FRAGMENT_SIZE;

    for(unsigned int numDifferentPackets=3; numDifferentPackets<8; numDifferentPackets++){
        std::tuple<unsigned int, unsigned int, unsigned int> buffersFullness = getBuffersFullness();
        ASSERT_EQ(buffersFullness, std::make_tuple(NUM_PACKET_BUFFERS, 0, 0));

        std::vector<std::pair<std::unique_ptr<unsigned char[]>, unsigned int>> packets = {};
        std::vector<std::string> datas = {};
        
        for(unsigned int identification=1; identification<=numDifferentPackets; identification++){
            unsigned int i = 3 + (rand() % 17);

            std::string data = generateRandomString((MAX_IPV4_FRAGMENT_SIZE/2)*i-UDP_HDRLEN);
            datas.push_back(data);

            std::vector<std::pair<std::unique_ptr<unsigned char[]>, unsigned int>> fragments = convertUDPToEthernetPackets(NetworkStackHandlerTests::clientMac, 
                NetworkStackHandlerTests::osMac,
                NetworkStackHandlerTests::clientIP,
                NetworkStackHandlerTests::osIP,
                src_port, dst_port, data.c_str(), data.size(), maxFragmentSize, identification);

            packets.insert(packets.end(),
                std::make_move_iterator(fragments.begin()),
                std::make_move_iterator(fragments.end())
            );
        }

        std::shuffle(packets.begin(), packets.end(), std::default_random_engine(std::chrono::system_clock::now().time_since_epoch().count()));

        for(int j=0; j<packets.size(); j++){
            PacketType packetType = pNetworkStackHandler->handleIncomingEthernetPacket(packets[j].first.get(), packets[j].second);
            ASSERT_EQ(static_cast<int>(packetType), static_cast<int>(PacketType::IPv4Packet));
        }

        buffersFullness = getBuffersFullness();
        ASSERT_EQ(buffersFullness, std::make_tuple(NUM_PACKET_BUFFERS-packets.size(), 0, numDifferentPackets));

        std::vector<bool> identifiedPacketFound(numDifferentPackets, false);
        unsigned int numPoppedFragments = 0;
        for(unsigned int i=0; i<numDifferentPackets; i++){
            IPv4Packet* pIPv4Packet = pNetworkStackHandler->getLatestIPv4Packet();

            unsigned int identification = pIPv4Packet->identification;
            identifiedPacketFound[identification-1] = true;

            unsigned int numIPv4Fragments = 0;
            IPv4Packet* pCurrentIPv4Packet = pIPv4Packet;
            while(pCurrentIPv4Packet->nextFragment!=nullptr){
                ASSERT_EQ(pCurrentIPv4Packet->fragmentOffset, MAX_IPV4_FRAGMENT_SIZE*numIPv4Fragments);
                ASSERT_EQ(pCurrentIPv4Packet->dataSize, MAX_IPV4_FRAGMENT_SIZE);
                
                unsigned int firstFragmentCompensation = numIPv4Fragments==0 ? UDP_HDRLEN : 0;
                ASSERT_EQ(memcmp(pCurrentIPv4Packet->pData->data + firstFragmentCompensation, 
                    datas[identification-1].c_str() + MAX_IPV4_FRAGMENT_SIZE*numIPv4Fragments - (UDP_HDRLEN-firstFragmentCompensation), 
                    pCurrentIPv4Packet->dataSize - firstFragmentCompensation), 0);
                
                numIPv4Fragments++;
                pCurrentIPv4Packet = pCurrentIPv4Packet->nextFragment;
            }

            ASSERT_EQ(pCurrentIPv4Packet->fragmentOffset, MAX_IPV4_FRAGMENT_SIZE*numIPv4Fragments);
            ASSERT_EQ(pCurrentIPv4Packet->dataSize, 
                datas[identification-1].size() - (MAX_IPV4_FRAGMENT_SIZE*numIPv4Fragments-UDP_HDRLEN));
            
            unsigned int firstFragmentCompensation = numIPv4Fragments==0 ? UDP_HDRLEN : 0;
            ASSERT_EQ(memcmp(pCurrentIPv4Packet->pData->data + firstFragmentCompensation, 
                    datas[identification-1].c_str() + MAX_IPV4_FRAGMENT_SIZE*numIPv4Fragments - (UDP_HDRLEN-firstFragmentCompensation), 
                    pCurrentIPv4Packet->dataSize - firstFragmentCompensation), 0);
            
            numIPv4Fragments++;
            ASSERT_EQ(numIPv4Fragments, (datas[identification-1].size()+UDP_HDRLEN+MAX_IPV4_FRAGMENT_SIZE-1)/MAX_IPV4_FRAGMENT_SIZE);

            pNetworkStackHandler->popLatestIPv4Packet();
            numPoppedFragments += numIPv4Fragments;

            buffersFullness = getBuffersFullness();
            ASSERT_EQ(buffersFullness, std::make_tuple(NUM_PACKET_BUFFERS-packets.size()+numPoppedFragments, 0, numDifferentPackets-(i+1)));
        }

        for(unsigned int i=0; i<numDifferentPackets; i++){
            ASSERT_EQ(identifiedPacketFound[i], true);
        }
    }

    std::tuple<unsigned int, unsigned int, unsigned int> buffersFullness = getBuffersFullness();
    ASSERT_EQ(buffersFullness, std::make_tuple(NUM_PACKET_BUFFERS, 0, 0));
}

TEST_F(NetworkStackHandlerTests, PacketsWithDifferentPacketSizes_ShouldBeGettable){
    int src_port = 54321;
    int dst_port = 6000;
    unsigned int maxFragmentSize = MAX_IPV4_FRAGMENT_SIZE;
    unsigned short identification = 1;

    std::vector<unsigned int> packetSizes = {0,
        1, 
        MAX_IPV4_FRAGMENT_SIZE-UDP_HDRLEN, 
        MAX_IPV4_FRAGMENT_SIZE-UDP_HDRLEN+1,
        2*MAX_IPV4_FRAGMENT_SIZE-UDP_HDRLEN-1,
        2*MAX_IPV4_FRAGMENT_SIZE-UDP_HDRLEN,
        2*MAX_IPV4_FRAGMENT_SIZE-UDP_HDRLEN+1};
    
    for(unsigned int packetSize : packetSizes){
        std::tuple<unsigned int, unsigned int, unsigned int> buffersFullness = getBuffersFullness();
        ASSERT_EQ(buffersFullness, std::make_tuple(NUM_PACKET_BUFFERS, 0, 0));

        std::string data = generateRandomString(packetSize);

        std::vector<std::pair<std::unique_ptr<unsigned char[]>, unsigned int>> packets = convertUDPToEthernetPackets(NetworkStackHandlerTests::clientMac, 
            NetworkStackHandlerTests::osMac,
            NetworkStackHandlerTests::clientIP,
            NetworkStackHandlerTests::osIP,
            src_port, dst_port, data.c_str(), data.size(), maxFragmentSize, identification);

        ASSERT_EQ(packets.size(), (data.size()+UDP_HDRLEN+MAX_IPV4_FRAGMENT_SIZE-1)/MAX_IPV4_FRAGMENT_SIZE);

        for(int j=0; j<packets.size(); j++){
            PacketType packetType = pNetworkStackHandler->handleIncomingEthernetPacket(packets[j].first.get(), packets[j].second);
            ASSERT_EQ(static_cast<int>(packetType), static_cast<int>(PacketType::IPv4Packet));

            std::tuple<unsigned int, unsigned int, unsigned int> buffersFullness = getBuffersFullness();

            if(j==packets.size()-1){
                ASSERT_EQ(buffersFullness, std::make_tuple(NUM_PACKET_BUFFERS-1-j, 0, 1));
            }
            else{
                ASSERT_EQ(buffersFullness, std::make_tuple(NUM_PACKET_BUFFERS-1-j, 1, 0));
            }
        }

        IPv4Packet* pIPv4Packet = pNetworkStackHandler->getLatestIPv4Packet();
        unsigned int numIPv4Fragments = 0;
        IPv4Packet* pCurrentIPv4Packet = pIPv4Packet;
        while(pCurrentIPv4Packet->nextFragment!=nullptr){
            ASSERT_EQ(pCurrentIPv4Packet->fragmentOffset, MAX_IPV4_FRAGMENT_SIZE*numIPv4Fragments);
            ASSERT_EQ(pCurrentIPv4Packet->dataSize, MAX_IPV4_FRAGMENT_SIZE);
            
            unsigned int firstFragmentCompensation = numIPv4Fragments==0 ? UDP_HDRLEN : 0;
            ASSERT_EQ(memcmp(pCurrentIPv4Packet->pData->data + firstFragmentCompensation, 
                data.c_str() + MAX_IPV4_FRAGMENT_SIZE*numIPv4Fragments - (UDP_HDRLEN-firstFragmentCompensation), 
                pCurrentIPv4Packet->dataSize - firstFragmentCompensation), 0);
            
            numIPv4Fragments++;
            pCurrentIPv4Packet = pCurrentIPv4Packet->nextFragment;
        }

        ASSERT_EQ(pCurrentIPv4Packet->fragmentOffset, MAX_IPV4_FRAGMENT_SIZE*numIPv4Fragments);
        ASSERT_EQ(pCurrentIPv4Packet->dataSize, 
            data.size() - (MAX_IPV4_FRAGMENT_SIZE*numIPv4Fragments-UDP_HDRLEN));
        
        unsigned int firstFragmentCompensation = numIPv4Fragments==0 ? UDP_HDRLEN : 0;
        ASSERT_EQ(memcmp(pCurrentIPv4Packet->pData->data + firstFragmentCompensation, 
                data.c_str() + MAX_IPV4_FRAGMENT_SIZE*numIPv4Fragments - (UDP_HDRLEN-firstFragmentCompensation), 
                pCurrentIPv4Packet->dataSize - firstFragmentCompensation), 0);
        
        numIPv4Fragments++;
        ASSERT_EQ(numIPv4Fragments, packets.size());

        pNetworkStackHandler->popLatestIPv4Packet();
    }

    std::tuple<unsigned int, unsigned int, unsigned int> buffersFullness = getBuffersFullness();
    ASSERT_EQ(buffersFullness, std::make_tuple(NUM_PACKET_BUFFERS, 0, 0));
}

TEST_F(NetworkStackHandlerTests, ArrivingIPv4WhenUnusedPacketsBufferIsEmpty_ShouldRemovePacketsFromIncompleteFragmentedDatagrams){
    int src_port = 54321;
    int dst_port = 6000;
    unsigned int maxFragmentSize = MAX_IPV4_FRAGMENT_SIZE;
    
    unsigned short firstFragmentedUncompletePacketId = 1;
    unsigned int firstFragmentedUncompletePacketSize = 4*MAX_IPV4_FRAGMENT_SIZE-UDP_HDRLEN+(MAX_IPV4_FRAGMENT_SIZE/2);
    std::string firstFragmentedUncompletePacketData = generateRandomString(firstFragmentedUncompletePacketSize);
    std::vector<std::pair<std::unique_ptr<unsigned char[]>, unsigned int>> firstFragmentedUncompletePacketFragments = convertUDPToEthernetPackets(NetworkStackHandlerTests::clientMac, 
            NetworkStackHandlerTests::osMac,
            NetworkStackHandlerTests::clientIP,
            NetworkStackHandlerTests::osIP,
            src_port, dst_port, firstFragmentedUncompletePacketData.c_str(), firstFragmentedUncompletePacketData.size(), maxFragmentSize, firstFragmentedUncompletePacketId);
    for(int j=0; j<firstFragmentedUncompletePacketFragments.size()-1; j++){
        PacketType packetType = pNetworkStackHandler->handleIncomingEthernetPacket(firstFragmentedUncompletePacketFragments[j].first.get(), firstFragmentedUncompletePacketFragments[j].second);
        ASSERT_EQ(static_cast<int>(packetType), static_cast<int>(PacketType::IPv4Packet));
    }
    std::tuple<unsigned int, unsigned int, unsigned int> buffersFullness = getBuffersFullness();
    ASSERT_EQ(buffersFullness, std::make_tuple(NUM_PACKET_BUFFERS-(firstFragmentedUncompletePacketFragments.size()-1), 1, 0));

    unsigned short secondFragmentedUncompletePacketId = 2;
    unsigned int secondFragmentedUncompletePacketSize = MAX_IPV4_FRAGMENT_SIZE-UDP_HDRLEN+(MAX_IPV4_FRAGMENT_SIZE/2);
    std::string secondFragmentedUncompletePacketData = generateRandomString(secondFragmentedUncompletePacketSize);
    std::vector<std::pair<std::unique_ptr<unsigned char[]>, unsigned int>> secondFragmentedUncompletePacketFragments = convertUDPToEthernetPackets(NetworkStackHandlerTests::clientMac, 
            NetworkStackHandlerTests::osMac,
            NetworkStackHandlerTests::clientIP,
            NetworkStackHandlerTests::osIP,
            src_port, dst_port, secondFragmentedUncompletePacketData.c_str(), secondFragmentedUncompletePacketData.size(), maxFragmentSize, secondFragmentedUncompletePacketId);
    for(int j=0; j<secondFragmentedUncompletePacketFragments.size()-1; j++){
        PacketType packetType = pNetworkStackHandler->handleIncomingEthernetPacket(secondFragmentedUncompletePacketFragments[j].first.get(), secondFragmentedUncompletePacketFragments[j].second);
        ASSERT_EQ(static_cast<int>(packetType), static_cast<int>(PacketType::IPv4Packet));
    }
    buffersFullness = getBuffersFullness();
    ASSERT_EQ(buffersFullness, std::make_tuple(NUM_PACKET_BUFFERS-(firstFragmentedUncompletePacketFragments.size()-1)-(secondFragmentedUncompletePacketFragments.size()-1), 2, 0));

    unsigned int remainingBufferSpace = NUM_PACKET_BUFFERS-(firstFragmentedUncompletePacketFragments.size()-1)-(secondFragmentedUncompletePacketFragments.size()-1);
    for(int i=0; i<remainingBufferSpace; i++){
        std::string data = "Hello World!";
        std::vector<std::pair<std::unique_ptr<unsigned char[]>, unsigned int>> packets = convertUDPToEthernetPackets(NetworkStackHandlerTests::clientMac, 
            NetworkStackHandlerTests::osMac,
            NetworkStackHandlerTests::clientIP,
            NetworkStackHandlerTests::osIP,
            src_port, dst_port, data.c_str(), data.size(), maxFragmentSize, 1);
        
        PacketType packetType = pNetworkStackHandler->handleIncomingEthernetPacket(packets[0].first.get(), packets[0].second);
        ASSERT_EQ(static_cast<int>(packetType), static_cast<int>(PacketType::IPv4Packet));
    }
    buffersFullness = getBuffersFullness();
    ASSERT_EQ(buffersFullness, std::make_tuple(0, 2, remainingBufferSpace));

    std::string newPacketData = "Packet arrives while no free buffer space??";
    std::vector<std::pair<std::unique_ptr<unsigned char[]>, unsigned int>> newPacket = convertUDPToEthernetPackets(NetworkStackHandlerTests::clientMac, 
        NetworkStackHandlerTests::osMac,
        NetworkStackHandlerTests::clientIP,
        NetworkStackHandlerTests::osIP,
        src_port, dst_port, newPacketData.c_str(), newPacketData.size(), maxFragmentSize, 1);
    PacketType packetType = pNetworkStackHandler->handleIncomingEthernetPacket(newPacket[0].first.get(), newPacket[0].second);
    ASSERT_EQ(static_cast<int>(packetType), static_cast<int>(PacketType::IPv4Packet));

    buffersFullness = getBuffersFullness();
    ASSERT_EQ(buffersFullness, std::make_tuple(firstFragmentedUncompletePacketFragments.size()-2, 1, remainingBufferSpace+1));

    for(int i=0; i<remainingBufferSpace; i++){
        IPv4Packet* pIPv4Packet = pNetworkStackHandler->getLatestIPv4Packet();
        ASSERT_EQ(pIPv4Packet->dataSize, strlen("Hello World!")+UDP_HDRLEN);
        ASSERT_EQ(memcmp(pIPv4Packet->pData->data + UDP_HDRLEN, "Hello World!", 12), 0);
        pNetworkStackHandler->popLatestIPv4Packet();
    }
    buffersFullness = getBuffersFullness();
    ASSERT_EQ(buffersFullness, std::make_tuple(NUM_PACKET_BUFFERS-(secondFragmentedUncompletePacketFragments.size()-1)-1, 1, 1));

    IPv4Packet* pIPv4Packet = pNetworkStackHandler->getLatestIPv4Packet();
    ASSERT_EQ(pIPv4Packet->dataSize, newPacketData.size()+UDP_HDRLEN);
    ASSERT_EQ(memcmp(pIPv4Packet->pData->data + UDP_HDRLEN, newPacketData.c_str(), newPacketData.size()), 0);
    pNetworkStackHandler->popLatestIPv4Packet();
    buffersFullness = getBuffersFullness();
    ASSERT_EQ(buffersFullness, std::make_tuple(NUM_PACKET_BUFFERS-(secondFragmentedUncompletePacketFragments.size()-1), 1, 0));

    packetType = pNetworkStackHandler->handleIncomingEthernetPacket(secondFragmentedUncompletePacketFragments[secondFragmentedUncompletePacketFragments.size()-1].first.get(), secondFragmentedUncompletePacketFragments[secondFragmentedUncompletePacketFragments.size()-1].second);
    ASSERT_EQ(static_cast<int>(packetType), static_cast<int>(PacketType::IPv4Packet));
    buffersFullness = getBuffersFullness();
    ASSERT_EQ(buffersFullness, std::make_tuple(NUM_PACKET_BUFFERS-secondFragmentedUncompletePacketFragments.size(), 0, 1));

    pIPv4Packet = pNetworkStackHandler->getLatestIPv4Packet();
    unsigned int numIPv4Fragments = 0;
    IPv4Packet* pCurrentIPv4Packet = pIPv4Packet;
    while(pCurrentIPv4Packet->nextFragment!=nullptr){
        ASSERT_EQ(pCurrentIPv4Packet->fragmentOffset, MAX_IPV4_FRAGMENT_SIZE*numIPv4Fragments);
        ASSERT_EQ(pCurrentIPv4Packet->dataSize, MAX_IPV4_FRAGMENT_SIZE);
        
        unsigned int firstFragmentCompensation = numIPv4Fragments==0 ? UDP_HDRLEN : 0;
        ASSERT_EQ(memcmp(pCurrentIPv4Packet->pData->data + firstFragmentCompensation, 
            secondFragmentedUncompletePacketData.c_str() + MAX_IPV4_FRAGMENT_SIZE*numIPv4Fragments - (UDP_HDRLEN-firstFragmentCompensation), 
            pCurrentIPv4Packet->dataSize - firstFragmentCompensation), 0);
        
        numIPv4Fragments++;
        pCurrentIPv4Packet = pCurrentIPv4Packet->nextFragment;
    }

    ASSERT_EQ(pCurrentIPv4Packet->fragmentOffset, MAX_IPV4_FRAGMENT_SIZE*numIPv4Fragments);
    ASSERT_EQ(pCurrentIPv4Packet->dataSize, 
        secondFragmentedUncompletePacketData.size() - (MAX_IPV4_FRAGMENT_SIZE*numIPv4Fragments-UDP_HDRLEN));
    
    unsigned int firstFragmentCompensation = numIPv4Fragments==0 ? UDP_HDRLEN : 0;
    ASSERT_EQ(memcmp(pCurrentIPv4Packet->pData->data + firstFragmentCompensation, 
            secondFragmentedUncompletePacketData.c_str() + MAX_IPV4_FRAGMENT_SIZE*numIPv4Fragments - (UDP_HDRLEN-firstFragmentCompensation), 
            pCurrentIPv4Packet->dataSize - firstFragmentCompensation), 0);
    
    numIPv4Fragments++;
    ASSERT_EQ(numIPv4Fragments, secondFragmentedUncompletePacketFragments.size());

    pNetworkStackHandler->popLatestIPv4Packet();

    buffersFullness = getBuffersFullness();
    ASSERT_EQ(buffersFullness, std::make_tuple(NUM_PACKET_BUFFERS, 0, 0));
}

TEST_F(NetworkStackHandlerTests, ArrivingIPv4WhenUnusedPacketsBufferIsEmpty_ShouldBeIgnoredSinceReadyToReadBufferIsFull){
    int src_port = 54321;
    int dst_port = 6000;
    unsigned int maxFragmentSize = MAX_IPV4_FRAGMENT_SIZE;
    
    for(int i=0; i<NUM_PACKET_BUFFERS; i++){
        std::string data = "Hello World!";
        std::vector<std::pair<std::unique_ptr<unsigned char[]>, unsigned int>> packets = convertUDPToEthernetPackets(NetworkStackHandlerTests::clientMac, 
            NetworkStackHandlerTests::osMac,
            NetworkStackHandlerTests::clientIP,
            NetworkStackHandlerTests::osIP,
            src_port, dst_port, data.c_str(), data.size(), maxFragmentSize, 1);
        
        PacketType packetType = pNetworkStackHandler->handleIncomingEthernetPacket(packets[0].first.get(), packets[0].second);
        ASSERT_EQ(static_cast<int>(packetType), static_cast<int>(PacketType::IPv4Packet));
    }
    std::tuple<unsigned int, unsigned int, unsigned int> buffersFullness = getBuffersFullness();
    ASSERT_EQ(buffersFullness, std::make_tuple(0, 0, NUM_PACKET_BUFFERS));

    std::string newPacketData = "Packet arrives while no free buffer space??";
    std::vector<std::pair<std::unique_ptr<unsigned char[]>, unsigned int>> newPacket = convertUDPToEthernetPackets(NetworkStackHandlerTests::clientMac, 
        NetworkStackHandlerTests::osMac,
        NetworkStackHandlerTests::clientIP,
        NetworkStackHandlerTests::osIP,
        src_port, dst_port, newPacketData.c_str(), newPacketData.size(), maxFragmentSize, 1);
    PacketType packetType = pNetworkStackHandler->handleIncomingEthernetPacket(newPacket[0].first.get(), newPacket[0].second);
    ASSERT_EQ(static_cast<int>(packetType), static_cast<int>(PacketType::IPv4Packet));

    buffersFullness = getBuffersFullness();
    ASSERT_EQ(buffersFullness, std::make_tuple(0, 0, NUM_PACKET_BUFFERS));

    for(int i=0; i<NUM_PACKET_BUFFERS; i++){
        IPv4Packet* pIPv4Packet = pNetworkStackHandler->getLatestIPv4Packet();
        ASSERT_EQ(pIPv4Packet->dataSize, strlen("Hello World!")+UDP_HDRLEN);
        ASSERT_EQ(memcmp(pIPv4Packet->pData->data + UDP_HDRLEN, "Hello World!", 12), 0);
        pNetworkStackHandler->popLatestIPv4Packet();
    }
    buffersFullness = getBuffersFullness();
    ASSERT_EQ(buffersFullness, std::make_tuple(NUM_PACKET_BUFFERS, 0, 0));
}

TEST_F(NetworkStackHandlerTests, FragmentedPacketTimesOut_ShouldBeRemovedFromIncompleteFragmentedDatagramsList){
    int src_port = 54321;
    int dst_port = 6000;
    unsigned int maxFragmentSize = MAX_IPV4_FRAGMENT_SIZE;

    unsigned short incompletePacket1Id = 1;
    unsigned int incompletePacket1Size = 2*MAX_IPV4_FRAGMENT_SIZE-UDP_HDRLEN;
    std::string incompletePacket1Data = generateRandomString(incompletePacket1Size);
    std::vector<std::pair<std::unique_ptr<unsigned char[]>, unsigned int>> incompletePacket1Fragments = convertUDPToEthernetPackets(NetworkStackHandlerTests::clientMac, 
        NetworkStackHandlerTests::osMac,
        NetworkStackHandlerTests::clientIP,
        NetworkStackHandlerTests::osIP,
        src_port, dst_port, incompletePacket1Data.c_str(), incompletePacket1Data.size(), maxFragmentSize, incompletePacket1Id);
    PacketType packetType = pNetworkStackHandler->handleIncomingEthernetPacket(incompletePacket1Fragments[0].first.get(), incompletePacket1Fragments[0].second);
    std::tuple<unsigned int, unsigned int, unsigned int> buffersFullness = getBuffersFullness();
    ASSERT_EQ(buffersFullness, std::make_tuple(NUM_PACKET_BUFFERS-1, 1, 0));

    unsigned short incompletePacket2Id = 2;
    unsigned int incompletePacket2Size = (FRAGMENT_TIMEOUT+1)*MAX_IPV4_FRAGMENT_SIZE-UDP_HDRLEN;
    std::string incompletePacket2Data = generateRandomString(incompletePacket2Size);
    std::vector<std::pair<std::unique_ptr<unsigned char[]>, unsigned int>> incompletePacket2Fragments = convertUDPToEthernetPackets(NetworkStackHandlerTests::clientMac, 
        NetworkStackHandlerTests::osMac,
        NetworkStackHandlerTests::clientIP,
        NetworkStackHandlerTests::osIP,
        src_port, dst_port, incompletePacket2Data.c_str(), incompletePacket2Data.size(), maxFragmentSize, incompletePacket2Id);
    for(int j=0; j<incompletePacket2Fragments.size()-1; j++){
        unsigned int index = incompletePacket2Fragments.size()-1-j;
        packetType = pNetworkStackHandler->handleIncomingEthernetPacket(incompletePacket2Fragments[index].first.get(), incompletePacket2Fragments[index].second);
        ASSERT_EQ(static_cast<int>(packetType), static_cast<int>(PacketType::IPv4Packet));

        pNetworkStackHandler->incrementTimerCounter();
    }
    buffersFullness = getBuffersFullness();
    ASSERT_EQ(buffersFullness, std::make_tuple(NUM_PACKET_BUFFERS-1-FRAGMENT_TIMEOUT, 2, 0));

    unsigned short incompletePacket3Id = 3;
    unsigned int incompletePacket3Size = 2*MAX_IPV4_FRAGMENT_SIZE-UDP_HDRLEN;
    std::string incompletePacket3Data = generateRandomString(incompletePacket3Size);
    std::vector<std::pair<std::unique_ptr<unsigned char[]>, unsigned int>> incompletePacket3Fragments = convertUDPToEthernetPackets(NetworkStackHandlerTests::clientMac, 
        NetworkStackHandlerTests::osMac,
        NetworkStackHandlerTests::clientIP,
        NetworkStackHandlerTests::osIP,
        src_port, dst_port, incompletePacket3Data.c_str(), incompletePacket3Data.size(), maxFragmentSize, incompletePacket3Id);
    packetType = pNetworkStackHandler->handleIncomingEthernetPacket(incompletePacket3Fragments[0].first.get(), incompletePacket3Fragments[0].second);
    buffersFullness = getBuffersFullness();
    ASSERT_EQ(buffersFullness, std::make_tuple(NUM_PACKET_BUFFERS-2-FRAGMENT_TIMEOUT, 3, 0));

    unsigned short incompletePacket4Id = 4;
    unsigned int incompletePacket4Size = 2*MAX_IPV4_FRAGMENT_SIZE-UDP_HDRLEN;
    std::string incompletePacket4Data = generateRandomString(incompletePacket4Size);
    std::vector<std::pair<std::unique_ptr<unsigned char[]>, unsigned int>> incompletePacket4Fragments = convertUDPToEthernetPackets(NetworkStackHandlerTests::clientMac, 
        NetworkStackHandlerTests::osMac,
        NetworkStackHandlerTests::clientIP,
        NetworkStackHandlerTests::osIP,
        src_port, dst_port, incompletePacket4Data.c_str(), incompletePacket4Data.size(), maxFragmentSize, incompletePacket4Id);
    packetType = pNetworkStackHandler->handleIncomingEthernetPacket(incompletePacket4Fragments[0].first.get(), incompletePacket4Fragments[0].second);
    buffersFullness = getBuffersFullness();
    ASSERT_EQ(buffersFullness, std::make_tuple(NUM_PACKET_BUFFERS-3-FRAGMENT_TIMEOUT, 4, 0));
    
    // Packet is timed out but will only be removed next time incompletePacketBuffer is accessed
    pNetworkStackHandler->incrementTimerCounter();
    buffersFullness = getBuffersFullness();
    ASSERT_EQ(buffersFullness, std::make_tuple(NUM_PACKET_BUFFERS-3-FRAGMENT_TIMEOUT, 4, 0));

    packetType = pNetworkStackHandler->handleIncomingEthernetPacket(incompletePacket3Fragments[1].first.get(), incompletePacket3Fragments[1].second);
    ASSERT_EQ(static_cast<int>(packetType), static_cast<int>(PacketType::IPv4Packet));
    IPv4Packet* pIPv4Packet = pNetworkStackHandler->getLatestIPv4Packet();
    ASSERT_EQ(pIPv4Packet->nextFragment->nextFragment, nullptr);
    ASSERT_EQ(pIPv4Packet->dataSize+pIPv4Packet->nextFragment->dataSize, incompletePacket3Data.size()+UDP_HDRLEN);
    ASSERT_EQ(memcmp(pIPv4Packet->pData->data + UDP_HDRLEN, incompletePacket3Data.c_str(), pIPv4Packet->dataSize-UDP_HDRLEN), 0);
    ASSERT_EQ(memcmp(pIPv4Packet->nextFragment->pData->data, incompletePacket3Data.c_str()+pIPv4Packet->dataSize-UDP_HDRLEN, pIPv4Packet->nextFragment->dataSize), 0);
    pNetworkStackHandler->popLatestIPv4Packet();

    buffersFullness = getBuffersFullness();
    ASSERT_EQ(buffersFullness, std::make_tuple(NUM_PACKET_BUFFERS-1, 1, 0));

    packetType = pNetworkStackHandler->handleIncomingEthernetPacket(incompletePacket4Fragments[1].first.get(), incompletePacket4Fragments[1].second);
    ASSERT_EQ(static_cast<int>(packetType), static_cast<int>(PacketType::IPv4Packet));
    pIPv4Packet = pNetworkStackHandler->getLatestIPv4Packet();
    ASSERT_EQ(pIPv4Packet->nextFragment->nextFragment, nullptr);
    ASSERT_EQ(pIPv4Packet->dataSize+pIPv4Packet->nextFragment->dataSize, incompletePacket4Data.size()+UDP_HDRLEN);
    ASSERT_EQ(memcmp(pIPv4Packet->pData->data + UDP_HDRLEN, incompletePacket4Data.c_str(), pIPv4Packet->dataSize-UDP_HDRLEN), 0);
    ASSERT_EQ(memcmp(pIPv4Packet->nextFragment->pData->data, incompletePacket4Data.c_str()+pIPv4Packet->dataSize-UDP_HDRLEN, pIPv4Packet->nextFragment->dataSize), 0);
    pNetworkStackHandler->popLatestIPv4Packet();

    buffersFullness = getBuffersFullness();
    ASSERT_EQ(buffersFullness, std::make_tuple(NUM_PACKET_BUFFERS, 0, 0));
}

TEST_F(NetworkStackHandlerTests, FragmentedPacketTimesOut1_ShouldBeRemovedFromIncompleteFragmentedDatagramsList){
    int src_port = 54321;
    int dst_port = 6000;
    unsigned int maxFragmentSize = MAX_IPV4_FRAGMENT_SIZE;

    unsigned short incompletePacketId = 1;
    unsigned int incompletePacketSize = 2*MAX_IPV4_FRAGMENT_SIZE-UDP_HDRLEN;
    std::string incompletePacketData = generateRandomString(incompletePacketSize);
    std::vector<std::pair<std::unique_ptr<unsigned char[]>, unsigned int>> incompletePacketFragments = convertUDPToEthernetPackets(NetworkStackHandlerTests::clientMac, 
        NetworkStackHandlerTests::osMac,
        NetworkStackHandlerTests::clientIP,
        NetworkStackHandlerTests::osIP,
        src_port, dst_port, incompletePacketData.c_str(), incompletePacketData.size(), maxFragmentSize, incompletePacketId);
    PacketType packetType = pNetworkStackHandler->handleIncomingEthernetPacket(incompletePacketFragments[0].first.get(), incompletePacketFragments[0].second);
    std::tuple<unsigned int, unsigned int, unsigned int> buffersFullness = getBuffersFullness();
    ASSERT_EQ(buffersFullness, std::make_tuple(NUM_PACKET_BUFFERS-1, 1, 0));

    for(int j=0; j<FRAGMENT_TIMEOUT; j++){
        pNetworkStackHandler->incrementTimerCounter();
    }

    // Timeout only occurs when incompletePacketList is updated
    unsigned short incompletePacket1Id = 2;
    unsigned int incompletePacket1Size = 2*MAX_IPV4_FRAGMENT_SIZE-UDP_HDRLEN;
    std::string incompletePacket1Data = generateRandomString(incompletePacket1Size);
    std::vector<std::pair<std::unique_ptr<unsigned char[]>, unsigned int>> incompletePacket1Fragments = convertUDPToEthernetPackets(NetworkStackHandlerTests::clientMac, 
        NetworkStackHandlerTests::osMac,
        NetworkStackHandlerTests::clientIP,
        NetworkStackHandlerTests::osIP,
        src_port, dst_port, incompletePacket1Data.c_str(), incompletePacket1Data.size(), maxFragmentSize, incompletePacket1Id);
    packetType = pNetworkStackHandler->handleIncomingEthernetPacket(incompletePacket1Fragments[0].first.get(), incompletePacket1Fragments[0].second);
    buffersFullness = getBuffersFullness();
    ASSERT_EQ(buffersFullness, std::make_tuple(NUM_PACKET_BUFFERS-2, 2, 0));

    pNetworkStackHandler->incrementTimerCounter();
    packetType = pNetworkStackHandler->handleIncomingEthernetPacket(incompletePacket1Fragments[1].first.get(), incompletePacket1Fragments[1].second);
    buffersFullness = getBuffersFullness();
    ASSERT_EQ(buffersFullness, std::make_tuple(NUM_PACKET_BUFFERS-2, 0, 1));

    pNetworkStackHandler->popLatestIPv4Packet();
    buffersFullness = getBuffersFullness();
    ASSERT_EQ(buffersFullness, std::make_tuple(NUM_PACKET_BUFFERS, 0, 0));
}

TEST_F(NetworkStackHandlerTests, IncorrectExtraFragmentPreventsCorrectProcessing_ShouldBeResolvedByTimeout){
    int src_port = 54321;
    int dst_port = 6000;

    unsigned int maxFragmentSize1 = 1280;
    unsigned short incompletePacketId = 1;
    unsigned int incompletePacketSize = 8*maxFragmentSize1-UDP_HDRLEN;
    std::string incompletePacketData = generateRandomString(incompletePacketSize);
    std::vector<std::pair<std::unique_ptr<unsigned char[]>, unsigned int>> incompletePacketFragments = convertUDPToEthernetPackets(NetworkStackHandlerTests::clientMac, 
        NetworkStackHandlerTests::osMac,
        NetworkStackHandlerTests::clientIP,
        NetworkStackHandlerTests::osIP,
        src_port, dst_port, incompletePacketData.c_str(), incompletePacketData.size(), maxFragmentSize1, incompletePacketId);
    for(int j=0; j<4; j++){
        PacketType packetType = pNetworkStackHandler->handleIncomingEthernetPacket(incompletePacketFragments[j].first.get(), incompletePacketFragments[j].second);
        ASSERT_EQ(static_cast<int>(packetType), static_cast<int>(PacketType::IPv4Packet));
    }
    std::tuple<unsigned int, unsigned int, unsigned int> buffersFullness = getBuffersFullness();
    ASSERT_EQ(buffersFullness, std::make_tuple(NUM_PACKET_BUFFERS-4, 1, 0));

    unsigned int maxFragmentSize2 = 640;
    // Notice incompletePacketId==incompletePacket2Id
    unsigned short incompletePacket2Id = 1;
    unsigned int incompletePacket2Size = 16*maxFragmentSize2-UDP_HDRLEN;
    std::string incompletePacket2Data = generateRandomString(incompletePacket2Size);
    std::vector<std::pair<std::unique_ptr<unsigned char[]>, unsigned int>> incompletePacket2Fragments = convertUDPToEthernetPackets(NetworkStackHandlerTests::clientMac, 
        NetworkStackHandlerTests::osMac,
        NetworkStackHandlerTests::clientIP,
        NetworkStackHandlerTests::osIP,
        src_port, dst_port, incompletePacket2Data.c_str(), incompletePacket2Data.size(), maxFragmentSize2, incompletePacket2Id);
    for(int j=0; j<9; j++){
        PacketType packetType = pNetworkStackHandler->handleIncomingEthernetPacket(incompletePacket2Fragments[j].first.get(), incompletePacket2Fragments[j].second);
        ASSERT_EQ(static_cast<int>(packetType), static_cast<int>(PacketType::IPv4Packet));
    }
    buffersFullness = getBuffersFullness();
    ASSERT_EQ(buffersFullness, std::make_tuple(NUM_PACKET_BUFFERS-13, 1, 0));

    for(int j=4; j<incompletePacketFragments.size(); j++){
        PacketType packetType = pNetworkStackHandler->handleIncomingEthernetPacket(incompletePacketFragments[j].first.get(), incompletePacketFragments[j].second);
        ASSERT_EQ(static_cast<int>(packetType), static_cast<int>(PacketType::IPv4Packet));
    }
    buffersFullness = getBuffersFullness();
    ASSERT_EQ(buffersFullness, std::make_tuple(NUM_PACKET_BUFFERS-17, 1, 0));

    for(int j=0; j<=FRAGMENT_TIMEOUT; j++){
        pNetworkStackHandler->incrementTimerCounter();
    }

    // Timeout only occurs when incompletePacketList is updated
    unsigned int maxFragmentSize = MAX_IPV4_FRAGMENT_SIZE;
    unsigned short incompletePacket1Id = 2;
    unsigned int incompletePacket1Size = 2*MAX_IPV4_FRAGMENT_SIZE-UDP_HDRLEN;
    std::string incompletePacket1Data = generateRandomString(incompletePacket1Size);
    std::vector<std::pair<std::unique_ptr<unsigned char[]>, unsigned int>> incompletePacket1Fragments = convertUDPToEthernetPackets(NetworkStackHandlerTests::clientMac, 
        NetworkStackHandlerTests::osMac,
        NetworkStackHandlerTests::clientIP,
        NetworkStackHandlerTests::osIP,
        src_port, dst_port, incompletePacket1Data.c_str(), incompletePacket1Data.size(), maxFragmentSize, incompletePacket1Id);
    PacketType packetType = pNetworkStackHandler->handleIncomingEthernetPacket(incompletePacket1Fragments[0].first.get(), incompletePacket1Fragments[0].second);
    packetType = pNetworkStackHandler->handleIncomingEthernetPacket(incompletePacket1Fragments[1].first.get(), incompletePacket1Fragments[1].second);
    pNetworkStackHandler->popLatestIPv4Packet();
    buffersFullness = getBuffersFullness();
    ASSERT_EQ(buffersFullness, std::make_tuple(NUM_PACKET_BUFFERS, 0, 0));
}

TEST_F(NetworkStackHandlerTests, SendingPacketToOtherSubnet_ShouldGetGatewayMAC){
    std::string data = "Hello World!";
    unsigned char writeBuffer[ETHERNET_MTU+ETHERNET_SIMPLE_HEADER_SIZE];
    unsigned short identification;
    unsigned int fragmentOffset = 0;

    Pair<IPv4PacketProgress, unsigned int> state = pNetworkStackHandler->handleOutgoingIPv4Packet(NetworkStackHandlerTests::client2IP, 
        9000, 
        1000, 
        UDP_IPV4_PROTOCOL,
        (unsigned char*)data.c_str(), 
        data.size(), 
        identification, 
        fragmentOffset,
        writeBuffer);

    ARPEntry arpEntry = getARPEntry(NetworkStackHandlerTests::gatewayIP);
    ASSERT_TRUE(arpEntry.IP!=0);
    
    std::pair<std::unique_ptr<unsigned char[]>, unsigned int> arpReply = createARPReply(NetworkStackHandlerTests::gatewayMac, writeBuffer);
    
    PacketType packetType = pNetworkStackHandler->handleIncomingEthernetPacket(arpReply.first.get(), arpReply.second);
    ASSERT_EQ(packetType, PacketType::ARPReply);

    arpEntry = getARPEntry(NetworkStackHandlerTests::gatewayIP);
    for(int i=0; i<6; i++){
        ASSERT_EQ(arpEntry.MAC[i], NetworkStackHandlerTests::gatewayMac[i]);
    }
}

TEST_F(NetworkStackHandlerTests, SendingPacketToMySubnet_ShouldGetClientMAC){
    std::string data = "Hello World!";
    unsigned char writeBuffer[ETHERNET_MTU+ETHERNET_SIMPLE_HEADER_SIZE];
    unsigned short identification;
    unsigned int fragmentOffset = 0;

    Pair<IPv4PacketProgress, unsigned int> state = pNetworkStackHandler->handleOutgoingIPv4Packet(NetworkStackHandlerTests::clientIP, 
        9000, 
        1000, 
        UDP_IPV4_PROTOCOL,
        (unsigned char*)data.c_str(), 
        data.size(), 
        identification, 
        fragmentOffset,
        writeBuffer);

    ARPEntry arpEntry = getARPEntry(NetworkStackHandlerTests::clientIP);
    ASSERT_TRUE(arpEntry.IP!=0);
    
    std::pair<std::unique_ptr<unsigned char[]>, unsigned int> arpReply = createARPReply(NetworkStackHandlerTests::clientMac, writeBuffer);
    
    PacketType packetType = pNetworkStackHandler->handleIncomingEthernetPacket(arpReply.first.get(), arpReply.second);
    ASSERT_EQ(packetType, PacketType::ARPReply);

    arpEntry = getARPEntry(NetworkStackHandlerTests::clientIP);
    for(int i=0; i<6; i++){
        ASSERT_EQ(arpEntry.MAC[i], NetworkStackHandlerTests::clientMac[i]);
    }
}

TEST_F(NetworkStackHandlerTests, SendingARP_ARPTableFreeEntryShouldBeUsed){
    // First, make sure there is only one free entry available
    for(int i=1; i<ARP_HASH_ENTRY_LIST_SIZE; i++){
        std::string data = "Hello World!";
        unsigned char writeBuffer[ETHERNET_MTU+ETHERNET_SIMPLE_HEADER_SIZE];
        unsigned short identification;
        unsigned int fragmentOffset = 0;

        Pair<IPv4PacketProgress, unsigned int> state = pNetworkStackHandler->handleOutgoingIPv4Packet(NetworkStackHandlerTests::osIP + ARP_HASH_TABLE_SIZE*i, 
            9000, 
            1000, 
            UDP_IPV4_PROTOCOL,
            (unsigned char*)data.c_str(), 
            data.size(), 
            identification, 
            fragmentOffset,
            writeBuffer);
    }

    for(int i=1; i<ARP_HASH_ENTRY_LIST_SIZE; i++){
        ARPEntry arpEntry = getARPEntry(NetworkStackHandlerTests::osIP + ARP_HASH_TABLE_SIZE*i);
        ASSERT_TRUE(arpEntry.IP!=0);
    }

    std::string data = "Hello World!";
    unsigned char writeBuffer[ETHERNET_MTU+ETHERNET_SIMPLE_HEADER_SIZE];
    unsigned short identification;
    unsigned int fragmentOffset = 0;
    Pair<IPv4PacketProgress, unsigned int> state = pNetworkStackHandler->handleOutgoingIPv4Packet(NetworkStackHandlerTests::osIP + ARP_HASH_TABLE_SIZE*ARP_HASH_ENTRY_LIST_SIZE, 
        9000, 
        1000, 
        UDP_IPV4_PROTOCOL,
        (unsigned char*)data.c_str(), 
        data.size(), 
        identification, 
        fragmentOffset,
        writeBuffer);
    
    for(int i=1; i<=ARP_HASH_ENTRY_LIST_SIZE; i++){
        ARPEntry arpEntry = getARPEntry(NetworkStackHandlerTests::osIP + ARP_HASH_TABLE_SIZE*i);
        ASSERT_TRUE(arpEntry.IP!=0);
    }
}

TEST_F(NetworkStackHandlerTests, SendingARP_ARPTableTimedOutUnusedEntryShouldBeUsed){
    // First, make it so that no entry is free and make one entry older than the others
    std::string data = "Hello World!";
    unsigned char writeBuffer[ETHERNET_MTU+ETHERNET_SIMPLE_HEADER_SIZE];
    unsigned short identification;
    unsigned int fragmentOffset = 0;

    Pair<IPv4PacketProgress, unsigned int> state = pNetworkStackHandler->handleOutgoingIPv4Packet(NetworkStackHandlerTests::osIP + ARP_HASH_TABLE_SIZE, 
        9000, 
        1000, 
        UDP_IPV4_PROTOCOL,
        (unsigned char*)data.c_str(), 
        data.size(), 
        identification, 
        fragmentOffset,
        writeBuffer);

    pNetworkStackHandler->incrementTimerCounter();

    for(int i=2; i<=ARP_HASH_ENTRY_LIST_SIZE; i++){
        std::string data = "Hello World!";
        unsigned char writeBuffer[ETHERNET_MTU+ETHERNET_SIMPLE_HEADER_SIZE];
        unsigned short identification;
        unsigned int fragmentOffset = 0;

        Pair<IPv4PacketProgress, unsigned int> state = pNetworkStackHandler->handleOutgoingIPv4Packet(NetworkStackHandlerTests::osIP + ARP_HASH_TABLE_SIZE*i, 
            9000, 
            1000, 
            UDP_IPV4_PROTOCOL,
            (unsigned char*)data.c_str(), 
            data.size(), 
            identification, 
            fragmentOffset,
            writeBuffer);
        
        std::pair<std::unique_ptr<unsigned char[]>, unsigned int> arpReply = createARPReply(NetworkStackHandlerTests::clientMac, writeBuffer);
        PacketType packetType = pNetworkStackHandler->handleIncomingEthernetPacket(arpReply.first.get(), arpReply.second);
        ASSERT_EQ(packetType, PacketType::ARPReply);
    }

    for(int i=0; i<UNUSED_ARP_ENTRY_TIMEOUT; i++){
        pNetworkStackHandler->incrementTimerCounter();
    }

    data = "Hello World!";
    fragmentOffset = 0;

    state = pNetworkStackHandler->handleOutgoingIPv4Packet(NetworkStackHandlerTests::osIP + ARP_HASH_TABLE_SIZE*(ARP_HASH_ENTRY_LIST_SIZE+1), 
        9000, 
        1000, 
        UDP_IPV4_PROTOCOL,
        (unsigned char*)data.c_str(), 
        data.size(), 
        identification, 
        fragmentOffset,
        writeBuffer);

    for(int i=1; i<=(ARP_HASH_ENTRY_LIST_SIZE+1); i++){
        if(i==1){
            ARPEntry arpEntry = getARPEntry(NetworkStackHandlerTests::osIP + ARP_HASH_TABLE_SIZE*i);
            ASSERT_TRUE(arpEntry.IP==0);
        }
        else{
            ARPEntry arpEntry = getARPEntry(NetworkStackHandlerTests::osIP + ARP_HASH_TABLE_SIZE*i);
            ASSERT_TRUE(arpEntry.IP!=0);
        }
    }
}

TEST_F(NetworkStackHandlerTests, SendingARP_ARPTableLastTimedOutUnusedEntryShouldBeUsed){
    // First, make it so that no entry is free
    for(int i=1; i<=ARP_HASH_ENTRY_LIST_SIZE; i++){
        std::string data = "Hello World!";
        unsigned char writeBuffer[ETHERNET_MTU+ETHERNET_SIMPLE_HEADER_SIZE];
        unsigned short identification;
        unsigned int fragmentOffset = 0;

        Pair<IPv4PacketProgress, unsigned int> state = pNetworkStackHandler->handleOutgoingIPv4Packet(NetworkStackHandlerTests::osIP + ARP_HASH_TABLE_SIZE*i, 
            9000, 
            1000, 
            UDP_IPV4_PROTOCOL,
            (unsigned char*)data.c_str(), 
            data.size(), 
            identification, 
            fragmentOffset,
            writeBuffer);
    }

    for(int i=0; i<=UNUSED_ARP_ENTRY_TIMEOUT; i++){
        pNetworkStackHandler->incrementTimerCounter();
    }

    std::string data = "Hello World!";
    unsigned char writeBuffer[ETHERNET_MTU+ETHERNET_SIMPLE_HEADER_SIZE];
    unsigned short identification;
    unsigned int fragmentOffset = 0;

    Pair<IPv4PacketProgress, unsigned int> state = pNetworkStackHandler->handleOutgoingIPv4Packet(NetworkStackHandlerTests::osIP + ARP_HASH_TABLE_SIZE*(ARP_HASH_ENTRY_LIST_SIZE+1), 
        9000, 
        1000, 
        UDP_IPV4_PROTOCOL,
        (unsigned char*)data.c_str(), 
        data.size(), 
        identification, 
        fragmentOffset,
        writeBuffer);

    for(int i=1; i<=(ARP_HASH_ENTRY_LIST_SIZE+1); i++){
        if(i==ARP_HASH_ENTRY_LIST_SIZE){
            ARPEntry arpEntry = getARPEntry(NetworkStackHandlerTests::osIP + ARP_HASH_TABLE_SIZE*i);
            ASSERT_TRUE(arpEntry.IP==0);
        }
        else{
            ARPEntry arpEntry = getARPEntry(NetworkStackHandlerTests::osIP + ARP_HASH_TABLE_SIZE*i);
            ASSERT_TRUE(arpEntry.IP!=0);
        }
    }
}

TEST_F(NetworkStackHandlerTests, SendingARP_ARPTableOldestUsedEntryShouldBeSwapped){
    unsigned char randomMac[6];
    for(int i=0; i<6; i++){
        randomMac[i] = 101+2*i;
    }

    // First make sure third entry is the oldest used entry
    for(int i=1; i<=ARP_HASH_ENTRY_LIST_SIZE; i++){
        std::string data = "Hello World!";
        unsigned char writeBuffer[ETHERNET_MTU+ETHERNET_SIMPLE_HEADER_SIZE];
        unsigned short identification;
        unsigned int fragmentOffset = 0;

        Pair<IPv4PacketProgress, unsigned int> state = pNetworkStackHandler->handleOutgoingIPv4Packet(NetworkStackHandlerTests::osIP + ARP_HASH_TABLE_SIZE*i, 
            9000, 
            1000, 
            UDP_IPV4_PROTOCOL,
            (unsigned char*)data.c_str(), 
            data.size(), 
            identification, 
            fragmentOffset,
            writeBuffer);
        std::pair<std::unique_ptr<unsigned char[]>, unsigned int> arpReply = createARPReply(randomMac, writeBuffer);
        PacketType packetType = pNetworkStackHandler->handleIncomingEthernetPacket(arpReply.first.get(), arpReply.second);
    }

    // Now make sure all entries are used and entry 3 is the oldest used
    std::string data = "Hello World!";
    unsigned char writeBuffer[ETHERNET_MTU+ETHERNET_SIMPLE_HEADER_SIZE];
    unsigned short identification;
    unsigned int fragmentOffset = 0;

    Pair<IPv4PacketProgress, unsigned int> state = pNetworkStackHandler->handleOutgoingIPv4Packet(NetworkStackHandlerTests::osIP + ARP_HASH_TABLE_SIZE*3, 
        9000, 
        1000, 
        UDP_IPV4_PROTOCOL,
        (unsigned char*)data.c_str(), 
        data.size(), 
        identification, 
        fragmentOffset,
        writeBuffer);
    
    pNetworkStackHandler->incrementTimerCounter();

    for(int i=4; i!=3; i=((i % ARP_HASH_ENTRY_LIST_SIZE) + 1)){
        std::string data = "Hello World!";
        unsigned char writeBuffer[ETHERNET_MTU+ETHERNET_SIMPLE_HEADER_SIZE];
        unsigned short identification;
        unsigned int fragmentOffset = 0;

        Pair<IPv4PacketProgress, unsigned int> state = pNetworkStackHandler->handleOutgoingIPv4Packet(NetworkStackHandlerTests::osIP + ARP_HASH_TABLE_SIZE*i, 
            9000, 
            1000, 
            UDP_IPV4_PROTOCOL,
            (unsigned char*)data.c_str(), 
            data.size(), 
            identification, 
            fragmentOffset,
            writeBuffer);
    }

    for(int i=1; i<=ARP_HASH_ENTRY_LIST_SIZE; i++){
        if(i==3){
            ARPEntry arpEntry = getARPEntry(NetworkStackHandlerTests::osIP + ARP_HASH_TABLE_SIZE*i);
            ASSERT_TRUE(arpEntry.lastUsed==0) << arpEntry.lastUsed << " " << i;
        }
        else{
            ARPEntry arpEntry = getARPEntry(NetworkStackHandlerTests::osIP + ARP_HASH_TABLE_SIZE*i);
            ASSERT_TRUE(arpEntry.lastUsed==1) << arpEntry.lastUsed << " " << i;
        }
    }

    data = "Hello World!";
    fragmentOffset = 0;

    state = pNetworkStackHandler->handleOutgoingIPv4Packet(NetworkStackHandlerTests::osIP + ARP_HASH_TABLE_SIZE*(ARP_HASH_ENTRY_LIST_SIZE+1), 
        9000, 
        1000, 
        UDP_IPV4_PROTOCOL,
        (unsigned char*)data.c_str(), 
        data.size(), 
        identification, 
        fragmentOffset,
        writeBuffer);
    
    for(int i=1; i<=(ARP_HASH_ENTRY_LIST_SIZE+1); i++){
        if(i==3){
            ARPEntry arpEntry = getARPEntry(NetworkStackHandlerTests::osIP + ARP_HASH_TABLE_SIZE*i);
            ASSERT_TRUE(arpEntry.IP==0);
        }
        else{
            ARPEntry arpEntry = getARPEntry(NetworkStackHandlerTests::osIP + ARP_HASH_TABLE_SIZE*i);
            ASSERT_TRUE(arpEntry.IP!=0);
        }
    }
}

TEST_F(NetworkStackHandlerTests, SendingARP_ARPTableFullShouldBeReturned){
    for(int i=1; i<=ARP_HASH_ENTRY_LIST_SIZE; i++){
        std::string data = "Hello World!";
        unsigned char writeBuffer[ETHERNET_MTU+ETHERNET_SIMPLE_HEADER_SIZE];
        unsigned short identification;
        unsigned int fragmentOffset = 0;

        Pair<IPv4PacketProgress, unsigned int> state = pNetworkStackHandler->handleOutgoingIPv4Packet(NetworkStackHandlerTests::osIP + ARP_HASH_TABLE_SIZE*i, 
            9000, 
            1000, 
            UDP_IPV4_PROTOCOL,
            (unsigned char*)data.c_str(), 
            data.size(), 
            identification, 
            fragmentOffset,
            writeBuffer);
    }

    std::string data = "Hello World!";
    unsigned char writeBuffer[ETHERNET_MTU+ETHERNET_SIMPLE_HEADER_SIZE];
    unsigned short identification;
    unsigned int fragmentOffset = 0;

    Pair<IPv4PacketProgress, unsigned int> state = pNetworkStackHandler->handleOutgoingIPv4Packet(NetworkStackHandlerTests::osIP + ARP_HASH_TABLE_SIZE*(ARP_HASH_ENTRY_LIST_SIZE+1), 
        9000, 
        1000, 
        UDP_IPV4_PROTOCOL,
        (unsigned char*)data.c_str(), 
        data.size(), 
        identification, 
        fragmentOffset,
        writeBuffer);
    ASSERT_EQ(state.first, IPv4PacketProgress::ARPTableFull);

    for(int i=1; i<=(ARP_HASH_ENTRY_LIST_SIZE+1); i++){
        if(i==(ARP_HASH_ENTRY_LIST_SIZE+1)){
            ARPEntry arpEntry = getARPEntry(NetworkStackHandlerTests::osIP + ARP_HASH_TABLE_SIZE*i);
            ASSERT_TRUE(arpEntry.IP==0);
        }
        else{
            ARPEntry arpEntry = getARPEntry(NetworkStackHandlerTests::osIP + ARP_HASH_TABLE_SIZE*i);
            ASSERT_TRUE(arpEntry.IP!=0);
        }
    }
}

TEST_F(NetworkStackHandlerTests, SendingARPToNonRespondingHost_BroadcastingSolutionShouldBeUsed){
    std::string data = "Hello World!";
    unsigned char writeBuffer[ETHERNET_MTU+ETHERNET_SIMPLE_HEADER_SIZE];
    unsigned short identification;
    unsigned int fragmentOffset = 0;

    Pair<IPv4PacketProgress, unsigned int> state = pNetworkStackHandler->handleOutgoingIPv4Packet(NetworkStackHandlerTests::clientIP, 
        9000, 
        1000, 
        UDP_IPV4_PROTOCOL,
        (unsigned char*)data.c_str(), 
        data.size(), 
        identification, 
        fragmentOffset,
        writeBuffer);
    ASSERT_EQ(state.first, IPv4PacketProgress::WaitingOnARPReply);

    for(int i=0; i<ARP_REQUEST_TIMEOUT; i++){
        pNetworkStackHandler->incrementTimerCounter();
    }

    fragmentOffset = 0;
    state = pNetworkStackHandler->handleOutgoingIPv4Packet(NetworkStackHandlerTests::clientIP, 
        9000, 
        1000, 
        UDP_IPV4_PROTOCOL,
        (unsigned char*)data.c_str(), 
        data.size(), 
        identification, 
        fragmentOffset,
        writeBuffer);
    ASSERT_EQ(state.first, IPv4PacketProgress::WaitingOnARPReply);

    pNetworkStackHandler->incrementTimerCounter();

    fragmentOffset = 0;
    state = pNetworkStackHandler->handleOutgoingIPv4Packet(NetworkStackHandlerTests::clientIP, 
        9000, 
        1000, 
        UDP_IPV4_PROTOCOL,
        (unsigned char*)data.c_str(), 
        data.size(), 
        identification, 
        fragmentOffset,
        writeBuffer);
    ASSERT_EQ(state.first, IPv4PacketProgress::Done);
    for(int i=0; i<6; i++){
        ASSERT_EQ(writeBuffer[i], 0xFF);
    }

    for(int i=(ARP_REQUEST_TIMEOUT+1); i<ARP_BROADCAST_ON_FAILURE_TIMEOUT; i++){
        pNetworkStackHandler->incrementTimerCounter();
    }

    fragmentOffset = 0;
    state = pNetworkStackHandler->handleOutgoingIPv4Packet(NetworkStackHandlerTests::clientIP, 
        9000, 
        1000, 
        UDP_IPV4_PROTOCOL,
        (unsigned char*)data.c_str(), 
        data.size(), 
        identification, 
        fragmentOffset,
        writeBuffer);
    ASSERT_EQ(state.first, IPv4PacketProgress::Done);

    pNetworkStackHandler->incrementTimerCounter();

    fragmentOffset = 0;
    state = pNetworkStackHandler->handleOutgoingIPv4Packet(NetworkStackHandlerTests::clientIP, 
        9000, 
        1000, 
        UDP_IPV4_PROTOCOL,
        (unsigned char*)data.c_str(), 
        data.size(), 
        identification, 
        fragmentOffset,
        writeBuffer);
    ASSERT_EQ(state.first, IPv4PacketProgress::WaitingOnARPReply);

    for(int i=0; i<ARP_REQUEST_TIMEOUT; i++){
        pNetworkStackHandler->incrementTimerCounter();
    }

    fragmentOffset = 0;
    state = pNetworkStackHandler->handleOutgoingIPv4Packet(NetworkStackHandlerTests::clientIP, 
        9000, 
        1000, 
        UDP_IPV4_PROTOCOL,
        (unsigned char*)data.c_str(), 
        data.size(), 
        identification, 
        fragmentOffset,
        writeBuffer);
    ASSERT_EQ(state.first, IPv4PacketProgress::WaitingOnARPReply);

    pNetworkStackHandler->incrementTimerCounter();

    fragmentOffset = 0;
    state = pNetworkStackHandler->handleOutgoingIPv4Packet(NetworkStackHandlerTests::clientIP, 
        9000, 
        1000, 
        UDP_IPV4_PROTOCOL,
        (unsigned char*)data.c_str(), 
        data.size(), 
        identification, 
        fragmentOffset,
        writeBuffer);
    ASSERT_EQ(state.first, IPv4PacketProgress::Done);
}

TEST_F(NetworkStackHandlerTests, SendingARPToRespondingHost_ARPEntryShouldAutomaticallyTimeoutAfterAWhile){
    std::string data = "Hello World!";
    unsigned char writeBuffer[ETHERNET_MTU+ETHERNET_SIMPLE_HEADER_SIZE];
    unsigned short identification;
    unsigned int fragmentOffset = 0;

    Pair<IPv4PacketProgress, unsigned int> state = pNetworkStackHandler->handleOutgoingIPv4Packet(NetworkStackHandlerTests::clientIP, 
        9000, 
        1000, 
        UDP_IPV4_PROTOCOL,
        (unsigned char*)data.c_str(), 
        data.size(), 
        identification, 
        fragmentOffset,
        writeBuffer);
    ASSERT_EQ(state.first, IPv4PacketProgress::WaitingOnARPReply);

    std::pair<std::unique_ptr<unsigned char[]>, unsigned int> arpReply = createARPReply(NetworkStackHandlerTests::clientMac, writeBuffer);
    PacketType packetType = pNetworkStackHandler->handleIncomingEthernetPacket(arpReply.first.get(), arpReply.second);

    ARPEntry arpEntry = getARPEntry(NetworkStackHandlerTests::clientIP);
    ASSERT_EQ(arpEntry.lastRequested, 0);
    ASSERT_EQ(arpEntry.lastAnswered, 0);
    ASSERT_EQ(arpEntry.lastUsed, -1);

    fragmentOffset = 0;
    state = pNetworkStackHandler->handleOutgoingIPv4Packet(NetworkStackHandlerTests::clientIP, 
        9000, 
        1000, 
        UDP_IPV4_PROTOCOL,
        (unsigned char*)data.c_str(), 
        data.size(), 
        identification, 
        fragmentOffset,
        writeBuffer);
    ASSERT_EQ(state.first, IPv4PacketProgress::Done);

    arpEntry = getARPEntry(NetworkStackHandlerTests::clientIP);
    ASSERT_EQ(arpEntry.lastRequested, 0);
    ASSERT_EQ(arpEntry.lastAnswered, 0);
    ASSERT_EQ(arpEntry.lastUsed, 0);

    for(int i=0; i<ARP_ENTRY_TIMEOUT; i++){
        pNetworkStackHandler->incrementTimerCounter();
    }

    fragmentOffset = 0;
    state = pNetworkStackHandler->handleOutgoingIPv4Packet(NetworkStackHandlerTests::clientIP, 
        9000, 
        1000, 
        UDP_IPV4_PROTOCOL,
        (unsigned char*)data.c_str(), 
        data.size(), 
        identification, 
        fragmentOffset,
        writeBuffer);
    ASSERT_EQ(state.first, IPv4PacketProgress::Done);

    arpEntry = getARPEntry(NetworkStackHandlerTests::clientIP);
    ASSERT_EQ(arpEntry.lastRequested, 0);
    ASSERT_EQ(arpEntry.lastAnswered, 0);
    ASSERT_EQ(arpEntry.lastUsed, ARP_ENTRY_TIMEOUT);

    pNetworkStackHandler->incrementTimerCounter();

    fragmentOffset = 0;
    state = pNetworkStackHandler->handleOutgoingIPv4Packet(NetworkStackHandlerTests::clientIP, 
        9000, 
        1000, 
        UDP_IPV4_PROTOCOL,
        (unsigned char*)data.c_str(), 
        data.size(), 
        identification, 
        fragmentOffset,
        writeBuffer);
    ASSERT_EQ(state.first, IPv4PacketProgress::WaitingOnARPReply);

    arpEntry = getARPEntry(NetworkStackHandlerTests::clientIP);
    ASSERT_EQ(arpEntry.lastRequested, ARP_ENTRY_TIMEOUT+1);
    ASSERT_EQ(arpEntry.lastAnswered, 0);
    ASSERT_EQ(arpEntry.lastUsed, ARP_ENTRY_TIMEOUT);

    pNetworkStackHandler->incrementTimerCounter();

    fragmentOffset = 0;
    state = pNetworkStackHandler->handleOutgoingIPv4Packet(NetworkStackHandlerTests::clientIP, 
        9000, 
        1000, 
        UDP_IPV4_PROTOCOL,
        (unsigned char*)data.c_str(), 
        data.size(), 
        identification, 
        fragmentOffset,
        writeBuffer);
    ASSERT_EQ(state.first, IPv4PacketProgress::WaitingOnARPReply);

    packetType = pNetworkStackHandler->handleIncomingEthernetPacket(arpReply.first.get(), arpReply.second);

    arpEntry = getARPEntry(NetworkStackHandlerTests::clientIP);
    ASSERT_EQ(arpEntry.lastRequested, ARP_ENTRY_TIMEOUT+1);
    ASSERT_EQ(arpEntry.lastAnswered, ARP_ENTRY_TIMEOUT+2);
    ASSERT_EQ(arpEntry.lastUsed, ARP_ENTRY_TIMEOUT);

    pNetworkStackHandler->incrementTimerCounter();

    fragmentOffset = 0;
    state = pNetworkStackHandler->handleOutgoingIPv4Packet(NetworkStackHandlerTests::clientIP, 
        9000, 
        1000, 
        UDP_IPV4_PROTOCOL,
        (unsigned char*)data.c_str(), 
        data.size(), 
        identification, 
        fragmentOffset,
        writeBuffer);
    ASSERT_EQ(state.first, IPv4PacketProgress::Done);

    arpEntry = getARPEntry(NetworkStackHandlerTests::clientIP);
    ASSERT_EQ(arpEntry.lastRequested, ARP_ENTRY_TIMEOUT+1);
    ASSERT_EQ(arpEntry.lastAnswered, ARP_ENTRY_TIMEOUT+2);
    ASSERT_EQ(arpEntry.lastUsed, ARP_ENTRY_TIMEOUT+3);
}

TEST_F(NetworkStackHandlerTests, SendingIPv4Packets_AllPacketFormatShouldBeCorrect){
    std::string data = "        Hello World!";
    data[0] = 9000 >> 8;
    data[1] = 9000 & 0xFF;
    data[2] = 1000 >> 8;
    data[3] = 1000 & 0xFF;
    data[4] = data.size() >> 8;
    data[5] = data.size() & 0xFF;
    data[6] = 0x00;
    data[7] = 0x00;
    unsigned char writeBuffer[ETHERNET_MTU+ETHERNET_SIMPLE_HEADER_SIZE];
    unsigned short identification = 99;
    unsigned int fragmentOffset = 0;

    Pair<IPv4PacketProgress, unsigned int> state = pNetworkStackHandler->handleOutgoingIPv4Packet(NetworkStackHandlerTests::clientIP, 
        9000, 
        1000, 
        UDP_IPV4_PROTOCOL,
        (unsigned char*)data.c_str(), 
        data.size(), 
        identification, 
        fragmentOffset,
        writeBuffer);
    std::pair<std::unique_ptr<unsigned char[]>, unsigned int> arpRequest = createARPRequest(NetworkStackHandlerTests::osMac, NetworkStackHandlerTests::osIP, NetworkStackHandlerTests::clientIP);
    ASSERT_EQ(state.second, arpRequest.second);
    ASSERT_EQ(memcmp(writeBuffer, arpRequest.first.get(), arpRequest.second), 0);

    std::pair<std::unique_ptr<unsigned char[]>, unsigned int> arpReply = createARPReply(NetworkStackHandlerTests::clientMac, writeBuffer);
    PacketType packetType = pNetworkStackHandler->handleIncomingEthernetPacket(arpReply.first.get(), arpReply.second);

    fragmentOffset = 0;
    state = pNetworkStackHandler->handleOutgoingIPv4Packet(NetworkStackHandlerTests::clientIP, 
        9000, 
        1000, 
        UDP_IPV4_PROTOCOL,
        (unsigned char*)data.c_str(), 
        data.size(), 
        identification, 
        fragmentOffset,
        writeBuffer);
    ASSERT_EQ(state.first, IPv4PacketProgress::Done);
    ASSERT_EQ(identification, 0);
    ASSERT_EQ(fragmentOffset, data.size());
    std::vector<std::pair<std::unique_ptr<unsigned char[]>, unsigned int>> packets = convertUDPToEthernetPackets(NetworkStackHandlerTests::osMac, 
        NetworkStackHandlerTests::clientMac, 
        NetworkStackHandlerTests::osIP, 
        NetworkStackHandlerTests::clientIP, 
        9000, 1000, data.c_str()+UDP_HEADER_SIZE, data.size()-UDP_HEADER_SIZE, MAX_IPV4_FRAGMENT_SIZE, identification);
    ASSERT_EQ(state.second, packets[0].second);
    // Remove checksum from packets[0].first ipv4 header
    packets[0].first[ETHERNET_SIMPLE_HEADER_SIZE+10] = 0;
    packets[0].first[ETHERNET_SIMPLE_HEADER_SIZE+11] = 0;
    ASSERT_EQ(memcmp(writeBuffer, packets[0].first.get(), packets[0].second), 0);

    data = "        " + generateRandomString(11000);
    data[0] = 9000 >> 8;
    data[1] = 9000 & 0xFF;
    data[2] = 1000 >> 8;
    data[3] = 1000 & 0xFF;
    data[4] = data.size() >> 8;
    data[5] = data.size() & 0xFF;
    data[6] = 0x00;
    data[7] = 0x00;
    fragmentOffset = 0;
    packets = convertUDPToEthernetPackets(NetworkStackHandlerTests::osMac, 
        NetworkStackHandlerTests::clientMac, 
        NetworkStackHandlerTests::osIP, 
        NetworkStackHandlerTests::clientIP, 
        9000, 1000, data.c_str()+UDP_HEADER_SIZE, data.size()-UDP_HEADER_SIZE, MAX_IPV4_FRAGMENT_SIZE, 1);
    for(int i=0; i<packets.size(); i++){
        state = pNetworkStackHandler->handleOutgoingIPv4Packet(NetworkStackHandlerTests::clientIP, 
            9000, 
            1000, 
            UDP_IPV4_PROTOCOL,
            (unsigned char*)data.c_str(), 
            data.size(), 
            identification, 
            fragmentOffset,
            writeBuffer);
        if(i==packets.size()-1){
            ASSERT_EQ(state.first, IPv4PacketProgress::Done);
            ASSERT_EQ(fragmentOffset, data.size());
        }
        else{
            ASSERT_EQ(state.first, IPv4PacketProgress::SendingFragment);
            ASSERT_EQ(fragmentOffset, (ETHERNET_MTU-IP4_HDRLEN)*(i+1));
        }
        ASSERT_EQ(identification, 1);
        ASSERT_EQ(state.second, packets[i].second);
        // Remove checksum from packets[i].first ipv4 header
        packets[i].first[ETHERNET_SIMPLE_HEADER_SIZE+10] = 0;
        packets[i].first[ETHERNET_SIMPLE_HEADER_SIZE+11] = 0;
        /*int index = 0;
        for(int j=0; j<packets[i].second; j++){
            if(writeBuffer[j]!=packets[i].first.get()[j]){
                index = j;
                break;
            }
        }
        ASSERT_EQ(memcmp(writeBuffer, packets[i].first.get(), packets[i].second), 0) << "\n" << (unsigned int)writeBuffer[index] << "\n" << (unsigned int)packets[i].first[index] << "\n" << index << "\n" << i << "\n";*/
        ASSERT_EQ(memcmp(writeBuffer, packets[i].first.get(), packets[i].second), 0);
    }
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}