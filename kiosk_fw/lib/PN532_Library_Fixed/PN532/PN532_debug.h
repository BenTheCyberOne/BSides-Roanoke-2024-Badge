#ifndef __DEBUG_H__
#define __DEBUG_H__

#define DEBUG

#include "Arduino.h"

//#ifdef ARDUINO_SAMD_VARIANT_COMPLIANCE
  //  #define SERIAL SerialUSB
//#else
   // #define SERIAL Serial
//#endif

#ifdef DEBUG
#define DMSG(args...) delay(1);
#define DMSG_STR(str) delay(1);
#define DMSG_HEX(num) delay(1);
#define DMSG_INT(num) delay(1);
#else
#define DMSG(args...)       Serial.print(args)
#define DMSG_STR(str)       Serial.println(str)
#define DMSG_HEX(num)       Serial.print(' '); Serial.print((num>>4)&0x0F, HEX); Serial.print(num&0x0F, HEX)
#define DMSG_INT(num)       Serial.print(' '); Serial.print(num)
#endif

#endif
