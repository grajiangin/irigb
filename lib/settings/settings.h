#ifndef SETTINGS_H
#define SETTINGS_H

#include <Arduino.h>
#include <Preferences.h>

class Settings {
public:
    Settings();
    ~Settings();

    // Load settings from Preferences
    bool load();

    // Save settings to Preferences
    bool save();

    // Network settings
    struct NetworkConfig {
        bool dhcp;
        String ip;
        String subnet;
        String gateway;
        String dns;
    } network;

    // NTP settings
    struct NTPConfig {
        String server;
        uint16_t port;
        int32_t timeOffset; // Time offset in hours
    } ntp;

    // System settings
    bool enabled;

    // Network changes flag - set to true when network settings are updated via web interface
    bool network_changes_flag;

    // IRIG channel modes (8 channels)
    uint8_t channel_1_mode;
    uint8_t channel_2_mode;
    uint8_t channel_3_mode;
    uint8_t channel_4_mode;
    uint8_t channel_5_mode;
    uint8_t channel_6_mode;
    uint8_t channel_7_mode;
    uint8_t channel_8_mode;

    // Network changes flag methods
    bool getNetworkChangesFlag();
    void setNetworkChangesFlag(bool flag);

    // Get default values
    static NetworkConfig getDefaultNetwork();
    static NTPConfig getDefaultNTP();
    static bool getDefaultEnabled();
    static bool getDefaultNetworkChangesFlag();
    static uint8_t getDefaultChannelMode();

private:
    Preferences preferences;
    static const char* NAMESPACE;
};

#endif // SETTINGS_H
