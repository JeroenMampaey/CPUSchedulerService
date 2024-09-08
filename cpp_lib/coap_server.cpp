#include "coap_server.h"
#include "syscalls.h"
#include "string.h"
#include "mem.h"

#define COAP_UDP_PORT 5683
#define COAP_PACKET_MINIMUM_SIZE 4
#define RESPONSE_OPTIONS_SIZE 2
#define PAYLOAD_MARKER_SIZE 1

CoAPResponse CoAPHandler::handleGET(char* path,
            ContentFormat contentFormat,
            unsigned char* payload, 
            unsigned int payloadSize, 
            unsigned char responseBuffer[RESPONSE_BUFFER_SIZE]){
    CoAPResponse response;
    response.responseCode = METHOD_NOT_ALLOWED_RESPONSE_CODE;
    response.responseSize = 0;
    response.contentFormat = ContentFormat::Text_Plain_Charset_UTF8;
    return response;
}

CoAPResponse CoAPHandler::handlePOST(char* path,
            ContentFormat contentFormat,
            unsigned char* payload, 
            unsigned int payloadSize, 
            unsigned char responseBuffer[RESPONSE_BUFFER_SIZE]){
    CoAPResponse response;
    response.responseCode = METHOD_NOT_ALLOWED_RESPONSE_CODE;
    response.responseSize = 0;
    response.contentFormat = ContentFormat::Text_Plain_Charset_UTF8;
    return response;
}

CoAPResponse CoAPHandler::handlePUT(char* path,
            ContentFormat contentFormat,
            unsigned char* payload, 
            unsigned int payloadSize, 
            unsigned char responseBuffer[RESPONSE_BUFFER_SIZE]){
    CoAPResponse response;
    response.responseCode = METHOD_NOT_ALLOWED_RESPONSE_CODE;
    response.responseSize = 0;
    response.contentFormat = ContentFormat::Text_Plain_Charset_UTF8;
    return response;
}

CoAPResponse CoAPHandler::handleDELETE(char* path,
            ContentFormat contentFormat,
            unsigned char* payload, 
            unsigned int payloadSize, 
            unsigned char responseBuffer[RESPONSE_BUFFER_SIZE]){
    CoAPResponse response;
    response.responseCode = METHOD_NOT_ALLOWED_RESPONSE_CODE;
    response.responseSize = 0;
    response.contentFormat = ContentFormat::Text_Plain_Charset_UTF8;
    return response;
}

CoAPServer::CoAPServer(unsigned short udpPort)
    :
    udpPort(udpPort)
{
    currentlyWrittenToReceiveBuffer = 0;
    currentlySendingWritebuffer = 1;
    currentlySendingWritebufferIndicatorWhenFinished = 1;
    currentlyWrittenToWriteBufferDataLen = 0;

    for(int i=0; i<MAX_NUM_COAP_PATHS; i++){
        pathListElements[i].value.pathSize = 0;
        if(i!=MAX_NUM_COAP_PATHS-1){
            pathListElements[i].next[0] = &pathListElements[i+1];
        }
        else{
            pathListElements[i].next[0] = nullptr;
        }
    }
    unusedPathListElementsHead = &pathListElements[0];
    usedPathListElementsHead = nullptr;

    socketID = openSocket(udpPort);
    setReceiveBuffer(socketID, receiveBuffers[currentlyWrittenToReceiveBuffer], RECEIVE_BUFFER_SIZE);
}

CoAPServer::~CoAPServer(){
    closeSocket(socketID);
}

void CoAPServer::setPathHandler(char* path, CoAPHandler* pHandler){
    unsigned int pathSize = strlen(path);

    if(pathSize > MAX_COAP_PATH_SIZE){
        return;
    }

    if(unusedPathListElementsHead == nullptr){
        return;
    }

    MultiLinkedListElement<2, CoAPPath>* pNewPathListElement = unusedPathListElementsHead;
    unusedPathListElementsHead = unusedPathListElementsHead->next[0];
    pNewPathListElement->next[0] = usedPathListElementsHead;
    usedPathListElementsHead = pNewPathListElement;

    pNewPathListElement->value.pathSize = pathSize;
    memCopy((unsigned char*)path, (unsigned char*)pNewPathListElement->value.path, pathSize);
    pNewPathListElement->value.pathProgressCounter = 0;
    pNewPathListElement->value.pHandler = pHandler;
}

