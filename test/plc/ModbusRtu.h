/**
 * @file 	ModbusRtu.h
 * @version     1.21
 * @date        2016.02.21
 * @author 	Samuel Marco i Armengol
 * @contact     sammarcoarmengol@gmail.com
 * @contribution Helium6072
 *
 * @description
 *  Arduino library for communicating with Modbus devices
 *  over RS232/USB/485 via RTU protocol.
 *
 *  Further information:
 *  http://modbus.org/
 *  http://modbus.org/docs/Modbus_over_serial_line_V1_02.pdf
 *
 * @license
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; version
 *  2.1 of the License.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * @defgroup setup Modbus Object Instantiation/Initialization
 * @defgroup loop Modbus Object Management
 * @defgroup buffer Modbus Buffer Management
 * @defgroup discrete Modbus Function Codes for Discrete Coils/Inputs
 * @defgroup register Modbus Function Codes for Holding/Input Registers
 *
 */

#include <inttypes.h>


/**
 * @struct modbus_t
 * @brief
 * Master query structure:
 * This includes all the necessary fields to make the Master generate a Modbus query.
 * A Master may keep several of these structures and send them cyclically or
 * use them according to program needs.
 */
typedef struct
{
    uint8_t u8id;          /*!< Slave address between 1 and 247. 0 means broadcast */
    uint8_t u8fct;         /*!< Function code: 1, 2, 3, 4, 5, 6, 15 or 16 */
    uint16_t u16RegAdd;    /*!< Address of the first register to access at slave/s */
    uint16_t u16CoilsNo;   /*!< Number of coils or registers to access */
    uint16_t *au16reg;     /*!< Pointer to memory image in master */
}
modbus_t;

enum
{
    RESPONSE_SIZE = 6,
    EXCEPTION_SIZE = 3,
    CHECKSUM_SIZE = 2
};

/**
 * @enum MESSAGE
 * @brief
 * Indexes to telegram frame positions
 */
enum MESSAGE
{
    ID                             = 0, //!< ID field
    FUNC, //!< Function code position
    ADD_HI, //!< Address high byte
    ADD_LO, //!< Address low byte
    NB_HI, //!< Number of coils or registers high byte
    NB_LO, //!< Number of coils or registers low byte
    BYTE_CNT  //!< byte counter
};

/**
 * @enum MB_FC
 * @brief
 * Modbus function codes summary.
 * These are the implement function codes either for Master or for Slave.
 *
 * @see also fctsupported
 * @see also modbus_t
 */
enum MB_FC
{
    MB_FC_NONE                     = 0,   /*!< null operator */
    MB_FC_READ_COILS               = 1,	/*!< FCT=1 -> read coils or digital outputs */
    MB_FC_READ_DISCRETE_INPUT      = 2,	/*!< FCT=2 -> read digital inputs */
    MB_FC_READ_REGISTERS           = 3,	/*!< FCT=3 -> read registers or analog outputs */
    MB_FC_READ_INPUT_REGISTER      = 4,	/*!< FCT=4 -> read analog inputs */
    MB_FC_WRITE_COIL               = 5,	/*!< FCT=5 -> write single coil or output */
    MB_FC_WRITE_REGISTER           = 6,	/*!< FCT=6 -> write single register */
    // the payload of these two cmd can be as long as 255 bytes(259B in total)
    MB_FC_WRITE_MULTIPLE_COILS     = 15,	/*!< FCT=15 -> write multiple coils or outputs */
    MB_FC_WRITE_MULTIPLE_REGISTERS = 16	/*!< FCT=16 -> write multiple registers */
};

enum COM_STATES
{
    COM_IDLE                     = 0,
    COM_WAITING                  = 1

};

enum ERR_LIST
{
    ERR_NOT_MASTER                = -1,
    ERR_POLLING                   = -2,
    ERR_BUFF_OVERFLOW             = -3,
    ERR_BAD_CRC                   = -4,
    ERR_EXCEPTION                 = -5
};

enum
{
    NO_REPLY = 255,
    EXC_FUNC_CODE = 1,
    EXC_ADDR_RANGE = 2,
    EXC_REGS_QUANT = 3,
    EXC_EXECUTE = 4
};

const unsigned char fctsupported[] =
{
    MB_FC_READ_COILS,
    MB_FC_READ_DISCRETE_INPUT,
    MB_FC_READ_REGISTERS,
    MB_FC_READ_INPUT_REGISTER,
    MB_FC_WRITE_COIL,
    MB_FC_WRITE_REGISTER,
    MB_FC_WRITE_MULTIPLE_COILS,
    MB_FC_WRITE_MULTIPLE_REGISTERS
};

#define T35  5
#define  MAX_BUFFER  64	//!< maximum size for the communication buffer in bytes

/**
 * @class Modbus
 * @brief
 * Arduino class library for communicating with Modbus devices over
 * USB/RS232/485 (via RTU protocol).
 */
class Modbus
{
private:
    HardwareSerial *port; //!< Pointer to Serial class object
    #if defined(__AVR__)
    SoftwareSerial *softPort; //!< Pointer to SoftwareSerial class object
    #endif
    
    uint8_t u8id; //!< 0=master, 1..247=slave number
    uint8_t u8serno; //!< serial port: 0-Serial, 1..3-Serial1..Serial3; 4: use software serial
    uint8_t u8txenpin; //!< flow control pin: 0=USB or RS-232 mode, >0=RS-485 mode
    uint8_t u8state;
    uint8_t u8lastError;
    uint8_t au8Buffer[MAX_BUFFER];
    uint8_t u8BufferSize;
    uint8_t u8lastRec;
    uint16_t *au16regs;
    uint16_t u16InCnt, u16OutCnt, u16errCnt;
    uint16_t u16timeOut;
    uint32_t u32time, u32timeOut;
    uint8_t u8regsize;

