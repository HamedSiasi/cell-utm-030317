#ifndef MDMSOCKET_H_
#define MDMSOCKET_H_

#include "MDM.h"

/** Socket file descriptor and select wrapper
  */
class mdmSocket {
public:
	mdmSocket() {
        _socket  = -1;
        _timeout_ms = MDMParser::TIMEOUT_BLOCKING;
        _mdm     = NULL;
    }
    
    void set_blocking(bool blocking, unsigned int timeout = 1500) {
        _timeout_ms = blocking ? MDMParser::TIMEOUT_BLOCKING : (int)timeout;
        if (_socket >= 0) {
            _mdm->socketSetBlocking(_socket, _timeout_ms); 
        }
    }
    
    int close() {
        bool ret = false;
        if (_socket >= 0)
        {
            ret = _mdm->socketClose(_socket);
            _mdm->socketFree(_socket);
            _socket = -1;
            _timeout_ms = MDMParser::TIMEOUT_BLOCKING;
        }
        return ret ? 0 : -1;
    }
    
    ~mdmSocket() { close(); }
    
protected:
    int _socket;
    int _timeout_ms;
    MDMParser* _mdm;
};


#endif /* SOCKET_H_ */
