#include "decoder.h"
#include "Arduino.h"

// Global decoder instance
IRIGBDecoder* IRIGBDecoder::instance = nullptr;

IRIGBDecoder::IRIGBDecoder(uint8_t inputPin) : inputPin(inputPin), bitIndex(0), lastEdgeTime(0), lastPinState(false), frameComplete(false) {
  // Initialize arrays manually to avoid volatile pointer issues
  for (int i = 0; i < 101; i++) {
    bits[i] = 0;
    bits_res[i] = 0;
  }
  bits[100] = '\0'; // Null terminator
  bit_state = 0;
  push_state = 0;
}

void IRIGBDecoder::begin() {
  pinMode(inputPin, INPUT_PULLUP);
  instance = this;

  // Attach interrupt to pin for both rising and falling edges
  attachInterrupt(digitalPinToInterrupt(inputPin), interruptHandler, CHANGE);
}

// Removed time decoding methods - now only raw bit output



void IRIGBDecoder::printBits() const {
  Serial.print("IRIG-B Bits: ");
  for (int i = 0; i < 100; i++) {
    if (i % 10 == 0) Serial.print(" ");
    Serial.print(bits_res[i]);
  }
  Serial.println();
}

void IRAM_ATTR IRIGBDecoder::interruptHandler() {
  if (instance) {
    instance->handleInterrupt();
  }
}

void IRIGBDecoder::push(char x) {
  switch(push_state) {
    case 0:
      if(x == 'M') {
        push_state = 1;
        bits[bitIndex] = 'M';
        bitIndex = 1;
      }
      break;
    case 1:
      if(x == 'M') {
        push_state = 2;
        bits[bitIndex] = 'M';
        bitIndex++;
      } else {
        push_state = 0;
        bitIndex = 0;
      }
      break;
    case 2:
      bits[bitIndex] = x;
      bitIndex++;
      if(bitIndex >= 100) {
        push_state = 0;
        for(int i = 0; i < bitIndex; i++) {
          bits_res[i] = bits[i];
        }
        bits_res[100] = '\0'; // Null terminator
        frameComplete = true;
        bitIndex = 0;
      }
      break;
  }
}

void IRIGBDecoder::handleInterrupt() {
  bool currentPinState = digitalRead(inputPin);
//   noInterrupts();

  switch (bit_state) {
    case 0:
      if (currentPinState == HIGH) {
        bit_state = 1;
        lastEdgeTime = micros();
      }
      break;
    case 1:
      if (currentPinState == LOW) {
        bit_state = 0;
        unsigned long width = micros() - lastEdgeTime;
        if(width > 1500 && width < 2500) {
          push('0');
        } else if(width > 4500 && width < 5500) {
          push('1');
        } else if(width > 7500 && width < 8500) {
          push('M');
        }
      }
      break;
  }

//   interrupts();
}

void IRIGBDecoder::processFrame() {
  // Frame is now complete - flag will be reset when getCompletedFrame() is called
}

// Time decoding removed - now only raw bit output

bool IRIGBDecoder::data_available() const {
  return frameComplete;
}

String IRIGBDecoder::get_data() {
  if (frameComplete) {
    frameComplete = false; // Reset the flag
    // Return the completed frame from bits_res
    return String(const_cast<char*>(bits_res));
  }
  return String("");
}

// Global functions for compatibility with main.cpp
static IRIGBDecoder* decoder_instance = nullptr;

IRIGBDecoder* get_decoder() {
  return decoder_instance;
}

void init_decoder() {
  decoder_instance = new IRIGBDecoder(47); // P8 is pin 47
  decoder_instance->begin();
  Serial.println("IRIG-B Decoder initialized on pin 47");
}