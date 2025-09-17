#include "display.h"


// Seven segment patterns for digits 0-9
// Based on segments A,B,C,D,E,F,G
const byte Display::segmentPatterns[10][7] = {
  //A  B  C  D  E  F  G
  { 1, 1, 1, 1, 1, 1, 0 }, // 0
  { 0, 1, 1, 0, 0, 0, 0 }, // 1
  { 1, 1, 0, 1, 1, 0, 1 }, // 2
  { 1, 1, 1, 1, 0, 0, 1 }, // 3
  { 0, 1, 1, 0, 0, 1, 1 }, // 4
  { 1, 0, 1, 1, 0, 1, 1 }, // 5
  { 1, 0, 1, 1, 1, 1, 1 }, // 6
  { 1, 1, 1, 0, 0, 0, 0 }, // 7
  { 1, 1, 1, 1, 1, 1, 1 }, // 8
  { 1, 1, 1, 1, 0, 1, 1 }  // 9
};

// Address mapping for TM1668 (based on debug results)
const byte Display::addressMap[7] = {
  6,  // A - Address 6 
  4,  // B - Address 4  
  2,  // C - Address 2
  0,  // D - Address 0
  12, // E - Address 12
  8,  // F - Address 8
  10  // G - Address 10
};

// TM1668 Communication Constants
#define TM16XX_CMD_DATA_AUTO 0x40
#define TM16XX_CMD_DATA_FIXED 0x44
#define TM16XX_CMD_DISPLAY 0x80
#define TM16XX_CMD_ADDRESS 0xC0
#define TM1668_DISPMODE_7x10 3

// TM1668 Communication Methods
void Display::bitDelay() {
    #if F_CPU > 100000000
        delayMicroseconds(1);
    #endif
}

void Display::start() {
    digitalWrite(strobePin, LOW);
    bitDelay();
}

void Display::stop() {
    digitalWrite(strobePin, HIGH);
    bitDelay();
}

void Display::send(byte data) {
    for (int i = 0; i < 8; i++) {
        digitalWrite(clockPin, LOW);
        bitDelay();
        digitalWrite(dataPin, data & 1 ? HIGH : LOW);
        bitDelay();
        data >>= 1;
        digitalWrite(clockPin, HIGH);
        bitDelay();
    }
    bitDelay();
}

void Display::sendCommand(byte cmd) {
    start();
    send(cmd);
    stop();
}

void Display::sendData(byte address, byte data) {
    begin(); // Ensure display is initialized
    sendCommand(TM16XX_CMD_DATA_FIXED);
    start();
    send(TM16XX_CMD_ADDRESS | address);
    send(data);
    stop();
}

void Display::begin() {
    if (fBeginDone) return;
    fBeginDone = true;
    clearDisplay();
    setupDisplay(true, 7); // Activate display with max intensity
}

void Display::setupDisplay(bool active, byte intensity) {
    byte actualIntensity = (intensity > 7) ? 7 : intensity;
    sendCommand(TM16XX_CMD_DISPLAY | (active ? 8 : 0) | actualIntensity);
}

void Display::clearDisplay() {
    sendCommand(TM16XX_CMD_DATA_AUTO);
    
    start();
    send(TM16XX_CMD_ADDRESS);
    for (int i = 0; i < 7; i++) { // TM1668 has 7 grids
        send(0x00);
        send(0x00); // Second byte for TM1668
    }
    stop();
}

Display::Display(byte dataPin, byte clockPin, byte strobePin) {
    this->dataPin = dataPin;
    this->clockPin = clockPin;
    this->strobePin = strobePin;
    current_leds = 0;
    fBeginDone = false;
    virtualBufferDirty = false;
    
    // Initialize virtual buffer
    clearVirtualBuffer();
    
    // Initialize pins
    pinMode(dataPin, OUTPUT);
    pinMode(clockPin, OUTPUT);
    pinMode(strobePin, OUTPUT);
    
    digitalWrite(strobePin, HIGH);
    digitalWrite(clockPin, HIGH);
    
    // Initialize display
    begin();
}