    void init(uint8_t u8id, uint8_t u8serno, uint8_t u8txenpin);
	  void init(uint8_t u8id);
    void sendTxBuffer();
    int8_t getRxBuffer();
    uint16_t calcCRC(uint8_t u8length);
    uint8_t validateAnswer();
    uint8_t validateRequest();
    void get_FC1();
    void get_FC3();
    int8_t process_FC1( uint16_t *regs, uint8_t u8size );
    int8_t process_FC3( uint16_t *regs, uint8_t u8size );
    int8_t process_FC5( uint16_t *regs, uint8_t u8size );
    int8_t process_FC6( uint16_t *regs, uint8_t u8size );
    int8_t process_FC15( uint16_t *regs, uint8_t u8size );
    int8_t process_FC16( uint16_t *regs, uint8_t u8size );
    void buildException( uint8_t u8exception ); // build exception message

public:
    Modbus();
    Modbus(uint8_t u8id, uint8_t u8serno);
    Modbus(uint8_t u8id, uint8_t u8serno, uint8_t u8txenpin);
    Modbus(uint8_t u8id);
    void begin(long u32speed);
    #if defined(__AVR__)
    void begin(SoftwareSerial *sPort, long u32speed);
    #endif
    void begin(long u32speed, uint8_t u8config);
    void begin();
    void setTimeOut( uint16_t u16timeout); //!<write communication watch-dog timer
    uint16_t getTimeOut(); //!<get communication watch-dog timer value
    boolean getTimeOutState(); //!<get communication watch-dog timer state
    int8_t query( modbus_t telegram ); //!<only for master
    int8_t poll(); //!<cyclic poll for master
    int8_t poll( uint16_t *regs, uint8_t u8size ); //!<cyclic poll for slave
    uint16_t getInCnt(); //!<number of incoming messages
    uint16_t getOutCnt(); //!<number of outcoming messages
    uint16_t getErrCnt(); //!<error counter
    uint8_t getID(); //!<get slave ID between 1 and 247
    uint8_t getState();
    uint8_t getLastError(); //!<get last error message
    void setID( uint8_t u8id ); //!<write new ID for the slave
    void end(); //!<finish any communication and release serial communication port
};

/* _____PUBLIC FUNCTIONS_____________________________________________________ */

/**
 * @brief
 * Default Constructor for Master through Serial
 *
 * @ingroup setup
 */
Modbus::Modbus()
{
    init(0, 0, 0);
}

/**
 * @brief
 * Full constructor for a Master/Slave through USB/RS232C
 *
 * @param u8id   node address 0=master, 1..247=slave
 * @param u8serno  serial port used 0..3
 * @ingroup setup
 * @overload Modbus::Modbus(uint8_t u8id, uint8_t u8serno)
 * @overload Modbus::Modbus(uint8_t u8id)
 * @overload Modbus::Modbus()
 */
Modbus::Modbus(uint8_t u8id, uint8_t u8serno)
{
    init(u8id, u8serno, 0);
}

/**
 * @brief
 * Full constructor for a Master/Slave through USB/RS232C/RS485
 * It needs a pin for flow control only for RS485 mode
 *
 * @param u8id   node address 0=master, 1..247=slave
 * @param u8serno  serial port used 0..3
 * @param u8txenpin pin for txen RS-485 (=0 means USB/RS232C mode)
 * @ingroup setup
 * @overload Modbus::Modbus(uint8_t u8id, uint8_t u8serno, uint8_t u8txenpin)
 * @overload Modbus::Modbus(uint8_t u8id)
 * @overload Modbus::Modbus()
 */
Modbus::Modbus(uint8_t u8id, uint8_t u8serno, uint8_t u8txenpin)
{
    init(u8id, u8serno, u8txenpin);
}

/**
 * @brief
 * Constructor for a Master/Slave through USB/RS232C via software serial
 * This constructor only specifies u8id (node address) and should be only
 * used if you want to use software serial instead of hardware serial.
 * If you use this constructor you have to begin ModBus object by
 * using "void Modbus::begin(SoftwareSerial *softPort, long u32speed)".
 *
 * @param u8id   node address 0=master, 1..247=slave
 * @ingroup setup
 * @overload Modbus::Modbus(uint8_t u8id, uint8_t u8serno, uint8_t u8txenpin)
 * @overload Modbus::Modbus(uint8_t u8id, uint8_t u8serno)
 * @overload Modbus::Modbus()
 */
Modbus::Modbus(uint8_t u8id)
{
    init(u8id);
}

/**
 * @brief
 * Initialize class object.
 *
 * Sets up the serial port using specified baud rate.
 * Call once class has been instantiated, typically within setup().
 *
 * @see http://arduino.cc/en/Serial/Begin#.Uy4CJ6aKlHY
 * @param speed   baud rate, in standard increments (300..115200)
 * @ingroup setup
 */
void Modbus::begin(long u32speed)
{

    switch( u8serno )
    {
#if defined(__AVR__)
#if defined(UBRR1H)
    case 1:
        port = &Serial1;
        break;
#endif

#if defined(UBRR2H)
    case 2:
        port = &Serial2;
        break;
#endif

#if defined(UBRR3H)
    case 3:
        port = &Serial3;
        break;
#endif

#else
    case 1:
        port = &Serial;
        break;

    case 2:
        port = &Serial;
        break;
        
    case 3:
        port = &Serial;
        break;
#endif

    case 0:
    default:
        port = &Serial;
        break;
    }

    port->begin(u32speed);
    if (u8txenpin > 1)   // pin 0 & pin 1 are reserved for RX/TX
    {
        // return RS485 transceiver to transmit mode
        pinMode(u8txenpin, OUTPUT);
        digitalWrite(u8txenpin, LOW);
    }

    while(port->read() >= 0);
    u8lastRec = u8BufferSize = 0;
    u16InCnt = u16OutCnt = u16errCnt = 0;
}

