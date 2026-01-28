/*
 * W5500 Ethernet Driver
 *
 * This implementation uses the native ESP-IDF W5500 driver.
 */

#include <Arduino.h>
#include "ethernet.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_eth.h"
#include "esp_netif.h"
#include "esp_event.h"
#include <ETH.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// Ethernet handle
static esp_eth_handle_t eth_handle = NULL;
static esp_netif_t *eth_netif = NULL;

// Global variable to track link status
static bool _eth_link_up = false;
static bool _eth_got_ip = false;
bool eth_reinit_flag = false;

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

// Event handler for ethernet events
static void eth_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    if (event_base == ETH_EVENT) {
        switch (event_id) {
            case ETHERNET_EVENT_CONNECTED:
                _eth_link_up = true;
                Serial.println("[ETH] Ethernet link UP");
                break;
            case ETHERNET_EVENT_DISCONNECTED:
                _eth_link_up = false;
                _eth_got_ip = false;
                Serial.println("[ETH] Ethernet link DOWN");
                break;
            case ETHERNET_EVENT_START:
                Serial.println("[ETH] Ethernet started");
                break;
            case ETHERNET_EVENT_STOP:
                Serial.println("[ETH] Ethernet stopped");
                break;
            default:
                break;
        }
    } else if (event_base == IP_EVENT) {
        if (event_id == IP_EVENT_ETH_GOT_IP) {
            ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
            _eth_got_ip = true;
            Serial.printf("[ETH] Got IP: " IPSTR "\n", IP2STR(&event->ip_info.ip));
            Serial.printf("[ETH] Netmask: " IPSTR "\n", IP2STR(&event->ip_info.netmask));
            Serial.printf("[ETH] Gateway: " IPSTR "\n", IP2STR(&event->ip_info.gw));
        }
    }
}

bool eth_init() {
    Serial.println("Initializing W5500 Ethernet...");
    
    // Configure reset pin and perform hardware reset
    pinMode(ETH_RST, OUTPUT);
    digitalWrite(ETH_RST, LOW);
    delay(100);
    digitalWrite(ETH_RST, HIGH);
    delay(100);

    // Initialize TCP/IP network interface
    ESP_ERROR_CHECK(esp_netif_init());
    
    // Create default event loop if not already created
    esp_err_t err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        Serial.printf("[ETH] Failed to create event loop: %d\n", err);
        return false;
    }

    // Register event handlers
    ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, &eth_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &eth_event_handler, NULL));

    // Create default ethernet netif
    esp_netif_config_t cfg = ESP_NETIF_DEFAULT_ETH();
    eth_netif = esp_netif_new(&cfg);

    // Install GPIO ISR service
    gpio_install_isr_service(0);

    // Configure SPI bus
    spi_bus_config_t buscfg = {
        .mosi_io_num = ETH_MOSI,
        .miso_io_num = ETH_MISO,
        .sclk_io_num = ETH_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_CH_AUTO));

    // Configure SPI device for W5500
    spi_device_handle_t spi_handle = NULL;
    spi_device_interface_config_t devcfg = {
        .command_bits = 16,  // Address phase in W5500 SPI frame
        .address_bits = 8,   // Control phase in W5500 SPI frame
        .mode = 0,
        .clock_speed_hz = SPI_CLOCK_MHZ * 1000 * 1000,
        .spics_io_num = ETH_CS,
        .queue_size = 20
    };
    ESP_ERROR_CHECK(spi_bus_add_device(SPI3_HOST, &devcfg, &spi_handle));

    // Configure W5500
    eth_w5500_config_t w5500_config = ETH_W5500_DEFAULT_CONFIG(spi_handle);
    w5500_config.int_gpio_num = ETH_INT;

    // Configure MAC and PHY
    eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
    eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();
    phy_config.reset_gpio_num = -1;  // We already did hardware reset

    // Create MAC and PHY instances
    esp_eth_mac_t *eth_mac = esp_eth_mac_new_w5500(&w5500_config, &mac_config);
    if (eth_mac == NULL) {
        Serial.println("[ETH] Failed to create W5500 MAC");
        return false;
    }

    esp_eth_phy_t *eth_phy = esp_eth_phy_new_w5500(&phy_config);
    if (eth_phy == NULL) {
        Serial.println("[ETH] Failed to create W5500 PHY");
        return false;
    }

    // Install ethernet driver
    esp_eth_config_t eth_config = ETH_DEFAULT_CONFIG(eth_mac, eth_phy);
    ESP_ERROR_CHECK(esp_eth_driver_install(&eth_config, &eth_handle));

    // Set MAC address (use a unique MAC)
    uint8_t mac_addr[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};
    ESP_ERROR_CHECK(esp_eth_ioctl(eth_handle, ETH_CMD_S_MAC_ADDR, mac_addr));

    // Attach ethernet driver to TCP/IP stack
    ESP_ERROR_CHECK(esp_netif_attach(eth_netif, esp_eth_new_netif_glue(eth_handle)));

    // Start ethernet driver
    ESP_ERROR_CHECK(esp_eth_start(eth_handle));

    Serial.println("[ETH] W5500 initialized successfully!");
    Serial.print("[ETH] MAC Address: ");
    Serial.println(eth_mac_address());

    return true;
}