void Display::print_display(int year, int hour, int min, int sec) {
  // Clear virtual buffer first
  clearVirtualBuffer();
  
  // Extract individual digits
  int digits[9];
  digits[0] = sec % 10;        // Digit 1 - seconds units
  digits[1] = (sec / 10) % 10; // Digit 2 - seconds tens
  digits[2] = min % 10;        // Digit 3 - minutes units  
  digits[3] = (min / 10) % 10; // Digit 4 - minutes tens
  digits[4] = hour % 10;       // Digit 5 - hours units
  digits[5] = (hour / 10) % 10;// Digit 6 - hours tens
  digits[6] = year % 10;       // Digit 7 - year units
  digits[7] = (year / 10) % 10;// Digit 8 - year tens  
  digits[8] = (year / 100) % 10; // Digit 9 - year hundreds
  
  // For each segment (A through G)
  for (int segment = 0; segment < 7; segment++) {
    byte data1to8 = 0;  // Data for digits 1-8
    byte data9 = 0;     // Data for digit 9
    
    // Build bit pattern for digits 1-8
    for (int digit = 0; digit < 8; digit++) {
      if (segmentPatterns[digits[digit]][segment]) {
        data1to8 |= (1 << digit);
      }
    }
    
    // Build bit pattern for digit 9
    if (segmentPatterns[digits[8]][segment]) {
      data9 = 1;
    }
    
    // Write to virtual buffer instead of sending directly
    setVirtualData(addressMap[segment], data1to8);      // Even address for digits 1-8
    setVirtualData(addressMap[segment] + 1, data9);     // Odd address for digit 9
  }
}

void Display::print_display_no_leading_zeros(int year, int hour, int min, int sec) {
  // Clear virtual buffer first
  clearVirtualBuffer();
  
  // Extract individual digits
  int digits[9];
  digits[0] = sec % 10;        // Digit 1 - seconds units
  digits[1] = (sec / 10) % 10; // Digit 2 - seconds tens
  digits[2] = min % 10;        // Digit 3 - minutes units  
  digits[3] = (min / 10) % 10; // Digit 4 - minutes tens
  digits[4] = hour % 10;       // Digit 5 - hours units
  digits[5] = (hour / 10) % 10;// Digit 6 - hours tens
  
  // Handle year with leading zero suppression
  if (year >= 1000) {
    digits[6] = year % 10;           // units
    digits[7] = (year / 10) % 10;    // tens
    digits[8] = (year / 100) % 10;   // hundreds
  } else if (year >= 100) {
    digits[6] = year % 10;           // units
    digits[7] = (year / 10) % 10;    // tens
    digits[8] = -1;                  // blank digit 9
  } else {
    digits[6] = year % 10;           // units
    digits[7] = (year / 10) % 10;    // tens
    digits[8] = -1;                  // blank digit 9
  }
  
  // For each segment (A through G)
  for (int segment = 0; segment < 7; segment++) {
    byte data1to8 = 0;  // Data for digits 1-8
    byte data9 = 0;     // Data for digit 9
    
    // Build bit pattern for digits 1-8
    for (int digit = 0; digit < 8; digit++) {
      if (segmentPatterns[digits[digit]][segment]) {
        data1to8 |= (1 << digit);
      }
    }
    
    // Build bit pattern for digit 9 (only if not blank)
    if (digits[8] >= 0 && segmentPatterns[digits[8]][segment]) {
      data9 = 1;
    }
    
    // Write to virtual buffer instead of sending directly
    setVirtualData(addressMap[segment], data1to8);      // Even address for digits 1-8
    setVirtualData(addressMap[segment] + 1, data9);     // Odd address for digit 9
  }
}
// LED address mapping based on debug results
// Each LED is controlled by a different address, all use bit 1 (0x02)
void Display::set_leds(bool seconds, bool minute, bool hour, bool ntp, bool enabled, bool network) {
  // We need to find the seconds LED address first - it wasn't found in the debug
  // For now, we'll control each LED individually
  set_minute_led(minute);
  set_hour_led(hour);
  set_ntp_led(ntp);
  set_enabled_led(enabled);
  set_network_led(network);
  // seconds LED address unknown - need more testing
}

void Display::set_seconds_led(bool state) {
  // Seconds LED address not found in debug - might be a different address
  // Try address 13 bit 1 (from original debug where you got "second")
  setVirtualBit(13, 1, state);
}

void Display::set_minute_led(bool state) {
  // Address 1, 13, 17 all control minute LED - using address 1
  setVirtualBit(1, 1, state);
}

