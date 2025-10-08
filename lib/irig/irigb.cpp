#include "irigb.h"

IRIGB::IRIGB(uint8_t outputPin) : outputPin(outputPin)
{
  currentTime = {0, 0, 0, 1, 23};
  lastFrameTime = 0;
  ntpTimeValid = false;
  requestSetTimeFlag = false;
  state = 0;
  bit_counter_marker = 0;
  for (int i = 0; i < 100; i++)
  {
    bits[i] = false;
  }
}

void IRIGB::begin()
{
  pinMode(outputPin, OUTPUT);
  digitalWrite(outputPin, LOW);
  enabled_flag = true;
}


bool IRIGB::update()
{
  bool isMarker = (bit_counter % 10 == 0) || (bit_counter == 1);
  bool val = use_buffer_0? bits_0[bit_counter]:bits_1[bit_counter];
  if (isMarker)
  {
    if (bit_counter_marker <= 7)
    {
      digitalWrite(outputPin, HIGH);
    }

    else
    {
      digitalWrite(outputPin, LOW);
    }
  }
  else if (val)
  {
    if (bit_counter_marker <= 4)
    {
      digitalWrite(outputPin, HIGH);
    }
    else
    {
      digitalWrite(outputPin, LOW);
    }
  }
  else
  {
    if (bit_counter_marker <= 1)
    {
      digitalWrite(outputPin, HIGH);
    }
    else
    {
      digitalWrite(outputPin, LOW);
    }
  }
  bit_counter_marker++;
  if (bit_counter_marker >= 10)
  {
    bit_counter_marker = 0;
    bit_counter++;
    if (bit_counter >= 100)
    {
      bit_counter = 0;
      use_buffer_0=!use_buffer_0;
      return true;
    }
  }
  return false;
}



void IRIGB::updateTimeFromEpoch(unsigned long epochTime)
{
  convertUnixTimeToIrigTime(epochTime, requestSetTime);
  requestSetTimeFlag = true;
  enable();
}

void IRIGB::convertUnixTimeToIrigTime(unsigned long unixTime, IrigTime &irigTime)
{
  time_t rawtime = unixTime;
  struct tm *timeinfo = gmtime(&rawtime);
  // Convert to IRIG-B time format
  irigTime.second = timeinfo->tm_sec;
  irigTime.minute = timeinfo->tm_min;
  irigTime.hour = timeinfo->tm_hour;
  irigTime.day = timeinfo->tm_yday + 1;   // tm_yday is 0-based, IRIG-B is 1-based
  irigTime.year = timeinfo->tm_year % 100; // Get last two digits of year
}

IrigTime IRIGB::getCurrentTime() const
{
  return currentTime;
}

void IRIGB::setCurrentTime(const IrigTime &time)
{
  currentTime = time;
}

void IRIGB::encodeTimeIntoBits(const IrigTime &time, int timeOffsetHours)
{
  if(use_buffer_0) this->encodeTimeIntoBits(time,this->bits_1, timeOffsetHours);
  else this->encodeTimeIntoBits(time,this->bits_0, timeOffsetHours);

  //for(int a=0;a<100;a++) bits[a]=use_buffer_0?bits_1[a]:bits_0[a];
}

void IRIGB::encodeTimeIntoBits(const IrigTime &time, bool *bits, int timeOffsetHours)
{
  this->encodeTimeIntoBits(time,bits,timeOffsetHours,TimeQuality::WITHIN_1_US,ContinuousTimeQuality::NOT_USED );
}