bool eth_dhcp() {
    Serial.println("Configuring DHCP...");
    
    // Enable DHCP client
    ESP_ERROR_CHECK(esp_netif_dhcpc_start(eth_netif));
    
    // Wait for IP address
    Serial.println("Waiting for DHCP to assign IP address...");
    int attempts = 0;
    while (!_eth_got_ip && attempts < 100) {
        delay(100);
        attempts++;
    }
    
    if (_eth_got_ip) {
        Serial.println("[ETH] DHCP configuration successful");
        return true;
    } else {
        Serial.println("[ETH] DHCP failed to obtain IP address");
        return false;
    }
}

bool eth_static(IPAddress ip, IPAddress mask, IPAddress gateway, IPAddress dns) {
    Serial.println("Configuring static IP...");
    Serial.print("IP: "); Serial.println(ip);
    Serial.print("Subnet Mask: "); Serial.println(mask);
    Serial.print("Gateway: "); Serial.println(gateway);
    Serial.print("DNS: "); Serial.println(dns);

    // Stop DHCP client first
    esp_netif_dhcpc_stop(eth_netif);

    // Set static IP configuration
    esp_netif_ip_info_t ip_info;
    ip_info.ip.addr = (uint32_t)ip;
    ip_info.netmask.addr = (uint32_t)mask;
    ip_info.gw.addr = (uint32_t)gateway;

    ESP_ERROR_CHECK(esp_netif_set_ip_info(eth_netif, &ip_info));

    // Set DNS server
    esp_netif_dns_info_t dns_info;
    dns_info.ip.u_addr.ip4.addr = (uint32_t)dns;
    dns_info.ip.type = ESP_IPADDR_TYPE_V4;
    ESP_ERROR_CHECK(esp_netif_set_dns_info(eth_netif, ESP_NETIF_DNS_MAIN, &dns_info));

    _eth_got_ip = true;
    Serial.println("[ETH] Static IP configuration successful");
    
    return true;
}

