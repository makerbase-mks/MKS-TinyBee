#ifndef _TCP_SERVER_H_
#define _TCP_SERVER_H_
#include "WiFiServer.h"
#include "WiFiClient.h"
#include "esp3dlibconfig.h"

#define MAX_SRV_CLIENTS 	1


class TCP_Server
{
public:
    TCP_Server();
    ~TCP_Server();
    void begin();
    void end();
    void handle();
    void tcp_print(const uint8_t *sbuf, uint32_t len);
    
private:
    WiFiServer _server;
    int _port;
    WiFiClient _clients[MAX_SRV_CLIENTS];
};

extern TCP_Server tcp_server;

#endif