void IRIGB::encodeTimeIntoBits(const IrigTime &time, bool *bits, int timeOffsetHours, TimeQuality tq, ContinuousTimeQuality ctq)
{

  // Serial.printf("ENCODE %i\n",outputPin);
  // Initialize bits
  for (int i = 0; i < 100; i++) {
    bits[i] = false;
  }

  // Calculate Straight Binary Seconds
  uint32_t straight_binary_second = time.hour * 3600 + time.minute * 60 + time.second;
  if (straight_binary_second > 86399) {
    Serial.println("Error: Invalid straight binary seconds");
    return;
  }

  // Position reference markers
  for (int i = 0; i < 10; i++) {
    bits[i * 10] = true; // PR markers
  }
  bits[0] = true; // First PR marker
  bits[1] = true; // Second PR marker for frame start

  // Encode seconds (BCD) - Bits 2-9
  bits[2] = (time.second % 10) & 0x01;
  bits[3] = ((time.second % 10) & 0x02) >> 1;
  bits[4] = ((time.second % 10) & 0x04) >> 2;
  bits[5] = ((time.second % 10) & 0x08) >> 3;
  bits[6] = 0; // Unused
  bits[7] = (time.second / 10) & 0x01;
  bits[8] = ((time.second / 10) & 0x02) >> 1;
  bits[9] = ((time.second / 10) & 0x04) >> 2;

  // Encode minutes (BCD) - Bits 11-19
  bits[11] = (time.minute % 10) & 0x01;
  bits[12] = ((time.minute % 10) & 0x02) >> 1;
  bits[13] = ((time.minute % 10) & 0x04) >> 2;
  bits[14] = ((time.minute % 10) & 0x08) >> 3;
  bits[15] = 0; // Unused
  bits[16] = (time.minute / 10) & 0x01;
  bits[17] = ((time.minute / 10) & 0x02) >> 1;
  bits[18] = ((time.minute / 10) & 0x04) >> 2;
  bits[19] = 0; // Unused

  // Encode hours (BCD) - Bits 21-29
  bits[21] = (time.hour % 10) & 0x01;
  bits[22] = ((time.hour % 10) & 0x02) >> 1;
  bits[23] = ((time.hour % 10) & 0x04) >> 2;
  bits[24] = ((time.hour % 10) & 0x08) >> 3;
  bits[25] = 0; // Unused
  bits[26] = (time.hour / 10) & 0x01;
  bits[27] = ((time.hour / 10) & 0x02) >> 1;
  bits[28] = 0; // Unused
  bits[29] = 0; // Unused

  // Encode day of year (BCD) - Bits 31-39, 41-42
  bits[31] = (time.day % 10) & 0x01;
  bits[32] = ((time.day % 10) & 0x02) >> 1;
  bits[33] = ((time.day % 10) & 0x04) >> 2;
  bits[34] = ((time.day % 10) & 0x08) >> 3;
  bits[35] = 0; // Unused
  bits[36] = ((time.day / 10) % 10) & 0x01;
  bits[37] = (((time.day / 10) % 10) & 0x02) >> 1;
  bits[38] = (((time.day / 10) % 10) & 0x04) >> 2;
  bits[39] = (((time.day / 10) % 10) & 0x08) >> 3;
  bits[41] = (time.day / 100) & 0x01;
  bits[42] = ((time.day / 100) & 0x02) >> 1;

  // Encode year (BCD) - Bits 51-58
  bits[51] = (time.year % 10) & 0x01;
  bits[52] = ((time.year % 10) & 0x02) >> 1;
  bits[53] = ((time.year % 10) & 0x04) >> 2;
  bits[54] = ((time.year % 10) & 0x08) >> 3;
  bits[55] = 0; // Unused
  bits[56] = (time.year / 10) & 0x01;
  bits[57] = ((time.year / 10) & 0x02) >> 1;
  bits[58] = ((time.year / 10) & 0x04) >> 2;

  // Control bits (IEEE C37.118.1 extensions) - Bits 61-64
  bits[61] = 0; // Leap Second Pending (LSP)
  bits[62] = 0; // Leap Second (LS)
  bits[63] = 0; // Daylight Saving Pending (DSP)
  bits[64] = 0; // Daylight Savings Time (DST)

  // Encode Time Offset for WIB - Bits 65-69
  bool isNegative = timeOffsetHours < 0;
  uint8_t offsetHours = static_cast<uint8_t>(abs(timeOffsetHours));
  bits[65] = isNegative ? 1 : 0;   // Sign bit (0 for WIB)
  bits[66] = (offsetHours & 0x01); // BCD for 7
  bits[67] = (offsetHours & 0x02) >> 1;
  bits[68] = (offsetHours & 0x04) >> 2;
  bits[69] = (offsetHours & 0x08) >> 3;
  bits[71] = 0; // No 0.5-hour offset

  // Encode Time Quality (TQ) - Bits 72-75
  uint8_t tqValue = static_cast<uint8_t>(tq);
  bits[72] = (tqValue & 0x01);
  bits[73] = (tqValue & 0x02) >> 1;
  bits[74] = (tqValue & 0x04) >> 2;
  bits[75] = (tqValue & 0x08) >> 3;

  // Encode Continuous Time Quality (CTQ) - Bits 77-79
  uint8_t ctqValue = static_cast<uint8_t>(ctq);
  bits[77] = (ctqValue & 0x01);
  bits[78] = (ctqValue & 0x02) >> 1;
  bits[79] = (ctqValue & 0x04) >> 2;

  // Parity check - Bit 76 (even parity over bits 2-75)
  int count = 0;
  for (int i = 2; i <= 75; i++) {
    if (i % 10 != 0) {
      count += bits[i];
    }
  }
  bits[76] = (count % 2) ? 0 : 1;

  // Encode Straight Binary Seconds (SBS) - Bits 81-89, 91-98
  bits[81] = straight_binary_second & 0x01;
  bits[82] = (straight_binary_second & 0x02) >> 1;
  bits[83] = (straight_binary_second & 0x04) >> 2;
  bits[84] = (straight_binary_second & 0x08) >> 3;
  bits[85] = (straight_binary_second & 0x10) >> 4;
  bits[86] = (straight_binary_second & 0x20) >> 5;
  bits[87] = (straight_binary_second & 0x40) >> 6;
  bits[88] = (straight_binary_second & 0x80) >> 7;
  bits[89] = (straight_binary_second & 0x100) >> 8;
  bits[91] = (straight_binary_second & 0x200) >> 9;
  bits[92] = (straight_binary_second & 0x400) >> 10;
  bits[93] = (straight_binary_second & 0x800) >> 11;
  bits[94] = (straight_binary_second & 0x1000) >> 12;
  bits[95] = (straight_binary_second & 0x2000) >> 13;
  bits[96] = (straight_binary_second & 0x4000) >> 14;
  bits[97] = (straight_binary_second & 0x8000) >> 15;
  bits[98] = (straight_binary_second & 0x10000) >> 16;
  bits[99] = 0; // Unused

}