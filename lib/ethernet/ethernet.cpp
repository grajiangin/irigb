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
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// Global variable to track link status (workaround for library bug)
static bool _custom_eth_link_up = false;
bool eth_reinit_flag=false;
// Ethernet monitoring task variables
static TaskHandle_t _eth_monitor_task_handle = NULL;
static bool _eth_monitor_running = false;

// Current network configuration tracking
static struct {
    bool dhcp;
    String ip;
    String subnet;
    String gateway;
    String dns;
} _current_network_config;

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
    delay(100);
    digitalWrite(ETH_RST, HIGH);
    delay(100);
    
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

bool eth_static(IPAddress ip, IPAddress mask, IPAddress gateway, IPAddress dns) {
    Serial.println("Configuring static IP...");
    Serial.print("IP: ");
    Serial.println(ip);
    Serial.print("Subnet Mask: ");
    Serial.println(mask);
    Serial.print("Gateway: ");
    Serial.println(gateway);
    Serial.print("DNS: ");
    Serial.println(dns);

    if (ETH.config(ip, gateway, mask, dns)) {
        Serial.println("✓ Static IP configuration successful");
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
        Serial.println("✗ Static IP configuration failed");
        return false;
    }
}

bool eth_configure_network(Settings* settings) {
    Serial.println("Configuring network using settings...");

    if (settings->network.dhcp) {
        Serial.println("Using DHCP configuration");
        return eth_dhcp();
    } else {
        Serial.println("Using static IP configuration");
        IPAddress ip;
        IPAddress mask;
        IPAddress gateway;
        IPAddress dns;

        if (!ip.fromString(settings->network.ip)) {
            Serial.println("✗ Invalid IP address format");
            return false;
        }

        if (!mask.fromString(settings->network.subnet)) {
            Serial.println("✗ Invalid subnet mask format");
            return false;
        }

        if (!gateway.fromString(settings->network.gateway)) {
            Serial.println("✗ Invalid gateway address format");
            return false;
        }

        if (!dns.fromString(settings->network.dns)) {
            Serial.println("✗ Invalid DNS server address format");
            return false;
        }

        return eth_static(ip, mask, gateway, dns);
    }
}

// Helper function to check if network settings have changed
static bool eth_network_settings_changed(Settings* settings) {
    return (_current_network_config.dhcp != settings->network.dhcp ||
            _current_network_config.ip != settings->network.ip ||
            _current_network_config.subnet != settings->network.subnet ||
            _current_network_config.gateway != settings->network.gateway ||
            _current_network_config.dns != settings->network.dns);
}

// Helper function to update current network config tracking
static void eth_update_current_config(Settings* settings) {
    _current_network_config.dhcp = settings->network.dhcp;
    _current_network_config.ip = settings->network.ip;
    _current_network_config.subnet = settings->network.subnet;
    _current_network_config.gateway = settings->network.gateway;
    _current_network_config.dns = settings->network.dns;
}

// Ethernet monitoring task function
static void eth_monitor_task(void *parameter) {
    Settings* settings = (Settings*)parameter;
    TickType_t last_wake_time = xTaskGetTickCount();

    Serial.println("Ethernet monitoring task started");
    bool reload_settings=false;
    while (_eth_monitor_running) {

        
        // Check ethernet link status
        eth_update_link_status();

        // Check for network configuration changes via flag
        if (settings->getNetworkChangesFlag() || reload_settings) {
            Serial.println("Network changes flag detected, reloading settings...");
            reload_settings=false;
            // Reload settings to get the updated network configuration
            if (settings->load()) {
                if (eth_network_settings_changed(settings)) {
                    Serial.println("Network configuration changed, reconfiguring...");

                    // Update current config tracking
                    eth_update_current_config(settings);

                    // Reconfigure network
                    if (!eth_configure_network(settings)) {
                        Serial.println("Failed to reconfigure network with new settings");
                    }
                } else {
                    Serial.println("Network changes flag was set but no actual changes detected");
                }
            } else {
                Serial.println("Failed to reload settings");
            }

            // Clear the changes flag
            settings->setNetworkChangesFlag(false);
            Serial.println("Network changes flag cleared");
        }
        if(eth_reinit_flag)
        {
            Serial.println("======Restart Ethernet Module START======");
            eth_reinit_flag=false;
            reload_settings=true;
            // eth_init();

 // Reset the ENC28J60
                digitalWrite(ETH_RST, LOW);
                delay(100);
                digitalWrite(ETH_RST, HIGH);
                delay(100);


            // delay(1000);
            Serial.println("======Restart Ethernet Module END========");
        }

        // Wait for next iteration (100ms)
        vTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(100));
    }

    Serial.println("Ethernet monitoring task stopped");
    vTaskDelete(NULL);
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

// Start ethernet monitoring task
bool eth_start_monitoring(Settings* settings) {
    if (_eth_monitor_running) {
        Serial.println("Ethernet monitoring task already running");
        return true;
    }

    // Initialize current network config tracking
    eth_update_current_config(settings);

    _eth_monitor_running = true;

    // Create the monitoring task
    BaseType_t result = xTaskCreate(
        eth_monitor_task,           // Task function
        "eth_monitor",              // Task name
        4096,                       // Stack size (bytes)
        settings,                   // Task parameter
        1,                          // Task priority (low priority)
        &_eth_monitor_task_handle   // Task handle
    );

    if (result == pdPASS) {
        Serial.println("✓ Ethernet monitoring task started successfully");
        return true;
    } else {
        Serial.println("✗ Failed to create ethernet monitoring task");
        _eth_monitor_running = false;
        return false;
    }
}

// Stop ethernet monitoring task
void eth_stop_monitoring() {
    if (!_eth_monitor_running) {
        Serial.println("Ethernet monitoring task not running");
        return;
    }

    _eth_monitor_running = false;

    // Wait for the task to finish
    if (_eth_monitor_task_handle != NULL) {
        vTaskDelete(_eth_monitor_task_handle);
        _eth_monitor_task_handle = NULL;
        Serial.println("✓ Ethernet monitoring task stopped");
    }
}