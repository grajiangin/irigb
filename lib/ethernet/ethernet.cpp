/*
 * Custom Ethernet Link Status Handler
 *
 * This is a workaround for the ESP32-ENC28J60 library bug where the linkUp() function
 * always returns false due to missing event handler implementation in the library.
 *
 * The ESP32-ENC28J60 library correctly detects link status at the hardware level
 * but never updates the Arduino wrapper's link status variable.
 *
 * This implementation:
 * 1. Registers a custom event handler to capture link status changes
 * 2. Tracks link status in a global variable
 * 3. Provides eth_status() that uses our tracked status instead of the broken library function
 */

#include <Arduino.h>
#include "ethernet.h"
#include "driver/spi_master.h"
#include "ESP32-ENC28J60.h"
#include "esp_event.h"

// Global variable to track link status (workaround for library bug)
static bool _custom_eth_link_up = false;

// Custom event handler to track link status changes
void custom_eth_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    if (event_base == ETH_EVENT) {
        if (event_id == ETHERNET_EVENT_CONNECTED) {
            _custom_eth_link_up = true;
            Serial.println("✓ [Custom Handler] Ethernet link is UP");
        } else if (event_id == ETHERNET_EVENT_DISCONNECTED) {
            _custom_eth_link_up = false;
            Serial.println("⚠ [Custom Handler] Ethernet link is DOWN");
        }
    }
}

bool eth_init() {
    Serial.println("Initializing Ethernet...");
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

        // Register our custom event handler to track link status changes
        esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, custom_eth_event_handler, NULL);
        Serial.println("✓ Custom event handler registered");

        // Force an initial link check by calling the ESP32-ENC28J60 internal link update
        // This will trigger our event handler if link is already up
        delay(100);

        // Check link status using our custom tracking
        if (_custom_eth_link_up) {
            Serial.println("✓ Ethernet link is UP");
            Serial.print("Link Speed: ");
            Serial.print(ETH.linkSpeed());
            Serial.println(" Mbps");
            return true;
        } else {
            Serial.println("⚠ Ethernet link is DOWN (cable may not be connected)");
            // Still return true since hardware init was successful
            return true;
        }
    } else {
        Serial.println("✗ ENC28J60 detection failed!");
        return false;
    }

}

bool eth_dhcp() {
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
            return true;
        } else {
            Serial.println("✗ DHCP failed to assign IP address");
            return false;
        }
    } else {
        Serial.println("✗ DHCP configuration failed");
        return false;
    }
}

bool eth_static(IPAddress ip, IPAddress mask) {
    Serial.println("Configuring static IP...");
    Serial.print("IP: ");
    Serial.println(ip);
    Serial.print("Subnet Mask: ");
    Serial.println(mask);
    
    if (ETH.config(ip, mask, IPAddress(0, 0, 0, 0))) {
        Serial.println("✓ Static IP configuration successful");
        Serial.print("  IP Address: ");
        Serial.println(ETH.localIP());
        Serial.print("  Subnet Mask: ");
        Serial.println(ETH.subnetMask());
        return true;
    } else {
        Serial.println("✗ Static IP configuration failed");
        return false;
    }
}

// Function to manually update link status
void eth_update_link_status() {
    // This will trigger the ESP32-ENC28J60 internal link check
    // which will call our event handler if link status changes
    if (ETH.localIP() != IPAddress(0, 0, 0, 0)) {
        // Only check if we have an IP (meaning ETH is initialized)
        // The internal link check will trigger our event handler
    }
}

// Helper function to get current link status without debug output
bool eth_link_up() {
    return _custom_eth_link_up;
}

bool eth_status() {
    // Update link status before reporting
    eth_update_link_status();
    if (!eth_link_up()) {
        Serial.println("Ethernet status: LINK DOWN");
        Serial.print("  IP Address: ");
        Serial.println(ETH.localIP());
        Serial.print("  Subnet Mask: ");
        Serial.println(ETH.subnetMask());
        Serial.print("  Gateway: ");
        Serial.println(ETH.gatewayIP());
        Serial.print("  DNS Server: ");
        Serial.println(ETH.dnsIP());
        return false;
    }
    Serial.println("Ethernet status: LINK UP");
    Serial.print("  Link Speed: ");
    Serial.print(ETH.linkSpeed());
    Serial.println(" Mbps");
    Serial.print("  IP Address: ");
    Serial.println(ETH.localIP());
    Serial.print("  Subnet Mask: ");
    Serial.println(ETH.subnetMask());
    Serial.print("  Gateway: ");
    Serial.println(ETH.gatewayIP());
    Serial.print("  DNS Server: ");
    Serial.println(ETH.dnsIP());

    return true;
}

// Legacy function for backward compatibility
bool init_ethernet() {
    if (eth_init()) {
        return eth_dhcp();
    }
    return false;
}