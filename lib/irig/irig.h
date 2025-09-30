#ifndef IRIGB_H
#define IRIGB_H

#include <Arduino.h>
#include "ntp.h"


#define FORMAT_B000_B120 0
#define FORMAT_B001_B121 1
#define FORMAT_B002_B122 2
#define FORMAT_B003_B123 3
#define FORMAT_B004_B124 4
#define FORMAT_B005_B125 5
#define FORMAT_B006_B126 6
#define FORMAT_B007_B127 7
#define FORMAT_DISABLED 8

void encodeTimeIntoBits(  uint8_t *bits, const NTPTime &time,int irig_format);

void convert_bit(bool *bits, const NTPTime &time);


#endif // IRIGB_H 