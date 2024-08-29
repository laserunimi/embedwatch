#include <inttypes.h>
#include <stdio.h>

typedef struct
{
    uint8_t u8id;           
    uint8_t u8fct;          
    uint16_t u16RegAdd;     
    uint16_t u16CoilsNo;    
    uint16_t *au16reg;      
}
modbus_t;

enum
{
    RESPONSE_SIZE = 6,
    EXCEPTION_SIZE = 3,
    CHECKSUM_SIZE = 2
};

enum MESSAGE
{
    ID                             = 0,  
    FUNC,  
    ADD_HI,  
    ADD_LO,  
    NB_HI,  
    NB_LO,  
    BYTE_CNT   
};

enum MB_FC
{
    MB_FC_NONE                     = 0,    
    MB_FC_READ_COILS               = 1,	 
    MB_FC_READ_DISCRETE_INPUT      = 2,	 
    MB_FC_READ_REGISTERS           = 3,	 
    MB_FC_READ_INPUT_REGISTER      = 4,	 
    MB_FC_WRITE_COIL               = 5,	 
    MB_FC_WRITE_REGISTER           = 6,	 
    MB_FC_WRITE_MULTIPLE_COILS     = 15,	 
    MB_FC_WRITE_MULTIPLE_REGISTERS = 16	 
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
#define  MAX_BUFFER  15

#define lowByte(w) ((uint8_t) ((w) & 0xff))
#define highByte(w) ((uint8_t) ((w) >> 8))
#define word(hi, low) (hi << 8 | low)

// Modbus Fields

uint8_t u8id;  
uint8_t u8serno;  
uint8_t u8txenpin;  
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

void bitWrite(uint8_t *x, uint8_t n, uint8_t value) {
    if(value == 1) {
        *x |= (1 << n);
    } else if(value == 0) {
        *x &= ~(1 << n);
    }
}

uint8_t bitRead(uint8_t *x, uint8_t n) {
    return ((*x) >> n) & 0x01;
}

// Modbus funcs

#define word(hi, low) (hi << 8 | low)

int8_t Modbus_process_FC1( uint16_t *regs, uint8_t u8size )
{

    printf("In FC1\n");

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

    
        au8Buffer[ u8BufferSize ] =  regs[ u8currentRegister ];
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
    return u8CopyBufferSize;
}

int8_t Modbus_process_FC3( uint16_t *regs, uint8_t u8size ){
    
    printf("In FC3\n");

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

    return u8CopyBufferSize;
}

int8_t Modbus_process_FC15( uint16_t *regs, uint8_t u8size )
{

    printf("In FC15\n");

    uint8_t u8currentRegister, u8currentBit, u8frameByte, u8bitsno;
    uint8_t u8CopyBufferSize;
    uint16_t u16currentCoil, u16coil;
    uint8_t bTemp;

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

        bTemp = au8Buffer[ u8frameByte ];

        
        regs[ u8currentRegister ] = bTemp;


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
    return u8CopyBufferSize;
}

int8_t Modbus_process_FC16( uint16_t *regs, uint8_t u8size ){
    
    printf("In FC16\n");

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

    return u8CopyBufferSize;
}

int8_t Modbus_poll(uint16_t *regs, uint8_t u8size){
  
  au16regs = regs;
  u8regsize = u8size;

  for(int i = 0; i < MAX_BUFFER; i++)
    au8Buffer[i] = fgetc(stdin);
  
  printf("%d",au8Buffer[FUNC]);
  
  switch( au8Buffer[ FUNC ] ) {
    case MB_FC_READ_COILS:
    case MB_FC_READ_DISCRETE_INPUT:
        return Modbus_process_FC1( regs, u8size );
        break;
    case MB_FC_READ_INPUT_REGISTER:
    case MB_FC_READ_REGISTERS :
        return Modbus_process_FC3( regs, u8size );
        break;
    case MB_FC_WRITE_MULTIPLE_COILS:
        return Modbus_process_FC15( regs, u8size );
        break;
    case MB_FC_WRITE_MULTIPLE_REGISTERS :
        return Modbus_process_FC16( regs, u8size );
        break;
    default:
        break;
  }

    return -1;
}