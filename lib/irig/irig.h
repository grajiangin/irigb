#ifndef IRIGB_H
#define IRIGB_H

#include <Arduino.h>
#include <WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

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

class IRIGB {
public:
  // Constructor
  IRIGB(uint8_t outputPin);
  
  // Initialize the IRIG-B generator
  void begin();
  
  // // Initialize WiFi and NTP
  // void beginWiFi(const char* ssid, const char* password);

  // Update and output a frame
  void update();
  void enable(){enabled_flag  = true;}
  void disable(){enabled_flag = false;}
  
  
  // Get current time
  IrigTime getCurrentTime() const;
  // Set current time
  void setCurrentTime(const IrigTime& time);
  void updateTimeFromEpoch(unsigned long epochTime);
  void convertUnixTimeToIrigTime(unsigned long unixTime, IrigTime& irigTime);

private:
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
  int bit_counter=0;
  uint8_t state=0;
  uint8_t bit_counter_marker=0;

  void encodeTimeIntoBits(const IrigTime& time);
  
};

#endif // IRIGB_H 