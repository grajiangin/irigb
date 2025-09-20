#ifndef IRIGB_H
#define IRIGB_H

#include <Arduino.h>


#define FORMAT_B000_B120 0
#define FORMAT_B001_B121 1
#define FORMAT_B002_B122 2
#define FORMAT_B003_B123 3
#define FORMAT_B004_B124 4
#define FORMAT_B005_B125 5
#define FORMAT_B006_B126 6
#define FORMAT_B007_B127 7

// IRIG-B time structure
struct IrigTime {
  uint8_t seconds;   // 0-59
  uint8_t minutes;   // 0-59
  uint8_t hours;     // 0-23
  uint16_t days;     // 1-366
  uint8_t year;      // 0-99
};

void encodeTimeIntoBits(  bool *bits, const IrigTime &time,int irig_format);

  


#endif // IRIGB_H 