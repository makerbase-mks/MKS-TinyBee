#include "serial2socket.h"
#include "tcp_server.h"

TCP_Server tcp_server;

TCP_Server::TCP_Server()
{
	_port = TCP_SOCKET_PORT;
}
TCP_Server::~TCP_Server()
{
    end();
}

void TCP_Server::begin()
{
	_server.begin(_port);
	Serial2Socket.attachTS(this);
}

void TCP_Server::end()
{
	int i;
	if (_server.hasClient())
	{
		for(i = 0; i < MAX_SRV_CLIENTS; i++)
		{
			//find free/disconnected spot		
			if(_clients[i].connected()) 
			{
				_clients[i].stop();
			}
			_clients[i] = _server.available();
		}
		if (_server.hasClient())
		{
			//no free/disconnected spot so reject
			WiFiClient serverClient = _server.available();
			serverClient.stop();

		}
	}
}

void TCP_Server::handle()
{
	int i;
	uint8_t dbgStr[100] ;
	
	if (_server.hasClient())
	{
		for(i = 0; i < MAX_SRV_CLIENTS; i++)
		{
			//find free/disconnected spot		
			if(_clients[i].connected()) 
			{
				_clients[i].stop();
			}
			_clients[i] = _server.available();
		}
		if (_server.hasClient())
		{
			//no free/disconnected spot so reject
			WiFiClient serverClient = _server.available();
			serverClient.stop();

		}
	}
	memset(dbgStr, 0, sizeof(dbgStr));
	for(i = 0; i < MAX_SRV_CLIENTS; i++)
	{
		if (_clients[i] && _clients[i].connected())
		{
			uint32_t readNum = _clients[i].available();

				
			if(readNum > 0)
			{
				char * point;
				
			  	uint8_t readStr[readNum + 1] ;

				uint32_t readSize;
				
				readSize = _clients[i].read(readStr, readNum);
					
				readStr[readSize] = 0;	
				
				Serial2Socket.push((const char *)readStr);

				//tcp_print((const uint8_t *)"rcv:", strlen((const char*)"rcv:"));
				//tcp_print((const uint8_t *)readStr, strlen((const char*)readStr));				
				
			
			}
			Serial2Socket.handle_flush();
		}
	}

}




void TCP_Server::tcp_print(const uint8_t *sbuf, uint32_t len)
{
	int i;
	
	for(i = 0; i < MAX_SRV_CLIENTS; i++){
			
	  if (_clients[i] && _clients[i].connected()){
		_clients[i].write(sbuf, len);
		delay(1);
		
	  }
	}
}