/**
 * @brief
 * Initialize class object.
 *
 * Sets up the software serial port using specified baud rate and SoftwareSerial object.
 * Call once class has been instantiated, typically within setup().
 *
 * @param speed   *softPort, pointer to SoftwareSerial class object
 * @param speed   baud rate, in standard increments (300..115200)
 * @ingroup setup
 */

#if defined(__AVR__)
void Modbus::begin(SoftwareSerial *sPort, long u32speed)
{

    softPort=sPort;

    softPort->begin(u32speed);

    if (u8txenpin > 1)   // pin 0 & pin 1 are reserved for RX/TX
    {
        // return RS485 transceiver to transmit mode
        pinMode(u8txenpin, OUTPUT);
        digitalWrite(u8txenpin, LOW);
    }

    while(softPort->read() >= 0);
    u8lastRec = u8BufferSize = 0;
    u16InCnt = u16OutCnt = u16errCnt = 0;
}
#endif
#if 0 // port->begin(u32speed, u8config); is not supported by arduino due, this api is not used by app
/**
 * @brief
 * Initialize class object.
 *
 * Sets up the serial port using specified baud rate.
 * Call once class has been instantiated, typically within setup().
 *
 * @see http://arduino.cc/en/Serial/Begin#.Uy4CJ6aKlHY
 * @param speed   baud rate, in standard increments (300..115200)
 * @param config  data frame settings (data length, parity and stop bits)
 * @ingroup setup
 */
void Modbus::begin(long u32speed,uint8_t u8config)
{

    switch( u8serno )
    {
#if defined(__AVR__)
#if defined(UBRR1H)
    case 1:
        port = &Serial1;
        break;
#endif

#if defined(UBRR2H)
    case 2:
        port = &Serial2;
        break;
#endif

#if defined(UBRR3H)
    case 3:
        port = &Serial3;
        break;
#endif
#else
    
    case 1:
        port = &Serial1;
        break;

    case 2:
        port = &Serial2;
        break;
    case 3:
        port = &Serial3;
        break;
    case 0:
    default:
        port = &Serial;
        break;
   #endif
    
    }

    port->begin(u32speed, u8config);
    if (u8txenpin > 1)   // pin 0 & pin 1 are reserved for RX/TX
    {
        // return RS485 transceiver to transmit mode
        pinMode(u8txenpin, OUTPUT);
        digitalWrite(u8txenpin, LOW);
    }

    while(port->read() >= 0);
    u8lastRec = u8BufferSize = 0;
    u16InCnt = u16OutCnt = u16errCnt = 0;
}
#endif
/**
 * @brief
 * Initialize default class object.
 *
 * Sets up the serial port using 19200 baud.
 * Call once class has been instantiated, typically within setup().
 *
 * @overload Modbus::begin(uint16_t u16BaudRate)
 * @ingroup setup
 */
void Modbus::begin()
{
    begin(19200);
}

/**
 * @brief
 * Method to write a new slave ID address
 *
 * @param 	u8id	new slave address between 1 and 247
 * @ingroup setup
 */
void Modbus::setID( uint8_t u8id)
{
    if (( u8id != 0) && (u8id <= 247))
    {
        this->u8id = u8id;
    }
}

/**
 * @brief
 * Method to read current slave ID address
 *
 * @return u8id	current slave address between 1 and 247
 * @ingroup setup
 */
uint8_t Modbus::getID()
{
    return this->u8id;
}

/**
 * @brief
 * Initialize time-out parameter
 *
 * Call once class has been instantiated, typically within setup().
 * The time-out timer is reset each time that there is a successful communication
 * between Master and Slave. It works for both.
 *
 * @param time-out value (ms)
 * @ingroup setup
 */
void Modbus::setTimeOut( uint16_t u16timeOut)
{
    this->u16timeOut = u16timeOut;
}

/**
 * @brief
 * Return communication Watchdog state.
 * It could be usefull to reset outputs if the watchdog is fired.
 *
 * @return TRUE if millis() > u32timeOut
 * @ingroup loop
 */
boolean Modbus::getTimeOutState()
{
    return (millis() > u32timeOut);
}

/**
 * @brief
 * Get input messages counter value
 * This can be useful to diagnose communication
 *
 * @return input messages counter
 * @ingroup buffer
 */
uint16_t Modbus::getInCnt()
{
    return u16InCnt;
}

/**
 * @brief
 * Get transmitted messages counter value
 * This can be useful to diagnose communication
 *
 * @return transmitted messages counter
 * @ingroup buffer
 */
uint16_t Modbus::getOutCnt()
{
    return u16OutCnt;
}

/**
 * @brief
 * Get errors counter value
 * This can be useful to diagnose communication
 *
 * @return errors counter
 * @ingroup buffer
 */
uint16_t Modbus::getErrCnt()
{
    return u16errCnt;
}

/**
 * Get modbus master state
 *
 * @return = 0 IDLE, = 1 WAITING FOR ANSWER
 * @ingroup buffer
 */
uint8_t Modbus::getState()
{
    return u8state;
}

/**
 * Get the last error in the protocol processor
 *
 * @returnreturn   NO_REPLY = 255      Time-out
 * @return   EXC_FUNC_CODE = 1   Function code not available
 * @return   EXC_ADDR_RANGE = 2  Address beyond available space for Modbus registers
 * @return   EXC_REGS_QUANT = 3  Coils or registers number beyond the available space
 * @ingroup buffer
 */
