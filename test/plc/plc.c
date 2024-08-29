/**
 *  Modbus slave example 1:
 *  The purpose of this example is to link a data array
 *  from the Arduino to an external device.
 *
 *  Recommended Modbus Master: QModbus
 *  http://qmodbus.sourceforge.net/
 */

#include "ZModbus.h"


// data array for modbus network sharing
uint16_t au16data[16] = {
  3, 1415, 9265, 4, 2, 7182, 28182, 8, 0, 0, 0, 0, 0, 0, 1, -1 };

/**
 *  Modbus object declaration
 *  u8id : node id = 0 for master, = 1..247 for slave
 *  u8serno : serial port (use 0 for Serial)
 *  u8txenpin : 0 for RS-232 and USB-FTDI 
 *               or any pin number > 1 for RS-485
 */


int main() {
    while(1)
      Modbus_poll(au16data,16);
}
