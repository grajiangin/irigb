#ifndef DECODER_H
#define DECODER_H

#include <Arduino.h>

// Removed IrigTime struct - no longer needed for raw bit output

class IRIGBDecoder {
public:
  // Constructor
  IRIGBDecoder(uint8_t inputPin);

  // Initialize the decoder
  void begin();

  // Removed time decoding methods - now only raw bit output

  // Get raw bits (for debugging)
  const char* getBits() const;

  // Check if data is available
  bool data_available() const;

  // Get data as String and reset the flag
  String get_data();

  // Print current bits for debugging
  void printBits() const;

private:
  uint8_t inputPin;
  volatile char bits[101]; // Store as 'M', '0', '1' + null terminator
  volatile char bits_res[101]; // Result buffer
  volatile uint8_t bitIndex;
  volatile unsigned long lastEdgeTime;
  volatile bool lastPinState;
  volatile bool frameComplete;
  volatile byte bit_state;
  volatile byte push_state;

  // Interrupt service routine
  static void IRAM_ATTR interruptHandler();
  void handleInterrupt();
  void push(char x);

  // Process completed frame
  void processFrame();

  // Static instance for ISR
  static IRIGBDecoder* instance;
};

#endif // DECODER_H
