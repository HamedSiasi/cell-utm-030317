#ifndef MDMTCPSOCKET_H
#define MDMTCPSOCKET_H

#include "mdmSocket.h"

/** UDP socket connection
 */
class mdmUDPSocketConnection: public mdmSocket
{
    //friend class mdmTCPSocketServer;

public:
    /** UDP socket connection
    */
    mdmUDPSocketConnection() {}

    /** Connects this TCP socket to the server
    \param host The host to connect to. It can either be an IP Address or a hostname that will be resolved with DNS.
    \param port The host's port to connect to.
    \return 0 on success, -1 on failure.
    */
    int connect(const char* host, const int port)
    {
        if (_mdm == NULL)
            _mdm = MDMParser::getInstance();
        if (_mdm == NULL)
            return -1;
            
        if (_socket < 0) {
            _socket = _mdm->socketSocket(MDMParser::IPPROTO_UDP);
            if (_socket < 0) {
                return -1;
            }
        }
    
        _mdm->socketSetBlocking(_socket, _timeout_ms); 
        if (!_mdm->socketConnect(_socket, host, port)) {
            return -1;
        }
        return 0;
    }
    /** Check if the socket is connected
    \return true if connected, false otherwise.
    */
    bool is_connected(void)                 { return _mdm->socketIsConnected(_socket); }

    /** Send data to the remote host.
    \param data The buffer to send to the host.
    \param length The length of the buffer to send.
    \return the number of written bytes on success (>=0) or -1 on failure
     */
    int send(char* data, int length)        { return _mdm->socketSend(_socket, data, length); }

    /** Send all the data to the remote host.
    \param data The buffer to send to the host.
    \param length The length of the buffer to send.
    \return the number of written bytes on success (>=0) or -1 on failure
    */
    int send_all(char* data, int length)    { return send(data,length); }

    /** Receive data from the remote host.
    \param data The buffer in which to store the data received from the host.
    \param length The maximum length of the buffer.
    \return the number of received bytes on success (>=0) or -1 on failure
     */
    int receive(char* data, int length)     { return _mdm->socketRecv(_socket, data, length); }

    /** Receive all the data from the remote host.
    \param data The buffer in which to store the data received from the host.
    \param length The maximum length of the buffer.
    \return the number of received bytes on success (>=0) or -1 on failure
    */
    int receive_all(char* data, int length) { return receive(data,length); }
    
};

#endif
