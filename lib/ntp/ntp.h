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
};

// NTP functions
void ntp_begin();
void ntp_begin(unsigned int port);
bool ntp_update();
bool ntp_forceUpdate();
bool ntp_isTimeSet();
String ntp_getFormattedTime();
unsigned long ntp_getEpochTime();
NTPTime ntp_get_time();
void ntp_setTimeOffset(int timeOffset);
void ntp_setUpdateInterval(unsigned long updateInterval);
void ntp_end();

// Initialization function
void init_ntp();

#endif // NTP_H