uint8_t Modbus::getLastError()
{
    return u8lastError;
}

/**
 * @brief
 * *** Only Modbus Master ***
 * Generate a query to an slave with a modbus_t telegram structure
 * The Master must be in COM_IDLE mode. After it, its state would be COM_WAITING.
 * This method has to be called only in loop() section.
 *
 * @see modbus_t
 * @param modbus_t  modbus telegram structure (id, fct, ...)
 * @ingroup loop
 * @todo finish function 15
 */
int8_t Modbus::query( modbus_t telegram )
{
    uint8_t u8regsno, u8bytesno;
    if (u8id!=0) return -2;
    if (u8state != COM_IDLE) return -1;

    if ((telegram.u8id==0) || (telegram.u8id>247)) return -3;

    au16regs = telegram.au16reg;

    // telegram header
    au8Buffer[ ID ]         = telegram.u8id;
    au8Buffer[ FUNC ]       = telegram.u8fct;
    au8Buffer[ ADD_HI ]     = highByte(telegram.u16RegAdd );
    au8Buffer[ ADD_LO ]     = lowByte( telegram.u16RegAdd );

    switch( telegram.u8fct )
    {
    case MB_FC_READ_COILS:
    case MB_FC_READ_DISCRETE_INPUT:
    case MB_FC_READ_REGISTERS:
    case MB_FC_READ_INPUT_REGISTER:
        au8Buffer[ NB_HI ]      = highByte(telegram.u16CoilsNo );
        au8Buffer[ NB_LO ]      = lowByte( telegram.u16CoilsNo );
        u8BufferSize = 6;
        break;
    case MB_FC_WRITE_COIL:
        au8Buffer[ NB_HI ]      = ((au16regs[0] > 0) ? 0xff : 0);
        au8Buffer[ NB_LO ]      = 0;
        u8BufferSize = 6;
        break;
    case MB_FC_WRITE_REGISTER:
        au8Buffer[ NB_HI ]      = highByte(au16regs[0]);
        au8Buffer[ NB_LO ]      = lowByte(au16regs[0]);
        u8BufferSize = 6;
        break;
    case MB_FC_WRITE_MULTIPLE_COILS: // TODO: implement "sending coils"
        u8regsno = telegram.u16CoilsNo / 16;
        u8bytesno = u8regsno * 2;
        if ((telegram.u16CoilsNo % 16) != 0)
        {
            u8bytesno++;
            u8regsno++;
        }

        au8Buffer[ NB_HI ]      = highByte(telegram.u16CoilsNo );
        au8Buffer[ NB_LO ]      = lowByte( telegram.u16CoilsNo );
        au8Buffer[ NB_LO+1 ]    = u8bytesno;
        u8BufferSize = 7;

        u8regsno = u8bytesno = 0; // now auxiliary registers
        for (uint16_t i = 0; i < telegram.u16CoilsNo; i++)
        {


        }
        break;

    case MB_FC_WRITE_MULTIPLE_REGISTERS:
        au8Buffer[ NB_HI ]      = highByte(telegram.u16CoilsNo );
        au8Buffer[ NB_LO ]      = lowByte( telegram.u16CoilsNo );
        au8Buffer[ NB_LO+1 ]    = (uint8_t) ( telegram.u16CoilsNo * 2 );
        u8BufferSize = 7;

        for (uint16_t i=0; i< telegram.u16CoilsNo; i++)
        {
            au8Buffer[ u8BufferSize ] = highByte( au16regs[ i ] );
            u8BufferSize++;
            au8Buffer[ u8BufferSize ] = lowByte( au16regs[ i ] );
            u8BufferSize++;
        }
        break;
    }

    sendTxBuffer();
    u8state = COM_WAITING;
    return 0;
}

/**
 * @brief *** Only for Modbus Master ***
 * This method checks if there is any incoming answer if pending.
 * If there is no answer, it would change Master state to COM_IDLE.
 * This method must be called only at loop section.
 * Avoid any delay() function.
 *
 * Any incoming data would be redirected to au16regs pointer,
 * as defined in its modbus_t query telegram.
 *
 * @params	nothing
 * @return errors counter
 * @ingroup loop
 */
int8_t Modbus::poll()
{
    // check if there is any incoming frame
	uint8_t u8current;
    if(u8serno<4)
        u8current = port->available();
    #if defined(__AVR__)
    else
        u8current = softPort->available();
    #endif

    if (millis() > u32timeOut)
    {
        u8state = COM_IDLE;
        u8lastError = NO_REPLY;
        u16errCnt++;
        return 0;
    }

    if (u8current == 0) return 0;

    // check T35 after frame end or still no frame end
    if (u8current != u8lastRec)
    {
        u8lastRec = u8current;
        u32time = millis() + T35;
        return 0;
    }
    if (millis() < u32time) return 0;

    // transfer Serial buffer frame to auBuffer
    u8lastRec = 0;
    int8_t i8state = getRxBuffer();
    if (i8state < 7)
    {
        u8state = COM_IDLE;
        u16errCnt++;
        return i8state;
    }

    // validate message: id, CRC, FCT, exception
    uint8_t u8exception = validateAnswer();
    if (u8exception != 0)
    {
        u8state = COM_IDLE;
        return u8exception;
    }

    // process answer
    switch( au8Buffer[ FUNC ] )
    {
    case MB_FC_READ_COILS:
    case MB_FC_READ_DISCRETE_INPUT:
        // call get_FC1 to transfer the incoming message to au16regs buffer
        get_FC1( );
        break;
    case MB_FC_READ_INPUT_REGISTER:
    case MB_FC_READ_REGISTERS :
        // call get_FC3 to transfer the incoming message to au16regs buffer
        get_FC3( );
        break;
    case MB_FC_WRITE_COIL:
    case MB_FC_WRITE_REGISTER :
    case MB_FC_WRITE_MULTIPLE_COILS:
    case MB_FC_WRITE_MULTIPLE_REGISTERS :
        // nothing to do
        break;
    default:
        break;
    }
    u8state = COM_IDLE;
    return u8BufferSize;
}

