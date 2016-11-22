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

__attribute__((section("AHBSRAM0"))) static IP myip;
__attribute__((section("AHBSRAM0"))) static int mysocket;
__attribute__((section("AHBSRAM0"))) static MDMSerial *pMdm = NULL;


CellInterface::CellInterface(){
	printf("Cell-Contractor \r\n");
}
CellInterface::~CellInterface(){
	printf("Cell-Decontractor \r\n");
}

int CellInterface::connect(
		const char *apn       /* = 0 */,
		const char *username  /* = 0 */,
		const char *password  /* = 0 */)
{
	printf("Cell-connect \r\n");
	pMdm = new MDMSerial();

	if (pMdm != NULL) {
		pMdm->setDebug(4);
		return (int) pMdm->connect(NULL, apn, username, password);
	}
    return (int)false;
}

int CellInterface::disconnect()
{
	printf("Cell-disconnect \r\n");
	if (pMdm != NULL) {
	   return (int)pMdm->disconnect();
	}
    return (int)false;
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
	    	printf("---> get_ip_address \r\n");
	    	return (const char *)pMdm->getIpAddress();
	    }
	    virtual int gethostbyname(SocketAddress *address, const char *host)
	    {
	    	printf("---> gethostbyname :%s\r\n", host);
	    	//IP ip = pMdm->gethostbyname(host);
	    	//printf("%lu \r\n", (unsigned long)ip);


	    	nsapi_addr_t my;
//	    	uint8_t d[4];
//	    	inttolitend(ip,d); 169.45.82.18 nsapi_version_t
	    	my.bytes[0]=169;
	    	my.bytes[1]=45;
	    	my.bytes[2]=82;
	    	my.bytes[3]=18;
	    	my.version = NSAPI_IPv4;


	        address->set_addr( my );
	        return 0;
	    }
	    virtual int setstackopt(int level, int optname, const void *optval, unsigned optlen)
	    {
	    	printf("---> setstackopt \r\n");
	    	return (int)true;
	    }
	    virtual int getstackopt(int level, int optname, void *optval, unsigned *optlen)
	    {
	    	printf("---> getstackopt \r\n");
	    	return (int)true;
	    }

	protected:
	    friend class Socket;
	    friend class UDPSocket;

	    virtual int socket_open(nsapi_socket_t *handle, nsapi_protocol_t proto)
	    {
	    	printf("---> socket_open \r\n");

	    	mysocket = pMdm->socketSocket(MDMParser::IPPROTO_UDP);

	    	pMdm->socketSetBlocking(mysocket, 10000);

	    	myip = pMdm->gethostbyname("api.connector.mbed.com");

	    	bool result = pMdm->socketConnect( (int)mysocket, "api.connector.mbed.com", 5684);
	    	printf("result:%d\r\n", (int)result);

	    	return (int)0;
	    }

	    virtual int socket_close(nsapi_socket_t handle)
	    {
	    	printf("---> socket_close \r\n");
	    	pMdm->socketClose( (int)mysocket );
	    	return (int)0;
	    }

	    virtual int socket_bind(nsapi_socket_t handle, const SocketAddress &address)
	    {
	    	printf("---> socket_bind \r\n");
	    	return (int)0;
	    }

	    virtual int socket_listen(nsapi_socket_t handle, int backlog)
	    {
	    	printf("---> socket_listen \r\n");
	    	return (int)true;
	    }
	    virtual int socket_connect(nsapi_socket_t handle, const SocketAddress &address)
	    {
	    	printf("---> socket_connect \r\n");
	    	pMdm->socketConnect( (int) mysocket, (const char*) "coap://api.connector.mbed.com", 5684);
	    	return (int)true;
	    }
	    virtual int socket_accept(nsapi_socket_t *handle, nsapi_socket_t server)
	    {
	    	printf("---> socket_accept \r\n");
	    	return (int)true;
	    }
	    virtual int socket_send(nsapi_socket_t handle, const void *data, unsigned size)
	    {
	    	printf("---> socket_send \r\n");
	    	return (int) pMdm->socketSend( (int) mysocket, (const char *) data, (int) size);
	    }
	    virtual int socket_recv(nsapi_socket_t handle, void *data, unsigned size)
	    {
	    	printf("---> socket_recv \r\n");
	    	return (int) pMdm->socketRecv( (int) mysocket, (char*) data, (int) size);
	    }
	    virtual int socket_sendto(nsapi_socket_t handle, const SocketAddress &address, const void *data, unsigned size)
	    {
	    	printf("---> socket_sendto \r\n");

	    	int retval=0;
	    	retval = (pMdm->socketSendTo( (int)mysocket, (IP)myip, (int)5684 ,(const char *)data, (int)size ));
	    	printf("--->  %d bytes sent\r\n", retval);
	    	return (int)retval;
	    }
	    virtual int socket_recvfrom(nsapi_socket_t handle, SocketAddress *address, void *buffer, unsigned size)
	    {
	    	printf("---> socket_recvfrom \r\n");

	    	int* pPort;
	    	*pPort=5684;
	    	return (int)  pMdm->socketRecvFrom( (int)mysocket, (IP*)myip, (int* )pPort, (char*)buffer, (int)size);
	    }
	    virtual void socket_attach(nsapi_socket_t handle, void (*callback)(void *), void *data)
	    {
	    	printf("---> socket_attach \r\n");
	    }

	    virtual int setsockopt(nsapi_socket_t handle, int level, int optname, const void *optval, unsigned optlen)
	    {
	    	printf("---> setsockopt \r\n");
	    	return (int)0;
	    }
	    virtual int getsockopt(nsapi_socket_t handle, int level, int optname, void *optval, unsigned *optlen)
	    {
	    	printf("---> getsockopt \r\n");
	    	return (int)0;
	    }
};

//---------------------------------------------------------------------------------------------------------------------------------------------------

NetworkStack *CellInterface::get_stack()
{
	printf("Cell-get-stack !!! \r\n");

	static CellNet *cellnet = NULL;
	if(!cellnet){
		cellnet = new CellNet;
	}
	return (NetworkStack *)cellnet;
}
