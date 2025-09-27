#include "irig.h"
#include "ntp.h" // NTPTime struct definition

void encodeTimeIntoBits(uint8_t *bits, const NTPTime &time, int irig_format)
{
    // Initialize all bits to 0 (logical 0)
    uint32_t control_function = 0;
    uint32_t straight_binary_second = time.hour * 3600 + time.minute * 60 + time.second;
    for (int i = 0; i < 100; i++)
    {
        bits[i] = 0;
    }

    // Position reference markers (every 10 bits)
    for (int i = 0; i < 10; i++)
    {
        bits[i * 10] = 2; // PR markers
    }

    // Add a second PR marker at the beginning (Frame reference marker)
    bits[0] = 2; // First PR marker (already set above)
    bits[1] = 2; // Second PR marker to indicate frame start

    // Encode seconds (BCD) - Bits 2-9 (shifted due to second PR marker)
    bits[2] = ((time.second % 10) & 0x01) ? 1 : 0;
    bits[3] = ((time.second % 10) & 0x02) ? 1 : 0;
    bits[4] = ((time.second % 10) & 0x04) ? 1 : 0;
    bits[5] = ((time.second % 10) & 0x08) ? 1 : 0;
    bits[6] = 0; // Unused
    bits[7] = ((time.second / 10) & 0x01) ? 1 : 0;
    bits[8] = ((time.second / 10) & 0x02) ? 1 : 0;
    bits[9] = ((time.second / 10) & 0x04) ? 1 : 0;

    // Encode minutes (BCD) - Bits 11-19
    bits[11] = ((time.minute % 10) & 0x01) ? 1 : 0;
    bits[12] = ((time.minute % 10) & 0x02) ? 1 : 0;
    bits[13] = ((time.minute % 10) & 0x04) ? 1 : 0;
    bits[14] = ((time.minute % 10) & 0x08) ? 1 : 0;
    bits[15] = 0; // Unused
    bits[16] = ((time.minute / 10) & 0x01) ? 1 : 0;
    bits[17] = ((time.minute / 10) & 0x02) ? 1 : 0;
    bits[18] = ((time.minute / 10) & 0x04) ? 1 : 0;
    bits[19] = 0; // Unused

    // Encode hours (BCD) - Bits 21-29
    bits[21] = ((time.hour % 10) & 0x01) ? 1 : 0;
    bits[22] = ((time.hour % 10) & 0x02) ? 1 : 0;
    bits[23] = ((time.hour % 10) & 0x04) ? 1 : 0;
    bits[24] = ((time.hour % 10) & 0x08) ? 1 : 0;
    bits[25] = 0; // Unused
    bits[26] = ((time.hour / 10) & 0x01) ? 1 : 0;
    bits[27] = ((time.hour / 10) & 0x02) ? 1 : 0;
    bits[28] = 0; // Unused
    bits[29] = 0; // Unused

    // Encode day of year (BCD) - Bits 31-39
    bits[31] = ((time.day % 10) & 0x01) ? 1 : 0;
    bits[32] = ((time.day % 10) & 0x02) ? 1 : 0;
    bits[33] = ((time.day % 10) & 0x04) ? 1 : 0;
    bits[34] = ((time.day % 10) & 0x08) ? 1 : 0;
    bits[35] = (((time.day / 10) % 10) & 0x01) ? 1 : 0;
    bits[36] = (((time.day / 10) % 10) & 0x02) ? 1 : 0;
    bits[37] = (((time.day / 10) % 10) & 0x04) ? 1 : 0;
    bits[38] = (((time.day / 10) % 10) & 0x08) ? 1 : 0;
    bits[39] = ((time.day / 100) & 0x01) ? 1 : 0;

    switch (irig_format) // Bits 51-59
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
        // kosong
        break;
    case FORMAT_B004_B124:
    case FORMAT_B005_B125:
    case FORMAT_B006_B126:
    case FORMAT_B007_B127:
        // Encode year (BCD) - Bits 50-58
        bits[50] = ((time.year % 10) & 0x01) ? 1 : 0;
        bits[51] = ((time.year % 10) & 0x02) ? 1 : 0;
        bits[52] = ((time.year % 10) & 0x04) ? 1 : 0;
        bits[53] = ((time.year % 10) & 0x08) ? 1 : 0;
        bits[54] = 0; // Unused
        bits[55] = ((time.year / 10) & 0x01) ? 1 : 0;
        bits[56] = ((time.year / 10) & 0x02) ? 1 : 0;
        bits[57] = ((time.year / 10) & 0x04) ? 1 : 0;
        bits[58] = ((time.year / 10) & 0x08) ? 1 : 0;

        break;
    default:
        break;
        // Rest of the frame (59-99) can be used for additional data or left as 0
        switch (irig_format) // Bits 61-79 control function
        {
        case FORMAT_B000_B120:
        case FORMAT_B001_B121:
        case FORMAT_B004_B124:
        case FORMAT_B005_B125:
        {
            /*****set for IRIG B 004 */
            /*****tested for siemens swt3000, dimat tpu-1 */
            // Table 5-1. Overview of the Control Bits Added in the IEEE Specifications
            bits[61] = 0; // active Leap Second Pending (LSP)—This field becomes a 1 up to 59 seconds before a leap is inserted ordeleted. Then, it returns to 0 after the event.
            bits[62] = 0; // Leap Second (LS) —0 = Add a Second (most common) and 1 = Subtract a Second
            bits[63] = 0; // Daylight Saving Pending (DSP)—This field becomes a 1 up to 59 seconds before a DST event.Returns to 0 after the event.
            bits[64] = 0; // Daylight Savings Time (DST) —Becomes 1 during DST.

            bits[65] = 0; // Time zone offset sign (0=+, 1=−)
            bits[66] = 1; // Bits 66-69 Time Offset —This is the offset from the IRIG-B time to UTC time that is, the local time offset (+12 hr for NZ). Taking this offset, and the IRIG time you can get the UTC time. That is, IRIG time – 12 hr = UTC tim

            bits[67] = 1;
            bits[68] = 1;

            bits[69] = 0;

            bits[71] = 0; // Time offset 0.5 hours —0 = No offset and 1 = 0.5-hour offset
            bits[72] = 0; // Bits 72-75 Time Quality bit—This is a 4-bit code representation of the approximate clock time error from  UTC. See Table 5-2 for the full range of values.
            bits[73] = 0;
            bits[74] = 0;
            bits[75] = 0;
            int count = 0;
            for (int i = 2; i <= 75; i++)
            {
                if (i % 10 != 0)
                    if (bits[i])
                        count++;
            }
            if (count % 2)
                bits[76] = 0;
            else
                bits[76] = 1; // Even parity of bits 2−75 Parity—This is the parity for preceding bits. Acts as a check to ensure that the preceding data makes sense. The parity bit ensures that an even parity is generated.The parity bit is even parity over all data bits from 1 through 74. Marker bits are ignored (or, equivalently, read as 0).
            bits[77] = 0;     // Bits 77-79  Continuous Time Quality—This is a 3-bit code representation of the estimated time error in the transmitted message. See Table 5-3 for the full range of values.
            bits[78] = 0;
            bits[79] = 0;
            break;
        }
        default:
            break;
        }
        switch (irig_format) // Bits 81-99 control function
        {
        case FORMAT_B000_B120:
        case FORMAT_B003_B123:
        case FORMAT_B004_B124:
        case FORMAT_B007_B127:
            bits[81] = (straight_binary_second & 0x01) ? 1 : 0;
            bits[82] = (straight_binary_second & 0x02) ? 1 : 0;
            bits[83] = (straight_binary_second & 0x04) ? 1 : 0;
            bits[84] = (straight_binary_second & 0x08) ? 1 : 0;
            bits[85] = (straight_binary_second & 0x10) ? 1 : 0;
            bits[86] = (straight_binary_second & 0x20) ? 1 : 0;
            bits[87] = (straight_binary_second & 0x40) ? 1 : 0;
            bits[88] = (straight_binary_second & 0x80) ? 1 : 0;
            bits[89] = (straight_binary_second & 0x100) ? 1 : 0;

            bits[91] = (straight_binary_second & 0x200) ? 1 : 0;
            bits[92] = (straight_binary_second & 0x400) ? 1 : 0;
            bits[93] = (straight_binary_second & 0x800) ? 1 : 0;
            bits[94] = (straight_binary_second & 0x1000) ? 1 : 0;
            bits[95] = (straight_binary_second & 0x2000) ? 1 : 0;
            bits[96] = (straight_binary_second & 0x4000) ? 1 : 0;
            bits[97] = (straight_binary_second & 0x8000) ? 1 : 0;
            bits[98] = (straight_binary_second & 0x10000) ? 1 : 0;
            bits[99] = 0;
            break;
        default:

            break;
        }
    }
}