void CoAPServer::removePathHandler(char* path){
    unsigned int pathSize = strlen(path);

    if(pathSize > MAX_COAP_PATH_SIZE){
        return;
    }

    MultiLinkedListElement<2, CoAPPath>* pCurrentPathListElement = usedPathListElementsHead;
    MultiLinkedListElement<2, CoAPPath>* pPreviousPathListElement = nullptr;
    while(pCurrentPathListElement != nullptr){
        if(pCurrentPathListElement->value.pathSize == pathSize){
            bool found = memCompare((unsigned char*)pCurrentPathListElement->value.path, (unsigned char*)path, pathSize)==0;
            if(found){
                if(pPreviousPathListElement==nullptr){
                    usedPathListElementsHead = pCurrentPathListElement->next[0];
                }
                else{
                    pPreviousPathListElement->next[0] = pCurrentPathListElement->next[0];
                }
                pCurrentPathListElement->next[0] = unusedPathListElementsHead;
                unusedPathListElementsHead = pCurrentPathListElement;
                return;
            }
        }
        pPreviousPathListElement = pCurrentPathListElement;
        pCurrentPathListElement = pCurrentPathListElement->next[0];
    }
}

void CoAPServer::attemptReceiveBufferSwap(){
    unsigned char* receiveBufferHeaderBegin = receiveBuffers[currentlyWrittenToReceiveBuffer];
    if(receiveBufferHeaderBegin[6]!=0 || receiveBufferHeaderBegin[7]!=0){
        setReceiveBuffer(socketID, receiveBuffers[1-currentlyWrittenToReceiveBuffer], RECEIVE_BUFFER_SIZE);

        unsigned int currentUsedReceivebufferOffset = 0;
        while(receiveBufferHeaderBegin[6]!=0 || receiveBufferHeaderBegin[7]!=0){
            unsigned int sourceIP = (((unsigned int)receiveBufferHeaderBegin[3]) << 24) + (((unsigned int)receiveBufferHeaderBegin[2]) << 16) + (((unsigned int)receiveBufferHeaderBegin[1]) << 8) + ((unsigned int)receiveBufferHeaderBegin[0]);
            unsigned short sourcePort = (((unsigned short)receiveBufferHeaderBegin[5]) << 8) + ((unsigned short)receiveBufferHeaderBegin[4]);
            unsigned short packetSize = (((unsigned int)receiveBufferHeaderBegin[7]) << 8) + ((unsigned int)receiveBufferHeaderBegin[6]);

            unsigned char* udpPayload = receiveBuffers[currentlyWrittenToReceiveBuffer] + currentUsedReceivebufferOffset + RECEIVE_BUFFER_HEADER_SIZE;
            unsigned int udpPayloadSize = packetSize;

            handlePacketFromReceiveBuffer(sourceIP, sourcePort, udpPayloadSize, udpPayload);

            currentUsedReceivebufferOffset += RECEIVE_BUFFER_HEADER_SIZE + packetSize;
            receiveBufferHeaderBegin = receiveBuffers[currentlyWrittenToReceiveBuffer] + currentUsedReceivebufferOffset;
        }

        currentlyWrittenToReceiveBuffer = 1 - currentlyWrittenToReceiveBuffer;
    }
}

void CoAPServer::attemptWriteBufferSwap(){
    if(currentlySendingWritebufferIndicatorWhenFinished == 1 && currentlyWrittenToWriteBufferDataLen > 0){
        setSendBuffer(socketID, writeBuffers[1-currentlySendingWritebuffer], currentlyWrittenToWriteBufferDataLen, &currentlySendingWritebufferIndicatorWhenFinished);
        currentlySendingWritebuffer = 1 - currentlySendingWritebuffer;
        currentlyWrittenToWriteBufferDataLen = 0;
    }
}

