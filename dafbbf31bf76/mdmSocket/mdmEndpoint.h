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
#ifndef MDMENDPOINT_H
#define MDMENDPOINT_H

#include "MDM.h"

class mdmUDPSocket;

class mdmEndpoint {
    friend class mdmUDPSocket;
public:
    mdmEndpoint(void)  {
        _ip[0] = '\0'; 
        _port = 0; 
        _mdm = NULL; 
    }
    
    void reset_address(void) { 
        _ip[0] = '\0'; 
        _port = 0; 
    }
    
    int  set_address(const char* host, const int port) {
        _ip[0] = '\0';
        _port = 0;
        if (_mdm == NULL)
            _mdm = MDMParser::getInstance();
        if (_mdm == NULL)
            return -1;
        // resove the host name (eventually does a dns lookup)
        MDMParser::IP ip = _mdm->gethostbyname(host);
        if (ip == NOIP)
            return -1;
        sprintf(_ip, IPSTR, IPNUM(ip));
        _port = port;
        return 0;
    }
    
    char* get_address(void)     {   return _ip; }
    
    int get_port(void)          {   return _port; }
    
protected:
    MDMParser* _mdm;
    char _ip[17];
    int _port;
};

#endif