/**
 * @brief
 * *** Only for Modbus Slave ***
 * This method checks if there is any incoming query
 * Afterwards, it would shoot a validation routine plus a register query
 * Avoid any delay() function !!!!
 * After a successful frame between the Master and the Slave, the time-out timer is reset.
 *
 * @param *regs  register table for communication exchange
 * @param u8size  size of the register table
 * @return 0 if no query, 1..4 if communication error, >4 if correct query processed
 * @ingroup loop
 */
int8_t Modbus::poll( uint16_t *regs, uint8_t u8size )
{

    au16regs = regs;
    u8regsize = u8size;
	uint8_t u8current;


    // check if there is any incoming frame
    if(u8serno<4)
        u8current = port->available();
    #if defined(__AVR__)
    else
        u8current = softPort->available();
    #endif
    
    // if no bytes in circular buffer
    if (u8current == 0) return 0;

/*
    // if new bytes have been received from USART since last iteration, 
    // push off timeout by T35 (5ms)
    if (u8current != u8lastRec)
    {
        u8lastRec = u8current;
        u32time = millis() + T35;
        return 0;
    }
    // no new bytes have been received since last poll, wait until timeout
    if (millis() < u32time) return 0;

    // timeout happens because no new bytes are received in last T35 (5ms)
    // timeout indicates end of a data frame
    // Delimiting frame by timeout shows modbus protocol is designed for low speed operation
    u8lastRec = 0;
*/


    // pop input from circular buffer into au8Buffer
    // returns bytes read or ERR_BUFF_OVERFLOW(-3)
    // TODO be careful in removing timeout, as extra bytes out of one cmd is dropped
/*
    int8_t i8state = getRxBuffer();
    u8lastError = i8state;
    // min frame length is 7 bytes, omit bytes received if less than min bytes
    if (i8state < 7) return i8state;
*/

    int8_t i8state;
    if (u8current >= 8) {
        i8state = getRxBuffer();
        u8lastError = i8state;
    } else {
        return 0;
    }

    // check slave id, omit bytes if doesn't match
    if (au8Buffer[ ID ] != u8id) return 0;

    // validate message: CRC, FCT, address and size
    uint8_t u8exception = validateRequest();
    if (u8exception > 0)
    {
        if (u8exception != NO_REPLY)
        {
            buildException( u8exception );
            sendTxBuffer();
        }
        u8lastError = u8exception;
        return u8exception;
    }

    // this value is not used by slave
    u32timeOut = millis() + long(u16timeOut);
    u8lastError = 0;

    // process message
    switch( au8Buffer[ FUNC ] )
    {
    case MB_FC_READ_COILS:
    case MB_FC_READ_DISCRETE_INPUT:
        return process_FC1( regs, u8size );
        break;
    case MB_FC_READ_INPUT_REGISTER:
    case MB_FC_READ_REGISTERS :
        return process_FC3( regs, u8size );
        break;
    case MB_FC_WRITE_COIL:
        return process_FC5( regs, u8size );
        break;
    case MB_FC_WRITE_REGISTER :
        return process_FC6( regs, u8size );
        break;
    case MB_FC_WRITE_MULTIPLE_COILS:
        return process_FC15( regs, u8size );
        break;
    case MB_FC_WRITE_MULTIPLE_REGISTERS :
        return process_FC16( regs, u8size );
        break;
    default:
        break;
    }
    return i8state;
}

/* _____PRIVATE FUNCTIONS_____________________________________________________ */

void Modbus::init(uint8_t u8id, uint8_t u8serno, uint8_t u8txenpin)
{
    this->u8id = u8id;
    this->u8serno = (u8serno > 3) ? 0 : u8serno;
    this->u8txenpin = u8txenpin;
    this->u16timeOut = 1000;
}

void Modbus::init(uint8_t u8id)
{
    this->u8id = u8id;
    this->u8serno = 4;
    this->u8txenpin = 0;
    this->u16timeOut = 1000;
}

/**
 * @brief
 * This method moves Serial buffer data to the Modbus au8Buffer.
 *
 * @return buffer size if OK, ERR_BUFF_OVERFLOW if u8BufferSize >= MAX_BUFFER
 * @ingroup buffer
 */
int8_t Modbus::getRxBuffer()
{
    boolean bBuffOverflow = false;

    if (u8txenpin > 1) digitalWrite( u8txenpin, LOW );

    u8BufferSize = 0;
    if(u8serno<4)
        while ( port->available() )
        {
            au8Buffer[ u8BufferSize ] = port->read();
            u8BufferSize ++;

            if (u8BufferSize >= MAX_BUFFER) bBuffOverflow = true;
        }
    #if defined(__AVR__)
    else
        while ( softPort->available() )
        {
            au8Buffer[ u8BufferSize ] = softPort->read();
            u8BufferSize ++;

            if (u8BufferSize >= MAX_BUFFER) bBuffOverflow = true;
        }
     #endif
    u16InCnt++;

    if (bBuffOverflow)
    {
        u16errCnt++;
        return ERR_BUFF_OVERFLOW;
    }
    return u8BufferSize;
}

