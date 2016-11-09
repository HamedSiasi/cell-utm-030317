#pragma once 

#include "mbed.h"
#include "Pipe.h"
#include "SerialPipe.h"

#ifdef TARGET_UBLOX_C027
 #define GPS_IF(onboard, shield) onboard
#else
 #define GPS_IF(onboard, shield) shield
#endif

/** basic gps parser class 
*/
class GPSParser
{
public:
    /** Power on / Wake up the gps 
    */
    virtual bool init(PinName pn) = 0;
    
    enum { 
        // getLine Responses
        WAIT      = -1, //!< wait for more incoming data (the start of a message was found, or no data available)
        NOT_FOUND =  0, //!< a parser concluded the the current offset of the pipe doe not contain a valid message
    
        #define LENGTH(x)   (x & 0x00FFFF)  //!< extract/mask the length
        #define PROTOCOL(x) (x & 0xFF0000)  //!< extract/mask the type

        UNKNOWN   = 0x000000,       //!< message type is unknown 
        UBX       = 0x100000,       //!< message if of protocol NMEA
        NMEA      = 0x200000        //!< message if of protocol UBX
    };
    
    /** Get a line from the physical interface. This fucntion 
        needs to be implemented in the inherited class.
        \param buf the buffer to store it
        \param len size of the buffer
        \return type and length if something was found, 
                WAIT if not enough data is available
                NOT_FOUND if nothing was found
    */ 
    virtual int getMessage(char* buf, int len) = 0;
    
    /** send a buffer
        \param buf the buffer to write
        \param len size of the buffer to write
        \return bytes written
    */
    virtual int send(const char* buf, int len);
    
    /** send a NMEA message, this function just takes the 
        payload and calculates and adds checksum. ($ and *XX\r\n will be added)
        \param buf the message payload to write
        \param len size of the message payload to write
        \return total bytes written
    */
    virtual int sendNmea(const char* buf, int len);
    
    /** send a UBX message, this function just takes the 
        payload and calculates and adds checksum.
        \param cls the UBX class id 
        \param id the UBX message id
        \param buf the message payload to write
        \param len size of the message payload to write
        \return total bytes written
    */
    virtual int sendUbx(unsigned char cls, unsigned char id, 
                        const void* buf = NULL, int len = 0);
    
    /** Power off the gps, it can be again woken up by an 
        edge on the serial port on the external interrupt pin. 
    */
    void powerOff(void);
    
    /** get the first character of a NMEA field
        \param ix the index of the field to find
        \param start the start of the buffer
        \param end the end of the buffer
        \return the pointer to the first character of the field. 
    */
    static const char* findNmeaItemPos(int ix, const char* start, const char* end);
    
    /** extract a double value from a buffer containing a NMEA message
        \param ix the index of the field to extract
        \param buf the NMEA message
        \param len the size of the NMEA message
        \param val the extracted value
        \return true if successful, false otherwise
    */
    static bool getNmeaItem(int ix, char* buf, int len, double& val);
    
    /** extract a interger value from a buffer containing a NMEA message
        \param ix the index of the field to extract
        \param buf the NMEA message
        \param len the size of the NMEA message
        \param val the extracted value
        \param base the numeric base to be used (e.g. 8, 10 or 16)
        \return true if successful, false otherwise
    */
    static bool getNmeaItem(int ix, char* buf, int len, int& val, int base/*=10*/);
    
    /** extract a char value from a buffer containing a NMEA message
        \param ix the index of the field to extract
        \param buf the NMEA message
        \param len the size of the NMEA message
        \param val the extracted value
        \return true if successful, false otherwise
    */
    static bool getNmeaItem(int ix, char* buf, int len, char& val);
    
    /** extract a latitude/longitude value from a buffer containing a NMEA message
        \param ix the index of the field to extract (will extract ix and ix + 1)
        \param buf the NMEA message
        \param len the size of the NMEA message
        \param val the extracted latitude or longitude
        \return true if successful, false otherwise
    */
    static bool getNmeaAngle(int ix, char* buf, int len, double& val);
    
protected:
    /** Get a line from the physical interface. 
        \param pipe the receiveing pipe to parse messages 
        \param buf the buffer to store it
        \param len size of the buffer
        \return type and length if something was found, 
                WAIT if not enough data is available
                NOT_FOUND if nothing was found
    */ 
    static int _getMessage(Pipe<char>* pipe, char* buf, int len);
    
    /** Check if the current offset of the pipe contains a NMEA message.
        \param pipe the receiveing pipe to parse messages 
        \param len numer of bytes to parse at maximum
        \return length if something was found (including the NMEA frame) 
                WAIT if not enough data is available
                NOT_FOUND if nothing was found
    */ 
    static int _parseNmea(Pipe<char>* pipe, int len);
    
