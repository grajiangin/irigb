#include "ntp.h"
#include <Arduino.h>
#include <string.h>

// Global NTP variables
WiFiUDP *ntpUDP;
bool _udpSetup = false;
const char* _poolServerName = "time.google.com"; // Changed from default pool.ntp.org
IPAddress _poolServerIP;
unsigned int _port = NTP_DEFAULT_LOCAL_PORT;
long _timeOffset = 3600; // 1 hour offset
unsigned long _updateInterval = 60000; // 60 seconds
unsigned long _currentEpoc = 0;
unsigned long _lastUpdate = 0;
byte _packetBuffer[NTP_PACKET_SIZE];

void sendNTPPacket() {
    // set all bytes in the buffer to 0
    memset(_packetBuffer, 0, NTP_PACKET_SIZE);
    // Initialize values needed to form NTP request
    _packetBuffer[0] = 0b11100011;   // LI, Version, Mode
    _packetBuffer[1] = 0;     // Stratum, or type of clock
    _packetBuffer[2] = 6;     // Polling Interval
    _packetBuffer[3] = 0xEC;  // Peer Clock Precision
    // 8 bytes of zero for Root Delay & Root Dispersion
    _packetBuffer[12]  = 49;
    _packetBuffer[13]  = 0x4E;
    _packetBuffer[14]  = 49;
    _packetBuffer[15]  = 52;

    // all NTP fields have been given values, now
    // you can send a packet requesting a timestamp:
    ntpUDP->beginPacket(_poolServerName, 123);
    ntpUDP->write(_packetBuffer, NTP_PACKET_SIZE);
    ntpUDP->endPacket();
}

void ntp_begin() {
    ntp_begin(NTP_DEFAULT_LOCAL_PORT);
}

void ntp_begin(unsigned int port) {
    _port = port;
    ntpUDP->begin(_port);
    _udpSetup = true;
}

bool ntp_forceUpdate() {
    // flush any existing packets
    while(ntpUDP->parsePacket() != 0)
        ntpUDP->flush();

    sendNTPPacket();

    // Wait till data is there or timeout...
    byte timeout = 0;
    int cb = 0;
    do {
        delay(10);
        cb = ntpUDP->parsePacket();
        if (timeout > 100) return false; // timeout after 1000 ms
        timeout++;
    } while (cb == 0);

    _lastUpdate = millis() - (10 * (timeout + 1)); // Account for delay in reading the time

    ntpUDP->read(_packetBuffer, NTP_PACKET_SIZE);

    unsigned long highWord = word(_packetBuffer[40], _packetBuffer[41]);
    unsigned long lowWord = word(_packetBuffer[42], _packetBuffer[43]);
    // combine the four bytes (two words) into a long integer
    // this is NTP time (seconds since Jan 1 1900):
    unsigned long secsSince1900 = highWord << 16 | lowWord;

    _currentEpoc = secsSince1900 - SEVENZYYEARS;

    return true;  // return true after successful update
}

bool ntp_update() {
    if ((millis() - _lastUpdate >= _updateInterval)     // Update after _updateInterval
        || _lastUpdate == 0) {                          // Update if there was no update yet.
        if (!_udpSetup || _port != NTP_DEFAULT_LOCAL_PORT) ntp_begin(_port); // setup the UDP client if needed
        return ntp_forceUpdate();
    }
    return false;   // return false if update does not occur
}

bool ntp_isTimeSet() {
    return (_lastUpdate != 0); // returns true if the time has been set, else false
}

unsigned long ntp_getEpochTime() {
    return _timeOffset + // User offset
           _currentEpoc + // Epoch returned by the NTP server
           ((millis() - _lastUpdate) / 1000); // Time since last update
}

String ntp_getFormattedTime() {
    unsigned long rawTime = ntp_getEpochTime();
    unsigned long hours = (rawTime % 86400L) / 3600;
    String hoursStr = hours < 10 ? "0" + String(hours) : String(hours);

    unsigned long minutes = (rawTime % 3600) / 60;
    String minuteStr = minutes < 10 ? "0" + String(minutes) : String(minutes);

    unsigned long seconds = rawTime % 60;
    String secondStr = seconds < 10 ? "0" + String(seconds) : String(seconds);

    return hoursStr + ":" + minuteStr + ":" + secondStr;
}



void ntp_end() {
    ntpUDP->stop();
    _udpSetup = false;
}

void ntp_setTimeOffset(int timeOffset) {
    _timeOffset = timeOffset;
}

void ntp_setUpdateInterval(unsigned long updateInterval) {
    _updateInterval = updateInterval;
}

// Calculate day of year (1-366) from epoch time
int getDayOfYear(unsigned long epochTime) {
    // Adjust for timezone offset to get local time
    unsigned long localTime = epochTime;

    // Calculate year
    unsigned long days = localTime / 86400UL;
    int year = 1970;
    while (days >= 365) {
        if ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0) {
            if (days >= 366) {
                days -= 366;
                year++;
            } else {
                break;
            }
        } else {
            days -= 365;
            year++;
        }
    }

    // Calculate day of year (1-based)
    int dayOfYear = days + 1;

    // Adjust for leap year if February hasn't passed yet
    bool isLeapYear = (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
    if (!isLeapYear && dayOfYear > 59) {
        dayOfYear--; // Non-leap year, February has 28 days
    }

    return dayOfYear;
}

NTPTime ntp_get_time() {
    NTPTime timeStruct;

    // Get current time components
    unsigned long epochTime = ntp_getEpochTime();
    unsigned long rawTime = epochTime;

    // Calculate day of year
    timeStruct.day = getDayOfYear(epochTime);

    // Calculate hours, minutes, seconds
    timeStruct.hour = (rawTime % 86400L) / 3600;
    timeStruct.minute = (rawTime % 3600) / 60;
    timeStruct.second = rawTime % 60;

    return timeStruct;
}

extern void ntp_hanlder( NTPTime time);
void ntp_task(void *param) {
    for (;;) {
        if (ntp_update()) {
            Serial.print("NTP Time: ");
            Serial.println(ntp_getFormattedTime());

            // Get current time components using the new function
            NTPTime currentTime = ntp_get_time();

            ntp_hanlder(currentTime);
        }
        delay(1000);
    }
}

void init_ntp() {
    Serial.println("Initializing NTP...");
    ntpUDP = new WiFiUDP();
    ntp_begin();
    ntp_setUpdateInterval(5000);
    ntp_setTimeOffset(7*3600);
    // Create a FreeRTOS task to periodically update NTP time
    xTaskCreate(
        ntp_task,
        "ntp_task",      // Task name
        4096,            // Stack size
        nullptr,         // Parameter
        1,               // Priority
        nullptr          // Task handle
    );
}