bool eth_configure_network(Settings* settings) {
    Serial.println("Configuring network using settings...");

    if (settings->network.dhcp) {
        Serial.println("Using DHCP configuration");
        return eth_dhcp();
    } else {
        Serial.println("Using static IP configuration");
        IPAddress ip, mask, gateway, dns;

        if (!ip.fromString(settings->network.ip)) {
            Serial.println("[ETH] Invalid IP address format");
            return false;
        }
        if (!mask.fromString(settings->network.subnet)) {
            Serial.println("[ETH] Invalid subnet mask format");
            return false;
        }
        if (!gateway.fromString(settings->network.gateway)) {
            Serial.println("[ETH] Invalid gateway address format");
            return false;
        }
        if (!dns.fromString(settings->network.dns)) {
            Serial.println("[ETH] Invalid DNS server address format");
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
    bool reload_settings = false;
    
    while (_eth_monitor_running) {
        // Check for network configuration changes via flag
        if (settings->getNetworkChangesFlag() || reload_settings) {
            Serial.println("Network changes flag detected, reloading settings...");
            reload_settings = false;
            
            if (settings->load()) {
                if (eth_network_settings_changed(settings)) {
                    Serial.println("Network configuration changed, reconfiguring...");
                    eth_update_current_config(settings);

                    if (!eth_configure_network(settings)) {
                        Serial.println("Failed to reconfigure network with new settings");
                    }
                } else {
                    Serial.println("Network changes flag was set but no actual changes detected");
                }
            } else {
                Serial.println("Failed to reload settings");
            }

            settings->setNetworkChangesFlag(false);
            Serial.println("Network changes flag cleared");
        }
        
        if (eth_reinit_flag) {
            Serial.println("======Restart Ethernet Module START======");
            eth_reinit_flag = false;
            reload_settings = true;

            // Hardware reset the W5500
            Serial.println("Step 1: Hardware reset W5500 chip...");
            digitalWrite(ETH_RST, LOW);
            delay(250);
            digitalWrite(ETH_RST, HIGH);
            delay(1000);

            // Reset link status
            _eth_link_up = false;
            _eth_got_ip = false;
            
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
    // Link status is updated via event handler
}

// Helper function to get current link status
bool eth_link_up() {
    return _eth_link_up;
}

// Get IP addresses
IPAddress eth_local_ip() {
    esp_netif_ip_info_t ip_info;
    if (esp_netif_get_ip_info(eth_netif, &ip_info) == ESP_OK) {
        return IPAddress(ip_info.ip.addr);
    }
    return IPAddress(0, 0, 0, 0);
}

IPAddress eth_subnet_mask() {
    esp_netif_ip_info_t ip_info;
    if (esp_netif_get_ip_info(eth_netif, &ip_info) == ESP_OK) {
        return IPAddress(ip_info.netmask.addr);
    }
    return IPAddress(0, 0, 0, 0);
}

IPAddress eth_gateway_ip() {
    esp_netif_ip_info_t ip_info;
    if (esp_netif_get_ip_info(eth_netif, &ip_info) == ESP_OK) {
        return IPAddress(ip_info.gw.addr);
    }
    return IPAddress(0, 0, 0, 0);
}

IPAddress eth_dns_ip() {
    esp_netif_dns_info_t dns_info;
    if (esp_netif_get_dns_info(eth_netif, ESP_NETIF_DNS_MAIN, &dns_info) == ESP_OK) {
        return IPAddress(dns_info.ip.u_addr.ip4.addr);
    }
    return IPAddress(0, 0, 0, 0);
}

String eth_mac_address() {
    uint8_t mac[6];
    if (eth_handle && esp_eth_ioctl(eth_handle, ETH_CMD_G_MAC_ADDR, mac) == ESP_OK) {
        char macStr[18];
        snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
                 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        return String(macStr);
    }
    return "00:00:00:00:00:00";
}

bool eth_status() {
    if (!_eth_link_up) {
        Serial.println("Ethernet status: LINK DOWN");
        return false;
    }
    
    Serial.println("Ethernet status: LINK UP");
    Serial.print("  IP Address: ");
    Serial.println(eth_local_ip());
    Serial.print("  Subnet Mask: ");
    Serial.println(eth_subnet_mask());
    Serial.print("  Gateway: ");
    Serial.println(eth_gateway_ip());
    Serial.print("  DNS Server: ");
    Serial.println(eth_dns_ip());

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

    eth_update_current_config(settings);
    _eth_monitor_running = true;

    BaseType_t result = xTaskCreate(
        eth_monitor_task,
        "eth_monitor",
        8192,
        settings,
        1,
        &_eth_monitor_task_handle
    );

    if (result == pdPASS) {
        Serial.println("[ETH] Ethernet monitoring task started successfully");
        return true;
    } else {
        Serial.println("[ETH] Failed to create ethernet monitoring task");
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

    if (_eth_monitor_task_handle != NULL) {
        vTaskDelete(_eth_monitor_task_handle);
        _eth_monitor_task_handle = NULL;
        Serial.println("[ETH] Ethernet monitoring task stopped");
    }
}