void CoAPServer::run(){
    while(1){
        attemptReceiveBufferSwap();
        attemptWriteBufferSwap();

        yield();
    }
}

void CoAPServer::handlePacketFromReceiveBuffer(unsigned int sourceIP, unsigned short sourcePort, unsigned short udpPayloadSize, unsigned char* udpPayload){
    if(udpPayloadSize < 4){
        return;
    }

    unsigned int version = (udpPayload[0] & 0b11000000) >> 6;
    // This server is based on RFC 7252, which covers CoAP version 1
    if(version != 1){
        return;
    }

    unsigned int type = (udpPayload[0] & 0b110000) >> 4;
    // Request can only be confirmable or non-confirmable
    if(type!=0 && type!=1){
        return;
    }

    bool requiresAck = (type==0);

    unsigned int tokenLength = udpPayload[0] & 0b1111;
    if(tokenLength > 8){
        return;
    }

    unsigned int codeClass = (udpPayload[1] & 0b11100000) >> 5;
    unsigned int codeDetail = (udpPayload[1] & 0b11111);
    // We expect requests thus code class should be 0
    if(codeClass != 0){
        return;
    }

    unsigned char messageIDLower = udpPayload[2];
    unsigned char messageIDUpper = udpPayload[3];

    unsigned char token[8];
    // Let's first make sure we can atleast read the token
    if(udpPayloadSize < 4 + tokenLength){
        return;
    }
    for(int i=0; i<tokenLength; i++){
        token[i] = udpPayload[4+i];
    }

    char path[MAX_COAP_PATH_SIZE+1]; // +1 since this path should be '\0' terminated
    unsigned int pathSizeSoFar = 0;
    unsigned int nextIndex = 0;
    MultiLinkedListElement<2, CoAPPath>* possibleValidPathsHead = usedPathListElementsHead;

    ContentFormat contentFormat = ContentFormat::Text_Plain_Charset_UTF8;

    unsigned int currentOptionOffset = 4 + tokenLength;
    unsigned int currentOptionNumber = 0;
    while(currentOptionOffset < udpPayloadSize && udpPayload[currentOptionOffset] != 0xFF){
        unsigned int optionDelta = (udpPayload[currentOptionOffset] & 0b11110000) >> 4;
        unsigned int optionLength = (udpPayload[currentOptionOffset] & 0b1111);
        unsigned int numExtendedBytes = 0;
        if(optionDelta == 13){
            numExtendedBytes += 1;
        }
        else if(optionDelta == 14){
            numExtendedBytes += 2;
        }
        else if(optionDelta == 15){
            // Reserved
            return;
        }
        if(optionLength == 13){
            numExtendedBytes += 1;
        }
        else if(optionLength == 14){
            numExtendedBytes += 2;
        }
        else if(optionLength == 15){
            // Reserved
            return;
        }

        // Check if we can atleast read the required extended fields
        if(udpPayloadSize <= currentOptionOffset + numExtendedBytes){
            return;
        }

        if(optionDelta == 13){
            optionDelta = udpPayload[currentOptionOffset+1] + 13;
            currentOptionOffset += 1;
        }
        else if(optionDelta == 14){
            optionDelta = (((unsigned int)udpPayload[currentOptionOffset+1]) << 8) + ((unsigned int)udpPayload[currentOptionOffset+2]) + 269;
            currentOptionOffset += 2;
        }
        if(optionLength == 13){
            optionLength = udpPayload[currentOptionOffset+1] + 13;
            currentOptionOffset += 1;
        }
        else if(optionLength == 14){
            optionLength = (((unsigned int)udpPayload[currentOptionOffset+1]) << 8) + ((unsigned int)udpPayload[currentOptionOffset+2]) + 269;
            currentOptionOffset += 2;
        }

        currentOptionNumber += optionDelta;
        // Option number cannot be more than 65535
        if(currentOptionNumber > 65535){
            return;
        }

        // Next byte now is the start of the option value, make sure we can read all bytes of the value
        currentOptionOffset += 1;
        if(udpPayloadSize < currentOptionOffset + optionLength){
            return;
        }

        if(currentOptionNumber==11 && possibleValidPathsHead!=nullptr){
            // Check if pathsize is not too big
            if(pathSizeSoFar + (1+optionLength) > MAX_COAP_PATH_SIZE){
                possibleValidPathsHead==nullptr;
            }
            else{
                path[pathSizeSoFar] = '/';
                possibleValidPathsHead = trimListBasedOnNewCharacter(possibleValidPathsHead, '/', pathSizeSoFar==0);
            }

            for(int i=0; i<optionLength && possibleValidPathsHead!=nullptr; i++){
                path[pathSizeSoFar+1+i] = udpPayload[currentOptionOffset + i];
                possibleValidPathsHead = trimListBasedOnNewCharacter(possibleValidPathsHead, udpPayload[currentOptionOffset + i], false);
            }

            pathSizeSoFar += 1 + optionLength;
        }
        else if(currentOptionNumber==12){
            if(optionLength==0){
                contentFormat = ContentFormat::Text_Plain_Charset_UTF8;
            }
            else if(optionLength==1){
                unsigned int contentFormatIdentifier = udpPayload[currentOptionOffset];
                
                switch(contentFormatIdentifier){
                    case 0:
                        contentFormat = ContentFormat::Text_Plain_Charset_UTF8;
                        break;
                    case 40:
                        contentFormat = ContentFormat::Application_Link_Format;
                        break;
                    case 41:
                        contentFormat = ContentFormat::Application_XML;
                        break;
                    case 42:
                        contentFormat = ContentFormat::Application_Octet_Stream;
                        break;
                    case 47:
                        contentFormat = ContentFormat::Application_EXI;
                        break;
                    case 50:
                        contentFormat = ContentFormat::Application_JSON;
                        break;
                    default:
                        // Content format is not recognized
                        return;
                }
            }
            else{
                // Content format is not recognized
                return;
            }
        }

        currentOptionOffset += optionLength;
    }

    // possibleValidPathsHead should point to a list of possible valid paths or nullptr if there are no possible paths
    path[pathSizeSoFar] = '\0';
    possibleValidPathsHead = trimListBasedOnNewCharacter(possibleValidPathsHead, '\0', pathSizeSoFar==0);

    unsigned int requiredWritebufferSize = 
        (possibleValidPathsHead==nullptr || codeDetail==0 || codeDetail>4)
        ? SEND_BUFFER_HEADER_SIZE+UDP_HEADER_SIZE+COAP_PACKET_MINIMUM_SIZE+tokenLength : SEND_BUFFER_HEADER_SIZE+UDP_HEADER_SIZE+COAP_PACKET_MINIMUM_SIZE+tokenLength+RESPONSE_OPTIONS_SIZE+PAYLOAD_MARKER_SIZE+RESPONSE_BUFFER_SIZE;
    
    attemptWriteBufferSwap();
    if(WRITE_BUFFER_SIZE-currentlyWrittenToWriteBufferDataLen < requiredWritebufferSize){
        // Simply not possible to send a response
        return;
    }

    unsigned char* writeBufferForResponse = writeBuffers[1-currentlySendingWritebuffer] + currentlyWrittenToWriteBufferDataLen;
    writeBufferForResponse[0] = (unsigned char)(sourceIP & 0xFF);
    writeBufferForResponse[1] = (unsigned char)((sourceIP >> 8) & 0xFF);
    writeBufferForResponse[2] = (unsigned char)((sourceIP >> 16) & 0xFF);
    writeBufferForResponse[3] = (unsigned char)((sourceIP >> 24) & 0xFF);
    writeBufferForResponse[4] = (unsigned char)(sourcePort & 0xFF);
    writeBufferForResponse[5] = (unsigned char)((sourcePort >> 8) & 0xFF);
    /*
        writeBufferForResponse[6 - 7] are the packet size, we will fill this in later
    */
    for(int i=0; i<UDP_HEADER_SIZE; i++){
        writeBufferForResponse[SEND_BUFFER_HEADER_SIZE+i] = 0;
    }
    writeBufferForResponse[SEND_BUFFER_HEADER_SIZE+UDP_HEADER_SIZE] = 0b1000000 + tokenLength;
    
    if(requiresAck){
        // Acknowledgement <-> type == 2
        writeBufferForResponse[SEND_BUFFER_HEADER_SIZE+UDP_HEADER_SIZE] += 0b100000;
    }
    else{
        // Non-conformible <-> type == 1
        writeBufferForResponse[SEND_BUFFER_HEADER_SIZE+UDP_HEADER_SIZE] += 0b10000;
    }

    /*
        writeBufferForResponse[SEND_BUFFER_HEADER_SIZE+UDP_HEADER_SIZE+1] is the code, we will fill this in later
    */

    writeBufferForResponse[SEND_BUFFER_HEADER_SIZE+UDP_HEADER_SIZE+2] = messageIDLower;
    writeBufferForResponse[SEND_BUFFER_HEADER_SIZE+UDP_HEADER_SIZE+3] = messageIDUpper;
    for(int i=0; i<tokenLength; i++){
        writeBufferForResponse[SEND_BUFFER_HEADER_SIZE+UDP_HEADER_SIZE+4+i] = token[i];
    }

    /*
        writeBufferForResponse[SEND_BUFFER_HEADER_SIZE+UDP_HEADER_SIZE+COAP_PACKET_MINIMUM_SIZE+tokenLength] is the start of the Content-Format options, we will fill this in later
        writeBufferForResponse[SEND_BUFFER_HEADER_SIZE+UDP_HEADER_SIZE+COAP_PACKET_MINIMUM_SIZE+tokenLength+RESPONSE_OPTIONS_SIZE] is where the payloadmarker is, we will fill this in later
    */

    CoAPResponse response;
    if(possibleValidPathsHead != nullptr){
        unsigned int payloadSize = udpPayloadSize;
        if(udpPayloadSize < (currentOptionOffset+PAYLOAD_MARKER_SIZE)){
            payloadSize = 0;
        }
        else{
            payloadSize = udpPayloadSize - (currentOptionOffset+PAYLOAD_MARKER_SIZE);
        }

        unsigned char* payload = nullptr;
        if(payloadSize > 0){
            payload = udpPayload + currentOptionOffset;

            if(*payload != 0xFF){
                // Payload marker is not present
                return;
            }

            payload += PAYLOAD_MARKER_SIZE;   
        }
        
        switch(codeDetail){
            case 1:
                response = possibleValidPathsHead->value.pHandler->handleGET(path, contentFormat, 
                    payload, payloadSize,
                    writeBufferForResponse+SEND_BUFFER_HEADER_SIZE+UDP_HEADER_SIZE+COAP_PACKET_MINIMUM_SIZE+tokenLength+RESPONSE_OPTIONS_SIZE+PAYLOAD_MARKER_SIZE);
                break;
            case 2:
                response = possibleValidPathsHead->value.pHandler->handlePOST(path, contentFormat,
                    payload, payloadSize,
                    writeBufferForResponse+SEND_BUFFER_HEADER_SIZE+UDP_HEADER_SIZE+COAP_PACKET_MINIMUM_SIZE+tokenLength+RESPONSE_OPTIONS_SIZE+PAYLOAD_MARKER_SIZE);
                break;
            case 3:
                response = possibleValidPathsHead->value.pHandler->handlePUT(path, contentFormat,
                    payload, payloadSize,
                    writeBufferForResponse+SEND_BUFFER_HEADER_SIZE+UDP_HEADER_SIZE+COAP_PACKET_MINIMUM_SIZE+tokenLength+RESPONSE_OPTIONS_SIZE+PAYLOAD_MARKER_SIZE);
                break;
            case 4:
                response = possibleValidPathsHead->value.pHandler->handleDELETE(path, contentFormat,
                    payload, payloadSize,
                    writeBufferForResponse+SEND_BUFFER_HEADER_SIZE+UDP_HEADER_SIZE+COAP_PACKET_MINIMUM_SIZE+tokenLength+RESPONSE_OPTIONS_SIZE+PAYLOAD_MARKER_SIZE);
                break;
            default:
                response = {METHOD_NOT_ALLOWED_RESPONSE_CODE, 0, ContentFormat::Text_Plain_Charset_UTF8};
                break;
        }
    }
    else{
        response = {NOT_FOUND_RESPONSE_CODE, 0, ContentFormat::Text_Plain_Charset_UTF8};
    }

    if(response.responseSize > RESPONSE_BUFFER_SIZE){
        response.responseCode = INTERNAL_SERVER_ERROR_RESPONSE_CODE;
        response.responseSize = 0;
    }

    if(response.responseSize > 0){
        unsigned char contentFormatIdentifier = 0;
        switch(response.contentFormat){
            case ContentFormat::Text_Plain_Charset_UTF8:
                contentFormatIdentifier = 0;
                break;
            case ContentFormat::Application_Link_Format:
                contentFormatIdentifier = 40;
                break;
            case ContentFormat::Application_XML:
                contentFormatIdentifier = 41;
                break;
            case ContentFormat::Application_Octet_Stream:
                contentFormatIdentifier = 42;
                break;
            case ContentFormat::Application_EXI:
                contentFormatIdentifier = 47;
                break;
            case ContentFormat::Application_JSON:
                contentFormatIdentifier = 50;
                break;
            default:
                // response.contentFormat is not recognized
                response.responseCode = INTERNAL_SERVER_ERROR_RESPONSE_CODE;
                response.responseSize = 0;
                break;
        }

        if(response.responseSize > 0){
            writeBufferForResponse[SEND_BUFFER_HEADER_SIZE+UDP_HEADER_SIZE+COAP_PACKET_MINIMUM_SIZE+tokenLength] = 0b11000001;
            writeBufferForResponse[SEND_BUFFER_HEADER_SIZE+UDP_HEADER_SIZE+COAP_PACKET_MINIMUM_SIZE+tokenLength+1] = contentFormatIdentifier;

            writeBufferForResponse[SEND_BUFFER_HEADER_SIZE+UDP_HEADER_SIZE+COAP_PACKET_MINIMUM_SIZE+tokenLength+RESPONSE_OPTIONS_SIZE] = 0xFF;
        }
    }

    writeBufferForResponse[SEND_BUFFER_HEADER_SIZE+UDP_HEADER_SIZE+1] = response.responseCode;

    unsigned short packetSize = UDP_HEADER_SIZE+COAP_PACKET_MINIMUM_SIZE+tokenLength+(response.responseSize>0 ? RESPONSE_OPTIONS_SIZE+PAYLOAD_MARKER_SIZE+response.responseSize : 0);
    writeBufferForResponse[6] = (unsigned char)(packetSize & 0xFF);
    writeBufferForResponse[7] = (unsigned char)((packetSize >> 8) & 0xFF);

    currentlyWrittenToWriteBufferDataLen += SEND_BUFFER_HEADER_SIZE + ((unsigned int)packetSize);

    attemptWriteBufferSwap();
}

