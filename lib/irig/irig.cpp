#include "irigb.h"

IRIGB::IRIGB(uint8_t outputPin) : outputPin(outputPin)
{
  currentTime = {0, 0, 0, 1, 23};
  lastFrameTime = 0;
  ntpTimeValid = false;
  requestSetTimeFlag = false;
  bit_counter = 0;
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

void IRIGB::update()
{
  if (!enable_output)
  {
    if (enabled_flag)
    {
      enable_output = true;
      if (requestSetTimeFlag)
      {
        requestSetTimeFlag = false;
        currentTime = requestSetTime;
        encodeTimeIntoBits(currentTime);
      }
    }
    else
    {
      return;
    }
  }

  bool isMarker = (bit_counter % 10 == 0) || (bit_counter == 1);
  bool val = bits[bit_counter];

  if (isMarker)
  {
    if (bit_counter_marker <= 7)
      digitalWrite(outputPin, HIGH);
    else
      digitalWrite(outputPin, LOW);
  }
  else if (val)
  {
    if (bit_counter_marker <= 4)
      digitalWrite(outputPin, HIGH);
    else
      digitalWrite(outputPin, LOW);
  }
  else
  {
    if (bit_counter_marker <= 1)
      digitalWrite(outputPin, HIGH);
    else
      digitalWrite(outputPin, LOW);
  }

  bit_counter_marker++;
  if (bit_counter_marker >= 10)
  {
    bit_counter_marker = 0;
    bit_counter++;
    if (bit_counter >= 100)
    {
      bit_counter = 0;
      if (!enabled_flag)
      {
        enable_output=false;
      }
      if (requestSetTimeFlag)
      {
        requestSetTimeFlag = false;
        currentTime = requestSetTime;
        encodeTimeIntoBits(currentTime);
      }
      
    }
  }
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
  irigTime.seconds = timeinfo->tm_sec;
  irigTime.minutes = timeinfo->tm_min;
  irigTime.hours = timeinfo->tm_hour;
  irigTime.days = timeinfo->tm_yday + 1;   // tm_yday is 0-based, IRIG-B is 1-based
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

void IRIGB::encodeTimeIntoBits(  bool *bits, const IrigTime &time,int irig_format)
{
  // Initialize all bits to false
  uint32_t control_function=0;
  uint32_t straight_binary_second  = time.hours*3600+time.minutes*60+time.seconds;
  for (int i = 0; i < 100; i++)
  {
    bits[i] = false;
  }

  // Position reference markers (every 10 bits)
  for (int i = 0; i < 10; i++)
  {
    bits[i * 10] = true; // PR markers
  }

  // Add a second PR marker at the beginning (Frame reference marker)
  bits[0] = true; // First PR marker (already set above)
  bits[1] = true; // Second PR marker to indicate frame start

  // Encode seconds (BCD) - Bits 2-9 (shifted due to second PR marker)
  bits[2] = (time.seconds % 10) & 0x01;
  bits[3] = (time.seconds % 10) & 0x02;
  bits[4] = (time.seconds % 10) & 0x04;
  bits[5] = (time.seconds % 10) & 0x08;
  bits[6] = 0; // Unused
  bits[7] = (time.seconds / 10) & 0x01;
  bits[8] = (time.seconds / 10) & 0x02;
  bits[9] = (time.seconds / 10) & 0x04;

  // Encode minutes (BCD) - Bits 11-19
  bits[11] = (time.minutes % 10) & 0x01;
  bits[12] = (time.minutes % 10) & 0x02;
  bits[13] = (time.minutes % 10) & 0x04;
  bits[14] = (time.minutes % 10) & 0x08;
  bits[15] = 0; // Unused
  bits[16] = (time.minutes / 10) & 0x01;
  bits[17] = (time.minutes / 10) & 0x02;
  bits[18] = (time.minutes / 10) & 0x04;
  bits[19] = 0; // Unused

  // Encode hours (BCD) - Bits 21-29
  bits[21] = (time.hours % 10) & 0x01;
  bits[22] = (time.hours % 10) & 0x02;
  bits[23] = (time.hours % 10) & 0x04;
  bits[24] = (time.hours % 10) & 0x08;
  bits[25] = 0; // Unused
  bits[26] = (time.hours / 10) & 0x01;
  bits[27] = (time.hours / 10) & 0x02;
  bits[28] = 0; // Unused
  bits[29] = 0; // Unused

  // Encode day of year (BCD) - Bits 31-39
  bits[31] = (time.days % 10) & 0x01;
  bits[32] = (time.days % 10) & 0x02;
  bits[33] = (time.days % 10) & 0x04;
  bits[34] = (time.days % 10) & 0x08;
  bits[35] = ((time.days / 10) % 10) & 0x01;
  bits[36] = ((time.days / 10) % 10) & 0x02;
  bits[37] = ((time.days / 10) % 10) & 0x04;
  bits[38] = ((time.days / 10) % 10) & 0x08;
  bits[39] = (time.days / 100) & 0x01;

  switch(irig_format) //Bits 51-59
	{
		case FORMAT_B000_B120:
		case FORMAT_B001_B121:
 /*           // Encode control function - Bits 51-59
  /*            bits[51] = control_function & 1<<0;//0
            bits[52] = control_function & 1<<1;//1
            bits[53] = control_function & 1<<2;//2
            bits[54] = control_function & 1<<3;//3
            bits[55] = control_function & 1<<4;//4
            bits[56] = control_function & 1<<5;//5
            bits[57] = control_function & 1<<6;//6
            bits[58] = control_function & 1<<7;//7
            bits[59] = control_function & 1<<8;//8
*/
            break;
        case FORMAT_B002_B122:
		case FORMAT_B003_B123:
            //kosong
            break;
        case FORMAT_B004_B124:
		case FORMAT_B005_B125:
        case FORMAT_B006_B126:
		case FORMAT_B007_B127:
            // Encode year (BCD) - Bits 50-58
            bits[50] = (time.year % 10) & 0x01;
            bits[51] = (time.year % 10) & 0x02;
            bits[52] = (time.year % 10) & 0x04;
            bits[53] = (time.year % 10) & 0x08;
            bits[54] = 0; // Unused
            bits[55] = (time.year / 10) & 0x01;
            bits[56] = (time.year / 10) & 0x02;
            bits[57] = (time.year / 10) & 0x04;
            bits[58] = (time.year / 10) & 0x08;

            break;
        default:
            break;
  // Rest of the frame (59-99) can be used for additional data or left as 0
switch(irig_format) //Bits 61-79 control function
	{
        case FORMAT_B000_B120:
		case FORMAT_B001_B121:
        case FORMAT_B004_B124:
		case FORMAT_B005_B125:
            /*****set for IRIG B 004 */
            /*****tested for siemens swt3000, dimat tpu-1 */
            // Table 5-1. Overview of the Control Bits Added in the IEEE Specifications
            bits[61] = false;//active Leap Second Pending (LSP)—This field becomes a 1 up to 59 seconds before a leap is inserted ordeleted. Then, it returns to 0 after the event.
            bits[62] = false; //Leap Second (LS) —0 = Add a Second (most common) and 1 = Subtract a Second
            bits[63] = false;//Daylight Saving Pending (DSP)—This field becomes a 1 up to 59 seconds before a DST event.Returns to 0 after the event.
            bits[64] = false;//Daylight Savings Time (DST) —Becomes 1 during DST.

            bits[65] = false; // Time zone offset sign (0=+, 1=−)
            bits[66] = true; //Bits 66-69 Time Offset —This is the offset from the IRIG-B time to UTC time that is, the local time offset (+12 hr for NZ). Taking this offset, and the IRIG time you can get the UTC time. That is, IRIG time – 12 hr = UTC tim

            bits[67] = true;
            bits[68] = true;
            
            bits[69] = false;

            bits[71] = false; //Time offset 0.5 hours —0 = No offset and 1 = 0.5-hour offset
            bits[72] = false; // Bits 72-75 Time Quality bit—This is a 4-bit code representation of the approximate clock time error from  UTC. See Table 5-2 for the full range of values.
            bits[73] = false;
            bits[74] = false;
            bits[75] = false;
            int count=0;
            for (int i = 2; i <= 75; i++)
            {
                if(i%10!=0)
                    if(bits[i])count++;
            }
            if(count%2)bits[76] = false;
            else bits[76] = true; // Even parity of bits 2−75 Parity—This is the parity for preceding bits. Acts as a check to ensure that the preceding data makes sense. The parity bit ensures that an even parity is generated.The parity bit is even parity over all data bits from 1 through 74. Marker bits are ignored (or, equivalently, read as 0).
            bits[77] = false; // Bits 77-79  Continuous Time Quality—This is a 3-bit code representation of the estimated time error in the transmitted message. See Table 5-3 for the full range of values.
            bits[78] = false;
            bits[79] = false;
                break;
        default:
        break;
    }

switch(irig_format)//Bits 81-99 control function
	{
		case FORMAT_B000_B120:
		case FORMAT_B003_B123:
		case FORMAT_B004_B124:
		case FORMAT_B007_B127:
            bits[81] = straight_binary_second &0x01;
            bits[82] = straight_binary_second &0x02;
            bits[83] = straight_binary_second &0x04;
            bits[84] = straight_binary_second &0x08;
            bits[85] = straight_binary_second &0x10;
            bits[86] = straight_binary_second &0x20;
            bits[87] = straight_binary_second &0x40;
            bits[88] = straight_binary_second &0x80;
            bits[89] = straight_binary_second &0x100;

            bits[91] = straight_binary_second &0x200;
            bits[92] = straight_binary_second &0x400;
            bits[93] = straight_binary_second &0x800;
            bits[94] = straight_binary_second &0x1000;
            bits[95] = straight_binary_second &0x2000;
            bits[96] = straight_binary_second &0x4000;
            bits[97] = straight_binary_second &0x8000;
            bits[98] = straight_binary_second &0x10000;
            bits[99] = 0;
            break;
        default:
          
            break;
    }
}
