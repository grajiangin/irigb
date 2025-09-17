#ifndef DISPLAY_H
#define DISPLAY_H

#include <Arduino.h>

// LED bit positions based on GRID mapping
#define LED_SECONDS  0  // GRID1 -> bit 0
#define LED_MINUTE   1  // GRID2 -> bit 1  
#define LED_HOUR     2  // GRID3 -> bit 2
#define LED_NTP      3  // GRID4 -> bit 3
#define LED_ENABLED  4  // GRID5 -> bit 4
#define LED_NETWORK  5  // GRID6 -> bit 5

class Display {
private:
    static const byte segmentPatterns[10][7];
    static const byte addressMap[7];
    byte current_leds;
    bool fBeginDone;
    byte dataPin;
    byte clockPin;
    byte strobePin;
    
    // Virtual display buffer
    // TM1668 has 7 grids, each with 2 bytes (14 bytes total)
    byte virtualBuffer[14];
    bool virtualBufferDirty;
    
    // TM1668 Communication Methods
    void bitDelay();
    void start();
    void stop();
    void send(byte data);
    void sendCommand(byte cmd);
    void sendData(byte address, byte data);
    void begin();
    void setupDisplay(bool active, byte intensity);
    void clearDisplay();
    

public:




    // Constructor
    Display(byte dataPin, byte clockPin, byte strobePin);
    
    // Display functions
    void print_display(int year, int hour, int min, int sec);
    void print_display_no_leading_zeros(int year, int hour, int min, int sec);
    void print_display_with_leds(int year, int hour, int min, int sec, 
                               bool seconds_led, bool minute_led, bool hour_led, 
                               bool ntp_led, bool enabled_led, bool network_led);
    void off();  // Blank all digits, keep only network and NTP LEDs
    
    // Virtual display buffer methods
    void display();  // Transfer virtual buffer to hardware
    void clearVirtualBuffer();  // Clear virtual buffer
    void setVirtualData(byte address, byte data);  // Set data in virtual buffer
    void setVirtualBit(byte address, byte bit, bool state);  // Set specific bit in virtual buffer
    bool isVirtualBufferDirty();  // Check if virtual buffer needs updating
    
    // LED control functions
    void set_leds(bool seconds, bool minute, bool hour, bool ntp, bool enabled, bool network);
    void set_seconds_led(bool state);
    void set_minute_led(bool state);
    void set_hour_led(bool state);
    void set_network_led(bool state);
    void set_enabled_led(bool state);
    void set_ntp_led(bool state);
    void clear_all_leds();
    
    // Utility functions
    void debug_segments();
    void test_address(int address, int digit_mask);
};

#endif // DISPLAY_H