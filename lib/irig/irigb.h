#ifndef IRIGB_H
#define IRIGB_H

#include <Arduino.h>

// IRIG-B time structure
struct IrigTime {
  uint8_t second;   // 0-59
  uint8_t minute;   // 0-59
  uint8_t hour;     // 0-23
  uint16_t day;     // 1-366
  uint8_t year;      // 0-99
};


enum class TimeQuality : uint8_t {
    LOCKED_TO_UTC = 0,     // Clock is locked to a UTC traceable source
    WITHIN_1_NS = 1,       // Time is within < 1 ns of UTC
    WITHIN_10_NS = 2,      // Time is within < 10 ns of UTC
    WITHIN_100_NS = 3,     // Time is within < 100 ns of UTC
    WITHIN_1_US = 4,       // Time is within < 1 μs of UTC
    WITHIN_10_US = 5,      // Time is within < 10 μs of UTC
    WITHIN_100_US = 6,     // Time is within < 100 μs of UTC
    WITHIN_1_MS = 7,       // Time is within < 1 ms of UTC
    WITHIN_10_MS = 8,      // Time is within < 10 ms of UTC
    WITHIN_100_MS = 9,     // Time is within < 100 ms of UTC
    WITHIN_1_S = 10,       // Time is within < 1 s of UTC
    WITHIN_10_S = 11,      // Time is within < 10 s of UTC
    FAULT = 15             // Clock failure, time is not reliable
};

enum class ContinuousTimeQuality : uint8_t {
    NOT_USED = 0,          // Indicates code from previous version of standard
    ERROR_LT_100_NS = 1,   // Estimated maximum time error < 100 ns
    ERROR_LT_1_US = 2,     // Estimated maximum time error < 1 μs
    ERROR_LT_10_US = 3,    // Estimated maximum time error < 10 μs
    ERROR_LT_100_US = 4,   // Estimated maximum time error < 100 μs
    ERROR_LT_1_MS = 5,     // Estimated maximum time error < 1 ms
    ERROR_LT_10_MS = 6,    // Estimated maximum time error < 10 ms
    ERROR_GT_10_MS = 7     // Estimated maximum time error > 10 ms or unknown
};

class IRIGB {
public:
  // Constructor
  IRIGB(uint8_t outputPin);
  
  // Initialize the IRIG-B generator
  void begin();
  
  // // Initialize WiFi and NTP
  // void beginWiFi(const char* ssid, const char* password);

  // Update and output a frame
  bool update(uint8_t bit_counter);
  void enable(){enabled_flag  = true;}
  void disable(){enabled_flag = false;}
  
  
  // Get current time
  IrigTime getCurrentTime() const;
  // Set current time
  void setCurrentTime(const IrigTime& time);
  void updateTimeFromEpoch(unsigned long epochTime);
  void convertUnixTimeToIrigTime(unsigned long unixTime, IrigTime& irigTime);

// private:
  // Timing parameters for IRIG-B
  static const unsigned long FRAME_DURATION_MS = 1000; // 1 second frame
  static const unsigned long P0_DURATION_US = 2000;    // 2ms pulse width for '0'
  static const unsigned long P1_DURATION_US = 5000;    // 5ms pulse width for '1'
  static const unsigned long PM_DURATION_US = 8000;    // 8ms pulse width for marker
  bool enabled_flag = true;
  uint8_t outputPin;

  IrigTime currentTime;
  unsigned long lastFrameTime;
  bool ntpTimeValid;
  bool enable_output = true;
  IrigTime requestSetTime;
  bool requestSetTimeFlag;
  bool bits[100];
  bool bits_0[100];
  bool bits_1[100];
  int bit_counter=0;
  bool use_buffer_0=true;
  uint8_t state=0;
  uint8_t bit_counter_marker=0;
   
  void encodeTimeIntoBits(const IrigTime& time, int timeOffsetHours);
  void encodeTimeIntoBits(const IrigTime& time, bool* bits, int timeOffsetHours);
  void encodeTimeIntoBits(const IrigTime &time,bool* bits, int timeOffsetHours, TimeQuality tq, ContinuousTimeQuality ctq);
  private:
};

#endif // IRIGB_H 