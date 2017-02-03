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
#include "network-socket/NetworkInterface.h"
//#include <string>
//#include <cstdio>
//#include <iostream>

__attribute__((section("AHBSRAM0"))) static MDMParser::IP myip;
static int mysocket = -1;
__attribute__((section("AHBSRAM0"))) static MDMSerial *pMdm = NULL;


//ZELITRON Server VIP (Load balancer )
#define HOST "192.168.5.80"
#define PORT 9005

//ZELITRON
//#define HOST "195.46.10.19"
//#define PORT 9005

//#define HOST "ciot.it-sgn.u-blox.com"
//#define PORT 5683

//#define HOST "api.connector.mbed.com"
//#define PORT 5684

//#define HOST "echo.u-blox.com"
//#define PORT 7

//#define HOST "coap.me"
//#define PORT 5683



#define SIMPIN      NULL                //  NULL
#define APN         "m2m.zelitron.com"  //  "giffgaff.com"
#define USERNAME    "web"               //  "giffgaff"
#define PASSWORD    "web"               //  NULL



int CellInterface::connect(
		const char *apn          /* = 0 */,
		const char *username     /* = 0 */,
		const char *password     /* = 0 */)
{
	printf("Cell-connect \r\n");
	pMdm = new MDMSerial();        // use mdm(D1,D0) if you connect the cellular shield to a C027
	pMdm->setDebug(4);             // enable this for debugging issues

	// initialize the modem
	MDMParser::DevStatus devStatus = {};
	MDMParser::NetStatus netStatus = {};
	bool mdmOk = pMdm->init(SIMPIN, &devStatus);
	pMdm->dumpDevStatus(&devStatus);
	if (mdmOk) {
	        // wait until we are connected
	        mdmOk = pMdm->registerNet(&netStatus);
	        pMdm->dumpNetStatus(&netStatus);
	}
	if (mdmOk)
	{
	        // join the internet connection
	        MDMParser::IP ip = pMdm->join(APN,USERNAME,PASSWORD);
	        if (ip == NOIP){
	            printf("Not able to join network \r\n");
	        }
	        else
	        {
	        	pMdm->dumpIp(ip);
	            myip = pMdm->gethostbyname(HOST);
	        }
	}
//	pMdm = new MDMSerial();
//
//	if (pMdm != NULL) {
//		pMdm->setDebug(4);
//		return (int) pMdm->connect(NULL, apn, username, password);
//	}
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
	    friend class mdmSocket;
	    friend class mdmUDPSocket;

	    virtual int socket_open(nsapi_socket_t *handle, nsapi_protocol_t proto)
	    {
	    	printf("---> socket_open handle:(%d) protocol:(%d) \r\n", (int)handle, (int)proto);
//
//	    	static bool status = true;
//	    	if(status){
//	    		mysocket = pMdm->socketSocket(MDMParser::IPPROTO_UDP, PORT);
//	    		pMdm->socketSetBlocking(mysocket, 10000);
//	    		status=false;
//	    	}
	    	return (int)0;
	    }

	    virtual int socket_close(nsapi_socket_t handle)
	    {
	    	printf("---> socket_close handle:(%d)\r\n", (int)handle);
	    	pMdm->socketClose( (int)mysocket );
	    	return (int)0;
	    }

	    virtual int socket_bind(nsapi_socket_t handle, const SocketAddress &address)
	    {
	    	printf("---> socket_bind handle:(%d)\r\n");

	        if (mysocket < 0) {
	            mysocket = pMdm->socketSocket(MDMParser::IPPROTO_UDP, PORT);
	            if (mysocket < 0) {
	                return -1;
	            }
	        }
	        pMdm->socketSetBlocking(mysocket, 10000);
	        return 0;
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
	    	int ret;
	    	printf("\n ---> socket_sendto handle:(%d) size:(%d)\r\n", (int)handle, (int)size);

	    	//ret = (int) (pMdm->socketSendTo( (int)mysocket, myip, PORT ,  static_cast<const char*>(data)   , (int)size ));
	    	ret = (int) (pMdm->socketSendTo( (int)mysocket, myip, PORT ,  reinterpret_cast<const char*>(data)   , (int)size ));


	    	//ret = (int) (pMdm->socketSendTo( (int)mysocket, myip, PORT ,(char *)data, (int)size ));
	    	pMdm->socketClose( (int)mysocket );
	    	return ret;
	    }

	    virtual int socket_recvfrom(nsapi_socket_t handle, SocketAddress *address, void *buffer, unsigned size)
	    {
	    	printf("---> socket_recvfrom handle:(%d) size:(%d) \r\n", (int)handle, (int)size);

	    	MDMParser::IP ip;
	    	int port;
	    	return pMdm->socketRecvFrom( mysocket, &ip, &port, (char*)buffer, (int)size );
	    }




	    virtual void socket_attach(nsapi_socket_t handle, void (*callback)(void *), void *data)
	    {
	    	printf("---> socket_attach handle:(%d)\r\n", (int)handle);
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

static CellNet *cellnet = NULL;
NetworkStack *CellInterface::get_stack()
{
	printf("Cell-get-stack !!! \r\n");
	if(!cellnet){
		cellnet = new CellNet;
	}
	return (NetworkStack *)cellnet;
}
