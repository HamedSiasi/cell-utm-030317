#pragma once 

#include "mbed.h"
#include "Pipe.h"

#define _SerialPipeBase SerialBase //!< base class used by this class

/** Buffered serial interface (rtos capable/interrupt driven)
*/
class SerialPipe : public _SerialPipeBase
{
public:
    /** Constructor
        \param tx the trasmitting pin
        \param rx the receiving pin
        \param rxSize the size of the receiving buffer
        \param txSize the size of the transmitting buffer
    */
    SerialPipe(PinName tx, PinName rx, int rxSize = 128, int txSize = 128);
    
    /** Destructor
    */
    virtual ~SerialPipe(void);
    
    // tx channel
    //----------------------------------------------------
    
    /** check if writable 
        return the number of free bytes
    */
    int writeable(void);
    
    /** send a character (blocking)
        \param c the character to send
        \return c
    */
    int putc(int c);
    
    /** send a buffer
        \param buffer the buffer to send
        \param length the size of the buffer to send
        \param blocking, if true this function will block 
               until all bytes placed in the buffer. 
        \return the number of bytes written 
    */
    int put(const void* buffer, int length, bool blocking);
    
    // rx channel
    //----------------------------------------------------
    
    /** check if readable
        \return the size available in the buffer.
    */
    int readable(void);
    
    /** receive one character from the serial port (blocking)
        \param the character received 
    */
    int getc(void);
    
    /** read a buffer from the serial port
        \param pointer to the buffer to read.
        \param length number of bytes to read 
        \param blocking true if all bytes shall be read. false if only the available bytes.
        \return the number of bytes read.
    */
    int get(void* buffer, int length, bool blocking);
    
protected:
    //! receive interrupt routine
    void rxIrqBuf(void);
    //! transmit interrupt woutine 
    void txIrqBuf(void);
    //! start transmission helper
    void txStart(void);
    //! move bytes to hardware
    void txCopy(void);
    Pipe<char> _pipeRx; //!< receive pipe
    Pipe<char> _pipeTx; //!< transmit pipe
};
