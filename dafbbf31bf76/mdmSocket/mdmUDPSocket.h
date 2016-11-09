/* Copyright (C) 2012 mbed.org, MIT License
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software
 * and associated documentation files (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge, publish, distribute,
 * sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
 * BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef MDMUDPSOCKET_H
#define MDMUDPSOCKET_H

#include "mdmSocket/mdmSocket.h"
#include "mdmSocket/mdmEndpoint.h"

/**
UDP Socket
*/
class mdmUDPSocket : public mdmSocket {

public:
    int init(void) 
    {
        if (_mdm == NULL)
            _mdm = MDMParser::getInstance();
        if (_mdm == NULL)
            return -1;
        return 0;
    }
    
    int bind(int port) {
        if (_socket < 0) {
            _socket = _mdm->socketSocket(MDMParser::IPPROTO_UDP, port);
            if (_socket < 0) {
                return -1;
            }
        }
        _mdm->socketSetBlocking(_socket, _timeout_ms); 
        return 0;
    }
    
    int join_multicast_group(const char* address)   { return -1; }
    
    int set_broadcasting(bool broadcast=true)       { return -1; }
    
    int sendTo(mdmEndpoint &remote, char *packet, int length)
    {
        char* str = remote.get_address();
        int port = remote.get_port();
        MDMParser::IP ip = _mdm->gethostbyname(str);
        if (ip == NOIP)
            return -1;
        return _mdm->socketSendTo(_socket, ip, port, packet, length); 
    }
    
    int receiveFrom(mdmEndpoint &remote, char *buffer, int length)
    { 
        MDMParser::IP ip;
        int port;
        int ret = _mdm->socketRecvFrom(_socket, &ip, &port, buffer, length); 
        if (ret >= 0) {
            char str[17];
            sprintf(str, IPSTR, IPNUM(ip));
            remote.set_address(str, port);
        }
        return ret;
    }
};

#endif
