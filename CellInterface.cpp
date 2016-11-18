/* CellularInterface
 * Copyright (c) 2015 ARM Limited
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "CellInterface.h"
#include "MDM.h"
#include "lwip_stack.h"

#include "network-socket/NetworkInterface.h"

typedef uint32_t IP;
//typedef enum { IPPROTO_TCP, IPPROTO_UDP } IpProtocol;

static MDMSerial *pMdm = NULL;
//static MDMParser *pMdm = NULL;


CellInterface::CellInterface(){
	printf("Cell-Contractor \r\n");
}

CellInterface::~CellInterface(){
	printf("Cell-Decontractor \r\n");
}

int CellInterface::connect(const char *apn /* = 0 */,
		const char *username /* = 0 */,
		const char *password /* = 0 */)
{
	printf("Cell-connect \r\n");
	pMdm = new MDMSerial();

	if (pMdm != NULL) {
		pMdm->setDebug(4);
		if (!pMdm->connect(NULL, apn, username, password) ){
	    	return 0;
	    }
	}
    return 1;
}

int CellInterface::disconnect()
{
	printf("Cell-disconnect \r\n");
	if (pMdm != NULL) {
	   pMdm->disconnect();
	}
    return 0;
}

const char *CellInterface::get_ip_address()
{
	printf("Cell-get-ip-address \r\n");
	char textToWrite[ 16 ];
	uint32_t ip;
	if (pMdm != NULL) {
	  ip = pMdm->getIpAddress();
	}
	sprintf(textToWrite,"%lu", ip);
    return textToWrite;
}

const char *CellInterface::get_mac_address()
{
	printf("Cell-get-mac-address \r\n");
	char *ret = "mac";
    return ret;
}

//---------------------------------------------------------------------------------------------------------------------------------------------------
class CellNet : public NetworkStack{
	public:
	    virtual const char *get_ip_address(void)
	    {
	    	printf("-->get_ip_address \r\n");
	    	return (const char *)pMdm->getIpAddress();
	    }


	    virtual int gethostbyname(SocketAddress *address, const char *host)
	    {
	    	printf("-->gethostbyname \r\n");
	    	(int) pMdm->gethostbyname(host);
	    	return 1;
	    }


	    virtual int setstackopt(int level, int optname, const void *optval, unsigned optlen)
	    {
	    	printf("-->setstackopt \r\n");
	    	return (int) 0;
	    }


	    virtual int getstackopt(int level, int optname, void *optval, unsigned *optlen)
	    {
	    	printf("-->getstackopt \r\n");
	    	return (int) 0;
	    }





	protected:
	    friend class Socket;
	    friend class UDPSocket;
	    friend class TCPSocket;
	    friend class TCPServer;

	    virtual int socket_open(nsapi_socket_t *handle, nsapi_protocol_t proto)
	    {
	    	printf("-->socket_open \r\n");

	    	return (int) pMdm->socketSocket(MDMParser::IPPROTO_UDP);
	    	//IpProtocol ipproto = IPPROTO_UDP;
	    	//return (int) pMdm->socketSocket( ipproto , 5684);
	    	//return 0;
	    }

	    virtual int socket_close(nsapi_socket_t handle)
	    {
	    	printf("-->socket_close \r\n");
	    	return (int) pMdm->socketClose( (int)handle );
	    }

	    virtual int socket_bind(nsapi_socket_t handle, const SocketAddress &address)
	    {
	    	printf("-->socket_bind \r\n");
	    	int socket = pMdm->socketSocket(MDMParser::IPPROTO_UDP);
	    	pMdm->socketSetBlocking(socket, 3600);
	    	pMdm->socketConnect(socket, "api.connector.mbed.com", 5684);
	    	return 1;
	    	//return (int)  pMdm->socketSetBlocking( (int) handle, 3600);
	    }

	    virtual int socket_listen(nsapi_socket_t handle, int backlog)
	    {
	    	printf("-->socket_listen \r\n");
	    	return (int) 0;
	    }

	    virtual int socket_connect(nsapi_socket_t handle, const SocketAddress &address)
	    {
	    	printf("-->socket_connect \r\n");
	    	return (int)  pMdm->socketConnect( (int) handle, (const char*) "coap://api.connector.mbed.com", 5684);
	    }

	    virtual int socket_accept(nsapi_socket_t *handle, nsapi_socket_t server)
	    {
	    	printf("-->socket_accept \r\n");
	    	return (int)  0;
	    }


	    virtual int socket_send(nsapi_socket_t handle, const void *data, unsigned size)
	    {
	    	printf("-->socket_send \r\n");

	    	int socket = pMdm->socketSocket(MDMParser::IPPROTO_UDP);
	    	//MDMParser::IP ip = mdm.gethostbyname("api.connector.mbed.com");
	    	return (int) pMdm->socketSend( (int) socket, (const char *) data, (int) size);
	    }


	    virtual int socket_recv(nsapi_socket_t handle, void *data, unsigned size)
	    {
	    	printf("-->socket_recv \r\n");
	    	return (int) pMdm->socketRecv( (int) handle, (char*) data, (int) size);
	    }


	    virtual int socket_sendto(nsapi_socket_t handle, const SocketAddress &address, const void *data, unsigned size)
	    {
	    	printf("-->socket_sendto \r\n");

	    	int socket = pMdm->socketSocket(MDMParser::IPPROTO_UDP);
	    	MDMParser::IP ip = pMdm->gethostbyname("api.connector.mbed.com");
	    	return (int)pMdm->socketSendTo( (int)socket, (IP)ip,  5684,  (const char *)data, (int)size );
	    }



	    virtual int socket_recvfrom(nsapi_socket_t handle, SocketAddress *address, void *buffer, unsigned size)
	    {
	    	printf("-->socket_recvfrom \r\n");
	    	return (int)  pMdm->socketRecvFrom( (int)handle, (IP*)address, NULL, (char*)buffer, (int)size);
	    }



	    virtual void socket_attach(nsapi_socket_t handle, void (*callback)(void *), void *data)
	    {
	    	printf("-->socket_attach \r\n");

	    }

	    virtual int setsockopt(nsapi_socket_t handle, int level, int optname, const void *optval, unsigned optlen)
	    {
	    	printf("-->setsockopt \r\n");
	    	return (int) 0;
	    }

	    virtual int getsockopt(nsapi_socket_t handle, int level, int optname, void *optval, unsigned *optlen)
	    {
	    	printf("-->getsockopt \r\n");
	    	return (int)  0;
	    }
};
//---------------------------------------------------------------------------------------------------------------------------------------------------

NetworkStack *CellInterface::get_stack()
{
	printf("Cell-get-stack !!! \r\n");

	// MDMSerial *pMdm
	//return nsapi_create_stack(&lwip_stack);
	//return nsapi_create_stack(   (nsapi_stack_t *)pMdm  );

	static CellNet *cellnet = NULL;

	if(!cellnet){
		cellnet = new CellNet;
	}
	return cellnet;
}