MultiLinkedListElement<2, CoAPPath>* CoAPServer::trimListBasedOnNewCharacter(MultiLinkedListElement<2, CoAPPath>* possibleValidPathsHead, char newCharacter, bool isFirstCharacter){
    MultiLinkedListElement<2, CoAPPath>* pCurrentPathListElement = possibleValidPathsHead;
    MultiLinkedListElement<2, CoAPPath>* pLastValidPathListElement = nullptr;
    MultiLinkedListElement<2, CoAPPath>* pNewHead = nullptr;
    
    while(pCurrentPathListElement != nullptr){
        if(isFirstCharacter){
            pCurrentPathListElement->value.pathProgressCounter = 0;
        }
        unsigned int currentPathProgressCounter  = pCurrentPathListElement->value.pathProgressCounter;
        unsigned int currentPathSize = pCurrentPathListElement->value.pathSize;

        if(newCharacter=='\0'){
            if(currentPathProgressCounter==currentPathSize || 
                (currentPathProgressCounter==(currentPathSize-1) && pCurrentPathListElement->value.path[currentPathProgressCounter]=='^'))
            {
                // This path is valid, we have indeed arrived at the end of the path
                if(pLastValidPathListElement==nullptr){
                    pNewHead = pCurrentPathListElement;
                }
                else{
                    pLastValidPathListElement->next[1] = pCurrentPathListElement;
                }

                pLastValidPathListElement = pCurrentPathListElement;
                if(isFirstCharacter){
                    pCurrentPathListElement = pCurrentPathListElement->next[0];
                }
                else{
                    pCurrentPathListElement = pCurrentPathListElement->next[1];
                }
                pLastValidPathListElement->next[1] = nullptr;

                continue;
            }
            else{
                // This path is not valid
                if(isFirstCharacter){
                    pCurrentPathListElement = pCurrentPathListElement->next[0];
                }
                else{
                    pCurrentPathListElement = pCurrentPathListElement->next[1];
                }

                continue;
            }
        }

        if(currentPathProgressCounter>=currentPathSize){
            // This path doesn't have any more characters
            if(isFirstCharacter){
                pCurrentPathListElement = pCurrentPathListElement->next[0];
            }
            else{
                pCurrentPathListElement = pCurrentPathListElement->next[1];
            }

            continue;
        }

        if(pCurrentPathListElement->value.path[currentPathProgressCounter]=='^' && newCharacter!='/'){
            // This path is valid and we don't yet need to move to the next character in the path
            if(pLastValidPathListElement==nullptr){
                pNewHead = pCurrentPathListElement;
            }
            else{
                pLastValidPathListElement->next[1] = pCurrentPathListElement;
            }

            pLastValidPathListElement = pCurrentPathListElement;
            if(isFirstCharacter){
                pCurrentPathListElement = pCurrentPathListElement->next[0];
            }
            else{
                pCurrentPathListElement = pCurrentPathListElement->next[1];
            }
            pLastValidPathListElement->next[1] = nullptr;
            
            continue;
        }
        else if(pCurrentPathListElement->value.path[currentPathProgressCounter]=='^' && newCharacter=='/'){
            // We need to move to the next character in the path
            currentPathProgressCounter++;
            if(currentPathProgressCounter>=currentPathSize){
                // This path doesn't have any more characters
                if(isFirstCharacter){
                    pCurrentPathListElement = pCurrentPathListElement->next[0];
                }
                else{
                    pCurrentPathListElement = pCurrentPathListElement->next[1];
                }

                continue;
            }
        }

        if(pCurrentPathListElement->value.path[currentPathProgressCounter] != newCharacter){
            // newCharacter doesn't agree with this path
            if(isFirstCharacter){
                pCurrentPathListElement = pCurrentPathListElement->next[0];
            }
            else{
                pCurrentPathListElement = pCurrentPathListElement->next[1];
            }

            continue;
        }
        else{
            // newCharacter agrees with this path
            if(pLastValidPathListElement==nullptr){
                pNewHead = pCurrentPathListElement;
            }
            else{
                pLastValidPathListElement->next[1] = pCurrentPathListElement;
            }
            
            pCurrentPathListElement->value.pathProgressCounter = currentPathProgressCounter + 1;

            pLastValidPathListElement = pCurrentPathListElement;
            if(isFirstCharacter){
                pCurrentPathListElement = pCurrentPathListElement->next[0];
            }
            else{
                pCurrentPathListElement = pCurrentPathListElement->next[1];
            }
            pLastValidPathListElement->next[1] = nullptr;

            continue;
        }
    }

    return pNewHead;
}