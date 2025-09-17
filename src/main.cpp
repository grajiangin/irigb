#include <Arduino.h>
#include "driver/spi_master.h"
#include <esp_err.h>
#include "ESP32-ENC28J60.h"
#include "pins.h"

#include "display.h"
#include "irig.h"


// SPI configuration for ENC28J60
#define SPI_CLOCK_MHZ 8  // Minimum 8MHz for ENC28J60 revision B5+
#define SPI_HOST SPI2_HOST


// Define the 7-segment font for digits 0-9
const byte SEVEN_SEG_FONT[] = {
  0b00111111, // 0: ABCDEF
  0b00000110, // 1: BC
  0b01011011, // 2: ABDEG
  0b01001111, // 3: ABCDG
  0b01100110, // 4: BCFG
  0b01101101, // 5: ACDFG
  0b01111101, // 6: ACDEFG
  0b00000111, // 7: ABC
  0b01111111, // 8: ABCDEFG
  0b01101111  // 9: ABCDFG
};
// Display object (now handles TM1668 communication internally)


// Function to detect ENC28J60 ethernet chip
bool detectENC28J60() {
  Serial.println("Detecting ENC28J60 ethernet chip...");
  
  // Initialize SPI pins
  pinMode(ETH_CS, OUTPUT);
  pinMode(ETH_RST, OUTPUT);
  pinMode(ETH_INT, INPUT);
  
  // Reset the ENC28J60
  digitalWrite(ETH_RST, LOW);
  delay(10);
  digitalWrite(ETH_RST, HIGH);
  delay(10);
  
  // Try to initialize the ENC28J60
  bool result = ETH.begin(ETH_MISO, ETH_MOSI, ETH_SCLK, ETH_CS, ETH_INT, SPI_CLOCK_MHZ, SPI_HOST);
  
  if (result) {
    Serial.println("✓ ENC28J60 detected and initialized successfully!");
    Serial.print("MAC Address: ");
    Serial.println(ETH.macAddress());
    
    // Wait for link to come up
    int attempts = 0;
    while (!ETH.linkUp() && attempts < 50) {
      delay(100);
      attempts++;
    }
    
    if (ETH.linkUp()) {
      Serial.println("✓ Ethernet link is UP");
      Serial.print("Link Speed: ");
      Serial.print(ETH.linkSpeed());
      Serial.println(" Mbps");
      
      // Configure DHCP
      Serial.println("Configuring DHCP...");
      if (ETH.config(IPAddress(0, 0, 0, 0), IPAddress(0, 0, 0, 0), IPAddress(0, 0, 0, 0))) {
        Serial.println("✓ DHCP configuration successful");
        
        // Wait for DHCP to assign IP address
        Serial.println("Waiting for DHCP to assign IP address...");
        int dhcpAttempts = 0;
        while (ETH.localIP() == IPAddress(0, 0, 0, 0) && dhcpAttempts < 100) {
          delay(100);
          dhcpAttempts++;
        }
        
        if (ETH.localIP() != IPAddress(0, 0, 0, 0)) {
          Serial.println("✓ DHCP IP address obtained:");
          Serial.print("  IP Address: ");
          Serial.println(ETH.localIP());
          Serial.print("  Subnet Mask: ");
          Serial.println(ETH.subnetMask());
          Serial.print("  Gateway: ");
          Serial.println(ETH.gatewayIP());
          Serial.print("  DNS Server: ");
          Serial.println(ETH.dnsIP());
        } else {
          Serial.println("✗ DHCP failed to assign IP address");
        }
      } else {
        Serial.println("✗ DHCP configuration failed");
      }
    } else {
      Serial.println("⚠ Ethernet link is DOWN (cable may not be connected)");
    }
    
    return true;
  } else {
    Serial.println("✗ ENC28J60 detection failed!");
    return false;
  }
}



// Ethernet monitoring variables
bool ethernetDetected = false;
bool lastLinkStatus = false;
IPAddress lastIP = IPAddress(0, 0, 0, 0);
unsigned long lastStatusCheck = 0;
const unsigned long STATUS_CHECK_INTERVAL = 5000; // Check every 5 seconds