void Display::set_hour_led(bool state) {
  // Address 3, 19 control hour LED - using address 3
  setVirtualBit(3, 1, state);
}

void Display::set_network_led(bool state) {
  // Address 7 controls network LED
  setVirtualBit(7, 1, state);
}

void Display::set_enabled_led(bool state) {
  // Address 9 controls enabled LED
  setVirtualBit(9, 1, state);
}

void Display::set_ntp_led(bool state) {
  // Address 11 controls NTP LED
  setVirtualBit(11, 1, state);
}

void Display::clear_all_leds() {
  // Clear all LED bits in virtual buffer (preserving other bits)
  setVirtualBit(1, 1, false);   // minute LED
  setVirtualBit(3, 1, false);   // hour LED
  setVirtualBit(7, 1, false);   // network LED
  setVirtualBit(9, 1, false);   // enabled LED
  setVirtualBit(11, 1, false);  // NTP LED
  setVirtualBit(13, 1, false);  // seconds LED (if this is correct)
}

void Display::print_display_with_leds(int year, int hour, int min, int sec, 
                                    bool seconds_led, bool minute_led, bool hour_led, 
                                    bool ntp_led, bool enabled_led, bool network_led) {
  // Display the time/date (this will clear and populate virtual buffer)
  print_display(year, hour, min, sec);
  
  // Set LEDs in virtual buffer
  set_leds(seconds_led, minute_led, hour_led, ntp_led, enabled_led, network_led);
}


void Display::debug_segments() {
  Serial.println("TM1668 Segment Debug - Testing each address:");
  Serial.println("Watch digit 1 and tell me which segment lights up");
  Serial.println("Segment positions:");
  Serial.println("  AAA");
  Serial.println(" F   B");
  Serial.println("  GGG");
  Serial.println(" E   C");
  Serial.println("  DDD");
  Serial.println();
  
  // Test each even address (these control digits 1-8)
  for (int addr = 0; addr <= 12; addr += 2) {
    clearDisplay();
    sendData(addr, 0x01); // Light up only digit 1 (bit 0)
    
    Serial.print("Address ");
    Serial.print(addr);
    Serial.print(": Which segment lights up on digit 1? (A/B/C/D/E/F/G): ");
    
    delay(2000); // Give time to observe
    
    // Wait for serial input
    while (!Serial.available()) {
      delay(10);
    }
    
    String segment = Serial.readString();
    segment.trim();
    Serial.println("Recorded: " + segment);
    Serial.println();
  }
  
  clearDisplay();
  Serial.println("Debug complete!");
}

void Display::test_address(int address, int digit_mask) {
  clearDisplay();
  sendData(address, digit_mask);
  Serial.print("Address ");
  Serial.print(address);
  Serial.print(" with mask 0x");
  Serial.print(digit_mask, HEX);
  Serial.println(" - Which segments light up?");
}

// Virtual display buffer methods
void Display::clearVirtualBuffer() {
  for (int i = 0; i < 14; i++) {
    virtualBuffer[i] = 0x00;
  }
  virtualBufferDirty = true;
}

void Display::setVirtualData(byte address, byte data) {
  if (address < 14) {
    virtualBuffer[address] = data;
    virtualBufferDirty = true;
  }
}

void Display::setVirtualBit(byte address, byte bit, bool state) {
  if (address < 14 && bit < 8) {
    if (state) {
      virtualBuffer[address] |= (1 << bit);  // Set bit
    } else {
      virtualBuffer[address] &= ~(1 << bit); // Clear bit
    }
    virtualBufferDirty = true;
  }
}

void Display::display() {
  for (int i = 0; i < 14; i++) {
    sendCommand(TM16XX_CMD_DATA_FIXED);
    start();
    send(TM16XX_CMD_ADDRESS | i);
    send(virtualBuffer[i]);
    stop();
  }
}

bool Display::isVirtualBufferDirty() {
  return virtualBufferDirty;
}

void Display::off() {
  // Clear virtual buffer first
  clearVirtualBuffer();
  
  // Turn on only network and NTP LEDs
  setVirtualBit(7, 1, true);   // network LED
  setVirtualBit(11, 1, true);  // NTP LED
  
  // All other LEDs remain off (cleared by clearVirtualBuffer)
  // All digits remain blank (cleared by clearVirtualBuffer)
}