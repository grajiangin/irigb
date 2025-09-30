#ifndef ETHERNET_H
#define ETHERNET_H
// Ethernet pins


// SPI configuration for ENC28J60

#include <Arduino.h>
#include <WiFi.h>
#include <ESP32-ENC28J60.h>
#include "driver/spi_master.h"
#include "settings.h"


#define SPI_CLOCK_MHZ 8  // Minimum 8MHz for ENC28J60 revision B5+
#define SPI_HOST SPI2_HOST 
#define ETH_MOSI 39
#define ETH_MISO 37
#define ETH_SCLK 41
#define ETH_CS 38
#define ETH_INT 36
#define ETH_CLK_OUT 35
#define ETH_RST 40

// New modular functions
bool eth_init();
bool eth_dhcp();
bool eth_static(IPAddress ip, IPAddress mask, IPAddress gateway, IPAddress dns);
bool eth_configure_network(Settings* settings);
bool eth_status();
bool eth_link_up();  // Helper function to get current link status
void eth_update_link_status();

// Ethernet monitoring task functions
bool eth_start_monitoring(Settings* settings);
void eth_stop_monitoring();

// Legacy function for backward compatibility
bool init_ethernet();
#endif // ETHERNET_H