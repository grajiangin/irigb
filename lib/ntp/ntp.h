#ifndef NTP_H
#define NTP_H

#include <WiFiUdp.h>

#define SEVENZYYEARS 2208988800UL
#define NTP_PACKET_SIZE 48
#define NTP_DEFAULT_LOCAL_PORT 1337

// NTP time structure
struct NTPTime {
    int day;      // Day of year (1-366)
    int hour;     // Hour (0-23)
    int minute;   // Minute (0-59)
    int second;   // Second (0-59)
    int millisecond; // Millisecond (0-999)
    int year;     // Year (e.g., 2024)
};

// NTP functions
void ntp_begin();
void ntp_begin(unsigned int port);
bool ntp_update();
bool ntp_forceUpdate(unsigned int serverPort);
bool ntp_isTimeSet();
unsigned long ntp_getEpochTime();
NTPTime ntp_get_time();
String ntp_getCurrentServer();
void ntp_setTimeOffset(int timeOffset);
void ntp_setUpdateInterval(unsigned long updateInterval);
void ntp_end();

// Initialization function
void init_ntp();

#endif // NTP_H