// LEDC configuration for 1kHz clock generation
#define LEDC_CHANNEL_0     0
#define LEDC_TIMER_13_BIT  13
#define LEDC_BASE_FREQ     5000
#define LEDC_FREQUENCY     1000  // 1kHz

void setup() {
  Serial.begin(115200);
  init_irig();
  
  // Display is now initialized automatically in its constructor
  // // Test all 10 digits with demonstration
  // Serial.println("Testing 10-digit seven-segment display:");
  
  // Serial.println("Displaying: 1234567890");
  // delay(2000);
  
  // // Configure LEDC timer
  // ledcSetup(LEDC_CHANNEL_0, LEDC_FREQUENCY, LEDC_TIMER_13_BIT);
  
  // // Attach the channel to the WCLK pin
  // ledcAttachPin(WCLK, LEDC_CHANNEL_0);
  
  // // Set duty cycle to 50% for square wave (1kHz clock)
  // ledcWrite(LEDC_CHANNEL_0, 4095);  // 50% of 8191 (2^13 - 1)
  
  // pinMode(P1, OUTPUT);
  // digitalWrite(P1, HIGH);
  
  // Serial.println("1kHz clock generator started on WCLK pin");
  
  // // Detect ENC28J60 ethernet chip
  // if (detectENC28J60()) {
  //   Serial.println("Ethernet chip detection successful");
  //   ethernetDetected = true;
  //   lastLinkStatus = ETH.linkUp();
  //   lastIP = ETH.localIP();
  // } else {
  //   Serial.println("Ethernet chip detection failed");
  //   ethernetDetected = false;
  // }
}

// Function to check ethernet status periodically
void checkEthernetStatus() {
  if (!ethernetDetected) {
    return; // Skip if ethernet wasn't detected initially
  }
  
  bool currentLinkStatus = ETH.linkUp();
  IPAddress currentIP = ETH.localIP();
  
  // Check for link status changes
  if (currentLinkStatus != lastLinkStatus) {
    if (currentLinkStatus) {
      Serial.println("🔗 Ethernet link is now UP");
    } else {
      Serial.println("🔌 Ethernet link is now DOWN");
    }
    lastLinkStatus = currentLinkStatus;
  }
  
  // Check for IP address changes
  if (currentIP != lastIP) {
    if (currentIP != IPAddress(0, 0, 0, 0)) {
      Serial.println("📡 IP address changed:");
      Serial.print("  New IP: ");
      Serial.println(currentIP);
      Serial.print("  Gateway: ");
      Serial.println(ETH.gatewayIP());
    } else {
      Serial.println("❌ IP address lost (DHCP expired or disconnected)");
    }
    lastIP = currentIP;
  }
  
  // Periodic status report
  if (millis() - lastStatusCheck >= STATUS_CHECK_INTERVAL) {
    Serial.println("📊 Ethernet Status Report:");
    Serial.print("  Link: ");
    Serial.println(currentLinkStatus ? "UP" : "DOWN");
    Serial.print("  Speed: ");
    Serial.print(ETH.linkSpeed());
    Serial.println(" Mbps");
    Serial.print("  IP: ");
    Serial.println(currentIP);
    Serial.print("  MAC: ");
    Serial.println(ETH.macAddress());
    Serial.println("---");
    lastStatusCheck = millis();
  }
}

Display display(DISP_DATA, DISP_CLK, DISP_STB);
uint8_t t = 0;





void loop() {
  // // NEW VIRTUAL DISPLAY BUFFER APPROACH:
  // // 1. Update virtual buffer with display content
  // display.print_display(0,0,0,t);
  // // display.set_seconds_led(true);
  // display.set_ntp_led(true);
  
  // // 2. Transfer virtual buffer to hardware display
  // display.display();
  
  // delay(500);
  
  // // Update virtual buffer again
  // display.set_ntp_led(false);
  
  // // Transfer to hardware
  // display.display();
  
  // // debug_leds();
  t++;
  delay(500);
}