/**
 * @brief
 * This method transmits au8Buffer to Serial line.
 * Only if u8txenpin != 0, there is a flow handling in order to keep
 * the RS485 transceiver in output state as long as the message is being sent.
 * This is done with UCSRxA register.
 * The CRC is appended to the buffer before starting to send it.
 *
 * @param nothing
 * @return nothing
 * @ingroup buffer
 */
void Modbus::sendTxBuffer()
{
    uint8_t i = 0;

    // append CRC to message
    uint16_t u16crc = calcCRC( u8BufferSize );
    au8Buffer[ u8BufferSize ] = u16crc >> 8;
    u8BufferSize++;
    au8Buffer[ u8BufferSize ] = u16crc & 0x00ff;
    u8BufferSize++;

    // set RS485 transceiver to transmit mode
    if (u8txenpin > 1)
    {
#if defined(__AVR__)
 switch( u8serno )
        {
#if defined(UBRR1H)
        case 1:
            UCSR1A=UCSR1A |(1 << TXC1);
            break;
#endif

#if defined(UBRR2H)
        case 2:
            UCSR2A=UCSR2A |(1 << TXC2);
            break;
#endif

#if defined(UBRR3H)
        case 3:
            UCSR3A=UCSR3A |(1 << TXC3);
            break;
#endif
        case 0:
        default:
            UCSR0A=UCSR0A |(1 << TXC0);
            break;
        }
#endif
        digitalWrite( u8txenpin, HIGH );
    }

    // transfer buffer to serial line
    if(u8serno<4)
        port->write( au8Buffer, u8BufferSize );
    #if    defined(__AVR__)
    else
        softPort->write( au8Buffer, u8BufferSize );
    #endif



    // keep RS485 transceiver in transmit mode as long as sending
    if (u8txenpin > 1)
    {
      #if defined(__AVR__)
        switch( u8serno )
        {
#if defined(UBRR1H)
        case 1:
            while (!(UCSR1A & (1 << TXC1)));
            break;
#endif

#if defined(UBRR2H)
        case 2:
            while (!(UCSR2A & (1 << TXC2)));
            break;
#endif

#if defined(UBRR3H)
        case 3:
            while (!(UCSR3A & (1 << TXC3)));
            break;
#endif
        case 0:
        default:
            while (!(UCSR0A & (1 << TXC0)));
            break;

        }
 #endif
        // return RS485 transceiver to receive mode
        digitalWrite( u8txenpin, LOW );
    }
    if(u8serno<4)
        // empty circular buffer
        // TODO this could waste fuzzing data
        while(port->read() >= 0);
    #if defined(__AVR__)
    else
        while(softPort->read() >= 0);
    #endif
    
    u8BufferSize = 0;

    // set time-out for master
    u32timeOut = millis() + (unsigned long) u16timeOut;

    // increase message counter
    u16OutCnt++;
}

/**
 * @brief
 * This method calculates CRC
 *
 * @return uint16_t calculated CRC value for the message
 * @ingroup buffer
 */
uint16_t Modbus::calcCRC(uint8_t u8length)
{
    unsigned int temp, temp2, flag;
    temp = 0xFFFF;
    for (unsigned char i = 0; i < u8length; i++)
    {
        temp = temp ^ au8Buffer[i];
        for (unsigned char j = 1; j <= 8; j++)
        {
            flag = temp & 0x0001;
            temp >>=1;
            if (flag)
                temp ^= 0xA001;
        }
    }
    // Reverse byte order.
    temp2 = temp >> 8;
    temp = (temp << 8) | temp2;
    temp &= 0xFFFF;
    // the returned value is already swapped
    // crcLo byte is first & crcHi byte is last
    return temp;
}

/**
 * @brief
 * This method validates slave incoming messages
 *
 * @return 0 if OK, EXCEPTION if anything fails
 * @ingroup buffer
 */
uint8_t Modbus::validateRequest()
{
    // check message crc vs calculated crc
    // The assumption that last 2 bytes in the frame are CRC doesn't hold during fuzzing
/*
    uint16_t u16MsgCRC =
        ((au8Buffer[u8BufferSize - 2] << 8)
         | au8Buffer[u8BufferSize - 1]); // combine the crc Low & High bytes
    if ( calcCRC( u8BufferSize-2 ) != u16MsgCRC )
    {
        u16errCnt ++;
        return NO_REPLY;
    }
*/

    // check fct code
    boolean isSupported = false;
    for (uint8_t i = 0; i< sizeof( fctsupported ); i++)
    {
        if (fctsupported[i] == au8Buffer[FUNC])
        {
            isSupported = 1;
            break;
        }
    }
    if (!isSupported)
    {
        u16errCnt ++;
        return EXC_FUNC_CODE;
    }

    // check start address & nb range
    uint16_t u16regs = 0;
    uint8_t u8regs;
    switch ( au8Buffer[ FUNC ] )
    {
    case MB_FC_READ_COILS:
    case MB_FC_READ_DISCRETE_INPUT:
    case MB_FC_WRITE_MULTIPLE_COILS:
        u16regs = word( au8Buffer[ ADD_HI ], au8Buffer[ ADD_LO ]) / 16;
        u16regs += word( au8Buffer[ NB_HI ], au8Buffer[ NB_LO ]) /16;
        u8regs = (uint8_t) u16regs;
        if (u8regs > u8regsize) return EXC_ADDR_RANGE;
        break;
    case MB_FC_WRITE_COIL:
        u16regs = word( au8Buffer[ ADD_HI ], au8Buffer[ ADD_LO ]) / 16;
        u8regs = (uint8_t) u16regs;
        if (u8regs > u8regsize) return EXC_ADDR_RANGE;
        break;
    case MB_FC_WRITE_REGISTER :
        u16regs = word( au8Buffer[ ADD_HI ], au8Buffer[ ADD_LO ]);
        u8regs = (uint8_t) u16regs;
        if (u8regs > u8regsize) return EXC_ADDR_RANGE;
        break;
    case MB_FC_READ_REGISTERS :
    case MB_FC_READ_INPUT_REGISTER :
    case MB_FC_WRITE_MULTIPLE_REGISTERS :
        u16regs = word( au8Buffer[ ADD_HI ], au8Buffer[ ADD_LO ]);
        u16regs += word( au8Buffer[ NB_HI ], au8Buffer[ NB_LO ]);
        u8regs = (uint8_t) u16regs;
        if (u8regs > u8regsize) return EXC_ADDR_RANGE;
        break;
    }
    return 0; // OK, no exception code thrown
}

