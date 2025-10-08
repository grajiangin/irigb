#include "ntp.h"
#include <Arduino.h>
#include <string.h>
#include "settings.h"

// Global NTP variables
WiFiUDP *ntpUDP;
bool _udpSetup = false;
String _poolServerName = ""; // Will be set from settings
IPAddress _poolServerIP;
unsigned int _port = NTP_DEFAULT_LOCAL_PORT;
unsigned int _serverPort = 123; // NTP server port, will be set from settings
long _timeOffset = 3600; // Will be set from settings
unsigned long _updateInterval = 60000; // 60 seconds
unsigned long _currentEpoc = 0;
unsigned long _lastUpdate = 0;
unsigned long _currentMilliseconds = 0;
byte _packetBuffer[NTP_PACKET_SIZE];
extern Settings settings;

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
    ntpUDP->beginPacket(_poolServerName.c_str(), _serverPort);
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
    // Debug: print current NTP server and port from settings before sending request
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

    // Extract fractional seconds (milliseconds)
    unsigned long fracHighWord = word(_packetBuffer[44], _packetBuffer[45]);
    unsigned long fracLowWord = word(_packetBuffer[46], _packetBuffer[47]);
    unsigned long fracSeconds = fracHighWord << 16 | fracLowWord;
    
    // Convert fractional seconds to milliseconds (multiply by 1000 and divide by 2^32)
    _currentMilliseconds = (fracSeconds * 1000ULL) >> 32;

    _currentEpoc = secsSince1900 - SEVENZYYEARS;
    return true;  // return true after successful update
}

bool ntp_update() {
    if ((millis() - _lastUpdate >= _updateInterval)     // Update after _updateInterval
        || _lastUpdate == 0) {  
        _timeOffset=settings.ntp.timeOffset; 
        _port=settings.ntp.port;
        _poolServerName=settings.ntp.server;                        // Update if there was no update yet.
        if (!_udpSetup || _port != NTP_DEFAULT_LOCAL_PORT) ntp_begin(_port); // setup the UDP client if needed
        return ntp_forceUpdate();
    }
    return false;   // return false if update does not occur
}

bool ntp_isTimeSet() {
    return (_lastUpdate != 0); // returns true if the time has been set, else false
}

unsigned long ntp_getEpochTime() {
    _timeOffset=settings.ntp.timeOffset;
    return (_timeOffset *3600) + // User offset
           _currentEpoc + // Epoch returned by the NTP server
           ((millis() - _lastUpdate) / 1000); // Time since last update
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


NTPTime ntp_get_time() {
    NTPTime timeStruct = {0}; // Initialize to zero
    unsigned long epochTime = ntp_getEpochTime();
    // Extract seconds, minutes, hours
    timeStruct.second = epochTime % 60;
    unsigned long remainingSeconds = epochTime / 60; // Minutes + hours + days
    timeStruct.minute = remainingSeconds % 60;
    remainingSeconds /= 60; // Hours + days
    timeStruct.hour = remainingSeconds % 24;

    // Calculate day of year
    unsigned long daysSinceEpoch = remainingSeconds / 24;
    int year = 1970;
    int dayOfYear = 0;

    while (daysSinceEpoch > 0) {
        int isLeapYear = (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
        unsigned long daysInYear = isLeapYear ? 366 : 365;

        if (daysSinceEpoch < daysInYear) {
            dayOfYear = daysSinceEpoch + 1; // 1-based for IRIG-B
            break;
        }
        daysSinceEpoch -= daysInYear;
        year++;
    }
    timeStruct.day = dayOfYear+1;
    timeStruct.year = year;
    timeStruct.millisecond = _currentMilliseconds;

    return timeStruct;
}


void init_ntp() {
    Serial.println("Initializing NTP...");
    ntpUDP = new WiFiUDP();

    // Set NTP server, port, and time offset from settings
    _poolServerName = settings.ntp.server;
    _serverPort = settings.ntp.port;
    _timeOffset = settings.ntp.timeOffset;

    Serial.printf("NTP Server: %s\n", _poolServerName.c_str());
    Serial.printf("NTP Port: %d\n", _serverPort);
    Serial.printf("NTP Time Offset: %d seconds\n", _timeOffset);
    ntp_begin();
    ntp_setUpdateInterval(60000);
}