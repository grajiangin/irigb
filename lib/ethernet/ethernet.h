#ifndef ETHERNET_H
#define ETHERNET_H
// Ethernet pins

// SPI configuration for W5500

#include <Arduino.h>
#include "settings.h"

#define SPI_CLOCK_MHZ 12  // W5500 SPI clock speed (12MHz for stability)
#define ETH_MOSI 39
#define ETH_MISO 37
#define ETH_SCLK 41
#define ETH_CS 38
#define ETH_INT 36
#define ETH_RST 40

// New modular functions
bool eth_init();
bool eth_dhcp();
bool eth_static(IPAddress ip, IPAddress mask, IPAddress gateway, IPAddress dns);
bool eth_configure_network(Settings* settings);
bool eth_status();
bool eth_link_up();  // Helper function to get current link status
void eth_update_link_status();

// Get IP address
IPAddress eth_local_ip();
IPAddress eth_subnet_mask();
IPAddress eth_gateway_ip();
IPAddress eth_dns_ip();
String eth_mac_address();

// Ethernet monitoring task functions
bool eth_start_monitoring(Settings* settings);
void eth_stop_monitoring();

// Legacy function for backward compatibility
bool init_ethernet();
#endif // ETHERNET_H