/**
 * @brief
 * This method validates master incoming messages
 *
 * @return 0 if OK, EXCEPTION if anything fails
 * @ingroup buffer
 */
uint8_t Modbus::validateAnswer()
{
    // check message crc vs calculated crc
    uint16_t u16MsgCRC =
        ((au8Buffer[u8BufferSize - 2] << 8)
         | au8Buffer[u8BufferSize - 1]); // combine the crc Low & High bytes
    if ( calcCRC( u8BufferSize-2 ) != u16MsgCRC )
    {
        u16errCnt ++;
        return NO_REPLY;
    }

    // check exception
    if ((au8Buffer[ FUNC ] & 0x80) != 0)
    {
        u16errCnt ++;
        return ERR_EXCEPTION;
    }

    // check fct code
    boolean isSupported = false;
    for (uint8_t i = 0; i< sizeof( fctsupported ); i++)
    {
        if (fctsupported[i] == au8Buffer[FUNC])
        {
            isSupported = 1;
            break;
        }
    }
    if (!isSupported)
    {
        u16errCnt ++;
        return EXC_FUNC_CODE;
    }

    return 0; // OK, no exception code thrown
}

/**
 * @brief
 * This method builds an exception message
 *
 * @ingroup buffer
 */
void Modbus::buildException( uint8_t u8exception )
{
    uint8_t u8func = au8Buffer[ FUNC ];  // get the original FUNC code

    au8Buffer[ ID ]      = u8id;
    au8Buffer[ FUNC ]    = u8func + 0x80;
    au8Buffer[ 2 ]       = u8exception;
    u8BufferSize         = EXCEPTION_SIZE;
}

/**
 * This method processes functions 1 & 2 (for master)
 * This method puts the slave answer into master data buffer
 *
 * @ingroup register
 * TODO: finish its implementation
 */
void Modbus::get_FC1()
{
    uint8_t u8byte, i;
    u8byte = 0;

    //  for (i=0; i< au8Buffer[ 2 ] /2; i++) {
    //    au16regs[ i ] = word(
    //    au8Buffer[ u8byte ],
    //    au8Buffer[ u8byte +1 ]);
    //    u8byte += 2;
    //  }
}

/**
 * This method processes functions 3 & 4 (for master)
 * This method puts the slave answer into master data buffer
 *
 * @ingroup register
 */
void Modbus::get_FC3()
{
    uint8_t u8byte, i;
    u8byte = 3;

    for (i=0; i< au8Buffer[ 2 ] /2; i++)
    {
        au16regs[ i ] = word(
                            au8Buffer[ u8byte ],
                            au8Buffer[ u8byte +1 ]);
        u8byte += 2;
    }
}

/**
 * @brief
 * This method processes functions 1 & 2
 * This method reads a bit array and transfers it to the master
 *
 * @return u8BufferSize Response to master length
 * @ingroup discrete
 */
int8_t Modbus::process_FC1( uint16_t *regs, uint8_t u8size )
{
    uint8_t u8currentRegister, u8currentBit, u8bytesno, u8bitsno;
    uint8_t u8CopyBufferSize;
    uint16_t u16currentCoil, u16coil;

    // get the first and last coil from the message
    uint16_t u16StartCoil = word( au8Buffer[ ADD_HI ], au8Buffer[ ADD_LO ] );
    uint16_t u16Coilno = word( au8Buffer[ NB_HI ], au8Buffer[ NB_LO ] );

    // put the number of bytes in the outcoming message
    u8bytesno = (uint8_t) (u16Coilno / 8);
    if (u16Coilno % 8 != 0) u8bytesno ++;
    au8Buffer[ ADD_HI ]  = u8bytesno;
    u8BufferSize         = ADD_LO;

    // read each coil from the register map and put its value inside the outcoming message
    u8bitsno = 0;

    for (u16currentCoil = 0; u16currentCoil < u16Coilno; u16currentCoil++)
    {
        u16coil = u16StartCoil + u16currentCoil;
        // 16 bits per register
        u8currentRegister = (uint8_t) (u16coil / 16);
        u8currentBit = (uint8_t) (u16coil % 16);

        bitWrite(
            au8Buffer[ u8BufferSize ],
            u8bitsno,
            // out of bound read can happen here
            bitRead( regs[ u8currentRegister ], u8currentBit ) );
        u8bitsno ++;

        if (u8bitsno > 7)
        {
            u8bitsno = 0;
            u8BufferSize++;
        }
    }

    // send outcoming message
    // TODO seems not necessary, verify it!
    if (u16Coilno % 8 != 0) u8BufferSize ++;
    u8CopyBufferSize = u8BufferSize +2;
    sendTxBuffer();
    return u8CopyBufferSize;
}

/**
 * @brief
 * This method processes functions 3 & 4
 * This method reads a word array and transfers it to the master
 *
 * @return u8BufferSize Response to master length
 * @ingroup register
 */