    /** Check if the current offset of the pipe contains a UBX message.
        \param pipe the receiveing pipe to parse messages 
        \param len numer of bytes to parse at maximum
        \return length if something was found (including the UBX frame)
                WAIT if not enough data is available
                NOT_FOUND if nothing was found
    */ 
    static int _parseUbx(Pipe<char>* pipe, int len);
    
    /** Write bytes to the physical interface. This function 
        needs to be implemented by the inherited class. 
        \param buf the buffer to write
        \param len size of the buffer to write
        \return bytes written
    */
    virtual int _send(const void* buf, int len) = 0;
    
    static const char toHex[16]; //!< num to hex conversion
#ifdef TARGET_UBLOX_C027
    bool _onboard;
#endif
};

/** gps class which uses a serial port 
    as physical interface. 
*/
class GPSSerial : public SerialPipe, public GPSParser
{
public:
    /** Constructor
        \param tx is the serial ports transmit pin (gps to CPU)
        \param rx is the serial ports receive pin (CPU to gps)
        \param baudrate the baudrate of the gps use 9600
        \param rxSize the size of the serial rx buffer
        \param txSize the size of the serial tx buffer
    */
    GPSSerial(PinName tx    GPS_IF( = GPSTXD, /* = D8 */), // resistor on shield not populated 
              PinName rx    GPS_IF( = GPSRXD, /* = D9 */), // resistor on shield not populated 
              int baudrate  GPS_IF( = GPSBAUD, = 9600 ),
              int rxSize    = 256 , 
              int txSize    = 128 );
              
    //! Destructor
    virtual ~GPSSerial(void);
    
    virtual bool init(PinName pn = NC);
    
    /** Get a line from the physical interface. 
        \param buf the buffer to store it
        \param len size of the buffer
        \return type and length if something was found, 
                WAIT if not enough data is available
                NOT_FOUND if nothing was found
    */ 
    virtual int getMessage(char* buf, int len);
    
protected:
    /** Write bytes to the physical interface.
        \param buf the buffer to write
        \param len size of the buffer to write
        \return bytes written
    */
    virtual int _send(const void* buf, int len);
};

/** gps class which uses a i2c as physical interface. 
*/
class GPSI2C : public I2C, public GPSParser
{
public: 
    /** Constructor
        \param sda is the I2C SDA pin (between CPU and GPS)
        \param scl is the I2C SCL pin (CPU to GPS) 
        \param adr the I2C address of the GPS set to (66<<1)
        \param rxSize the size of the serial rx buffer
    */
    GPSI2C(PinName sda          GPS_IF( = GPSSDA, = D14 ), 
           PinName scl          GPS_IF( = GPSSCL, = D15 ),
           unsigned char i2cAdr GPS_IF( = GPSADR, = (66<<1) ), 
           int rxSize           = 256 );
    //! Destructor
    virtual ~GPSI2C(void);
    
    /** helper function to probe the i2c device
        \return true if successfully detected the gps. 
    */ 
    virtual bool init(PinName pn = GPS_IF( GPSINT, NC /* D7 resistor R67 on shield not mounted */));
    
    /** Get a line from the physical interface. 
        \param buf the buffer to store it
        \param len size of the buffer
        \return type and length if something was found, 
                WAIT if not enough data is available
                NOT_FOUND if nothing was found
    */ 
    virtual int getMessage(char* buf, int len);
    
    /** send a buffer
        \param buf the buffer to write
        \param len size of the buffer to write
        \return bytes written
    */
    virtual int send(const char* buf, int len);
    
    /** send a NMEA message, this function just takes the 
        payload and calculates and adds checksum. ($ and *XX\r\n will be added)
        \param buf the message payload to write
        \param len size of the message payload to write
        \return total bytes written
    */
    virtual int sendNmea(const char* buf, int len);
    
    /** send a UBX message, this function just takes the 
        payload and calculates and adds checksum.
        \param cls the UBX class id 
        \param id the UBX message id
        \param buf the message payload to write
        \param len size of the message payload to write
        \return total bytes written
    */
    virtual int sendUbx(unsigned char cls, unsigned char id, 
                        const void* buf = NULL, int len = 0);
                        
protected:
    /** check if the port is writeable (like SerialPipe)
        \return true if writeable        
    */
    bool writeable(void) { return true; }
    
    /** Write a character (like SerialPipe)
        \param c  the character to write
        \return true if succesffully written 
    */
    bool putc(int c)     { char ch = c; return send(&ch, 1); }
    
    /** Write bytes to the physical interface.
        \param buf the buffer to write
        \param len size of the buffer to write
        \return bytes written
    */
    virtual int _send(const void* buf, int len);
    
    /** read bytes from the physical interface.
        \param buf the buffer to read into
        \param len size of the read buffer 
        \return bytes read
    */
    int _get(char* buf, int len);
    
    Pipe<char> _pipe;           //!< the rx pipe
    unsigned char _i2cAdr;      //!< the i2c address
    static const char REGLEN;   //!< the length i2c register address
    static const char REGSTREAM;//!< the stream i2c register address
};
