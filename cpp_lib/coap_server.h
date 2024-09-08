#pragma once

#include "list.h"

#define MAX_COAP_PATH_SIZE 100
#define MAX_NUM_COAP_PATHS 10

#define RECEIVE_BUFFER_SIZE (3*0x10000)
// TODO: actually, this RESPONSE_BUFFER_SIZE is impossible, there is a limit on how many bytes can be sent in a 
// single UDP packet because IP packet size is limited
#define RESPONSE_BUFFER_SIZE 0x10000
#define WRITE_BUFFER_SIZE (3*RESPONSE_BUFFER_SIZE)

#define CREATED_RESPONSE_CODE 0x41
#define DELETED_RESPONSE_CODE 0x42
#define CHANGED_RESPONSE_CODE 0x44
#define CONTENT_RESPONSE_CODE 0x45
#define BAD_REQUEST_RESPONSE_CODE 0x80
#define FORBIDDEN_RESPONSE_CODE 0x83
#define NOT_FOUND_RESPONSE_CODE 0x84
#define METHOD_NOT_ALLOWED_RESPONSE_CODE 0x85
#define CONFLICT_RESPONSE_CODE 0x89
#define UNSUPPORTED_CONTENT_FORMAT_RESPONSE_CODE 0x8F
#define INTERNAL_SERVER_ERROR_RESPONSE_CODE 0xA0
#define NOT_IMPLEMENTED_RESPONSE_CODE 0xA1
#define SERVICE_UNAVAILABLE_RESPONSE_CODE 0xA3

enum class ContentFormat{
    Text_Plain_Charset_UTF8,
    Application_Link_Format,
    Application_XML,
    Application_Octet_Stream,
    Application_EXI,
    Application_JSON
};

typedef struct CoAPResponse{
    unsigned char responseCode;
    unsigned int responseSize;
    ContentFormat contentFormat;
} CoAPResponse;

class CoAPHandler{
    public:
        virtual ~CoAPHandler() = default;

        virtual CoAPResponse handleGET(char* path,
            ContentFormat contentFormat,
            unsigned char* payload, 
            unsigned int payloadSize, 
            unsigned char responseBuffer[RESPONSE_BUFFER_SIZE]);
        virtual CoAPResponse handlePOST(char* path,
            ContentFormat contentFormat,
            unsigned char* payload, 
            unsigned int payloadSize, 
            unsigned char responseBuffer[RESPONSE_BUFFER_SIZE]);
        virtual CoAPResponse handlePUT(char* path,
            ContentFormat contentFormat,
            unsigned char* payload, 
            unsigned int payloadSize, 
            unsigned char responseBuffer[RESPONSE_BUFFER_SIZE]);
        virtual CoAPResponse handleDELETE(char* path,
            ContentFormat contentFormat,
            unsigned char* payload, 
            unsigned int payloadSize, 
            unsigned char responseBuffer[RESPONSE_BUFFER_SIZE]);
        
        static void operator delete (void *p) {
            return;
        }
};

typedef struct CoAPPath{
    char path[MAX_COAP_PATH_SIZE];
    unsigned int pathSize;
    unsigned int pathProgressCounter;
    CoAPHandler* pHandler;
} CoAPPath;


class CoAPServer{
    private:
        void handlePacketFromReceiveBuffer(unsigned int sourceIP, unsigned short sourcePort, unsigned short udpPayloadSize, unsigned char* udpPayload);
        
        void attemptReceiveBufferSwap();
        void attemptWriteBufferSwap();

        MultiLinkedListElement<2, CoAPPath>* trimListBasedOnNewCharacter(MultiLinkedListElement<2, CoAPPath>* possibleValidPathsHead, char newCharacter, bool isFirstCharacter);

        MultiLinkedListElement<2, CoAPPath> pathListElements[MAX_NUM_COAP_PATHS];
        MultiLinkedListElement<2, CoAPPath>* unusedPathListElementsHead;
        MultiLinkedListElement<2, CoAPPath>* usedPathListElementsHead;
        int socketID;

        unsigned char receiveBuffers[2][RECEIVE_BUFFER_SIZE];
        unsigned int currentlyWrittenToReceiveBuffer;
        unsigned char writeBuffers[2][WRITE_BUFFER_SIZE];
        unsigned int currentlySendingWritebuffer;
        int currentlySendingWritebufferIndicatorWhenFinished;
        unsigned int currentlyWrittenToWriteBufferDataLen;

        unsigned short udpPort;
    
    public:
        CoAPServer(unsigned short udpPort);
        ~CoAPServer();

        void setPathHandler(char* path, CoAPHandler* pHandler);
        void removePathHandler(char* path);

        void run();
};