#include "../../cpp_lib/coap_server.h"
#include "../../cpp_lib/mem.h"
#include "../../cpp_lib/string.h"
#include "../../cpp_lib/placement_new.h"
#include "../../cpp_lib/syscalls.h"

#define COAP_PORT 6000
#define COAP_SERVER_ADDR 0x2400000

class HelloWorldHandler : public CoAPHandler{
    public:
        CoAPResponse handleGET(char* path,
            ContentFormat contentFormat,
            unsigned char* payload, 
            unsigned int payloadSize, 
            unsigned char responseBuffer[RESPONSE_BUFFER_SIZE]) override
        {
            print((char*)"GET request received\n");
            
            char* responseString = (char*)"Hello World!";
            CoAPResponse response;
            response.responseCode = CONTENT_RESPONSE_CODE;
            response.responseSize = strlen(responseString);
            memCopy((unsigned char*)responseString, responseBuffer, response.responseSize);
            response.contentFormat = ContentFormat::Text_Plain_Charset_UTF8;
            return response;
        }
};

void main(){
    CoAPServer* pServer = new((unsigned char*)COAP_SERVER_ADDR) CoAPServer(COAP_PORT);

    HelloWorldHandler helloWorldHandler;
    pServer->setPathHandler((char*)"/", &helloWorldHandler);
    
    print((char*)"Starting CoAP server...\n");
    pServer->run();
}