int8_t Modbus::process_FC3( uint16_t *regs, uint8_t u8size )
{

    uint8_t u8StartAdd = word( au8Buffer[ ADD_HI ], au8Buffer[ ADD_LO ] );
    uint8_t u8regsno = word( au8Buffer[ NB_HI ], au8Buffer[ NB_LO ] );
    uint8_t u8CopyBufferSize;
    uint8_t i;

    au8Buffer[ 2 ]       = u8regsno * 2;
    u8BufferSize         = 3;

    for (i = u8StartAdd; i < u8StartAdd + u8regsno; i++)
    {
        au8Buffer[ u8BufferSize ] = highByte(regs[i]);
        u8BufferSize++;
        au8Buffer[ u8BufferSize ] = lowByte(regs[i]);
        u8BufferSize++;
    }
    u8CopyBufferSize = u8BufferSize +2;
    sendTxBuffer();

    return u8CopyBufferSize;
}

/**
 * @brief
 * This method processes function 5
 * This method writes a value assigned by the master to a single bit
 *
 * @return u8BufferSize Response to master length
 * @ingroup discrete
 */
int8_t Modbus::process_FC5( uint16_t *regs, uint8_t u8size )
{
    uint8_t u8currentRegister, u8currentBit;
    uint8_t u8CopyBufferSize;
    uint16_t u16coil = word( au8Buffer[ ADD_HI ], au8Buffer[ ADD_LO ] );

    // point to the register and its bit
    u8currentRegister = (uint8_t) (u16coil / 16);
    u8currentBit = (uint8_t) (u16coil % 16);

    // write to coil
    bitWrite(
        regs[ u8currentRegister ],
        u8currentBit,
        au8Buffer[ NB_HI ] == 0xff );


    // send answer to master
    u8BufferSize = 6;
    u8CopyBufferSize = u8BufferSize +2;
    sendTxBuffer();

    return u8CopyBufferSize;
}

/**
 * @brief
 * This method processes function 6
 * This method writes a value assigned by the master to a single word
 *
 * @return u8BufferSize Response to master length
 * @ingroup register
 */
int8_t Modbus::process_FC6( uint16_t *regs, uint8_t u8size )
{

    uint8_t u8add = word( au8Buffer[ ADD_HI ], au8Buffer[ ADD_LO ] );
    uint8_t u8CopyBufferSize;
    uint16_t u16val = word( au8Buffer[ NB_HI ], au8Buffer[ NB_LO ] );

    regs[ u8add ] = u16val;

    // keep the same header
    u8BufferSize         = RESPONSE_SIZE;

    u8CopyBufferSize = u8BufferSize +2;
    sendTxBuffer();

    return u8CopyBufferSize;
}

/**
 * @brief
 * This method processes function 15
 * This method writes a bit array assigned by the master
 *
 * @return u8BufferSize Response to master length
 * @ingroup discrete
 */
int8_t Modbus::process_FC15( uint16_t *regs, uint8_t u8size )
{
    uint8_t u8currentRegister, u8currentBit, u8frameByte, u8bitsno;
    uint8_t u8CopyBufferSize;
    uint16_t u16currentCoil, u16coil;
    boolean bTemp;

    // get the first and last coil from the message
    uint16_t u16StartCoil = word( au8Buffer[ ADD_HI ], au8Buffer[ ADD_LO ] );
    uint16_t u16Coilno = word( au8Buffer[ NB_HI ], au8Buffer[ NB_LO ] );


    // read each coil from the register map and put its value inside the outcoming message
    u8bitsno = 0;
    u8frameByte = 7;
    for (u16currentCoil = 0; u16currentCoil < u16Coilno; u16currentCoil++)
    {

        u16coil = u16StartCoil + u16currentCoil;
        u8currentRegister = (uint8_t) (u16coil / 16);
        u8currentBit = (uint8_t) (u16coil % 16);

        bTemp = bitRead(
                    au8Buffer[ u8frameByte ],
                    u8bitsno );

        bitWrite(
            regs[ u8currentRegister ],
            u8currentBit,
            bTemp );

        u8bitsno ++;

        if (u8bitsno > 7)
        {
            u8bitsno = 0;
            u8frameByte++;
        }
    }

    // send outcoming message
    // it's just a copy of the incomping frame until 6th byte
    u8BufferSize         = 6;
    u8CopyBufferSize = u8BufferSize +2;
    sendTxBuffer();
    return u8CopyBufferSize;
}

/**
 * @brief
 * This method processes function 16
 * This method writes a word array assigned by the master
 *
 * @return u8BufferSize Response to master length
 * @ingroup register
 */
int8_t Modbus::process_FC16( uint16_t *regs, uint8_t u8size )
{
    uint8_t u8func = au8Buffer[ FUNC ];  // get the original FUNC code
    uint8_t u8StartAdd = au8Buffer[ ADD_HI ] << 8 | au8Buffer[ ADD_LO ];
    uint8_t u8regsno = au8Buffer[ NB_HI ] << 8 | au8Buffer[ NB_LO ];
    uint8_t u8CopyBufferSize;
    uint8_t i;
    uint16_t temp;

    // build header
    au8Buffer[ NB_HI ]   = 0;
    au8Buffer[ NB_LO ]   = u8regsno;
    u8BufferSize         = RESPONSE_SIZE;

    // write registers
    for (i = 0; i < u8regsno; i++)
    {
        temp = word(
                   au8Buffer[ (BYTE_CNT + 1) + i * 2 ],
                   au8Buffer[ (BYTE_CNT + 2) + i * 2 ]);

        regs[ u8StartAdd + i ] = temp;
    }
    u8CopyBufferSize = u8BufferSize +2;
    sendTxBuffer();

    return u8CopyBufferSize;
}
