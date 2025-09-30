#include "settings.h"

const char* Settings::NAMESPACE = "irigb";

Settings::Settings() {
    // Initialize with default values
    network = getDefaultNetwork();
    ntp = getDefaultNTP();
    enabled = getDefaultEnabled();
    network_changes_flag = getDefaultNetworkChangesFlag();
    ntp_changes_flag = getDefaultNTPChangesFlag();
    

    // Initialize channel modes
    channel_1_mode = getDefaultChannelMode();
    channel_2_mode = getDefaultChannelMode();
    channel_3_mode = getDefaultChannelMode();
    channel_4_mode = getDefaultChannelMode();
    channel_5_mode = getDefaultChannelMode();
    channel_6_mode = getDefaultChannelMode();
    channel_7_mode = getDefaultChannelMode();
    channel_8_mode = getDefaultChannelMode();
}

Settings::~Settings() {
    // Cleanup if needed
}

bool Settings::load() {
    Serial.println("Loading settings from Preferences...");
    
    if (!preferences.begin(NAMESPACE, true)) {
        Serial.println("Failed to open Preferences for reading");
        return false;
    }

    // Load network settings
    network.dhcp = preferences.getBool("dhcp", network.dhcp);
    network.ip = preferences.getString("ip", network.ip);
    network.subnet = preferences.getString("subnet", network.subnet);
    network.gateway = preferences.getString("gateway", network.gateway);
    network.dns = preferences.getString("dns", network.dns);

    // Load NTP settings
    ntp.server = preferences.getString("ntpServer", ntp.server);
    ntp.port = preferences.getUShort("ntpPort", ntp.port);
    ntp.timeOffset = preferences.getInt("timeOffset", ntp.timeOffset);

    // Load system settings
    enabled = preferences.getBool("enabled", enabled);

    // Load network changes flag
    network_changes_flag = preferences.getBool("netFlag", network_changes_flag);

    // Load NTP changes flag
    ntp_changes_flag = preferences.getBool("ntpFlag", ntp_changes_flag);

    // Load channel modes
    channel_1_mode = preferences.getUChar("channel_1_mode", channel_1_mode);
    channel_2_mode = preferences.getUChar("channel_2_mode", channel_2_mode);
    channel_3_mode = preferences.getUChar("channel_3_mode", channel_3_mode);
    channel_4_mode = preferences.getUChar("channel_4_mode", channel_4_mode);
    channel_5_mode = preferences.getUChar("channel_5_mode", channel_5_mode);
    channel_6_mode = preferences.getUChar("channel_6_mode", channel_6_mode);
    channel_7_mode = preferences.getUChar("channel_7_mode", channel_7_mode);
    channel_8_mode = preferences.getUChar("channel_8_mode", channel_8_mode);

    preferences.end();

    Serial.println("Settings loaded successfully");
    Serial.printf("Network: DHCP=%s, IP=%s\n", network.dhcp ? "true" : "false", network.ip.c_str());
    Serial.printf("NTP: Server=%s, Port=%d, Offset=%d\n", ntp.server.c_str(), ntp.port, ntp.timeOffset);
    Serial.printf("System: Enabled=%s\n", enabled ? "true" : "false");
    Serial.printf("Channels: 1=%d, 2=%d, 3=%d, 4=%d, 5=%d, 6=%d, 7=%d, 8=%d\n",
                  channel_1_mode, channel_2_mode, channel_3_mode, channel_4_mode,
                  channel_5_mode, channel_6_mode, channel_7_mode, channel_8_mode);

    return true;
}

bool Settings::getNetworkChangesFlag() {
    return network_changes_flag;
}

void Settings::setNetworkChangesFlag(bool flag) {
    network_changes_flag = flag;
    // Save immediately when flag is set to true to ensure it's persisted
    if (flag) {
        save();
    }
}

bool Settings::getNTPChangesFlag() {
    return ntp_changes_flag;
}

void Settings::setNTPChangesFlag(bool flag) {
    ntp_changes_flag = flag;
    // Save immediately when flag is set to true to ensure it's persisted
    if (flag) {
        save();
    }
}

bool Settings::save() {
    Serial.println("Saving settings to Preferences...");
    
    if (!preferences.begin(NAMESPACE, false)) {
        Serial.println("Failed to open Preferences for writing");
        return false;
    }

    // Save network settings
    preferences.putBool("dhcp", network.dhcp);
    preferences.putString("ip", network.ip);
    preferences.putString("subnet", network.subnet);
    preferences.putString("gateway", network.gateway);
    preferences.putString("dns", network.dns);

    // Save NTP settings
    preferences.putString("ntpServer", ntp.server);
    preferences.putUShort("ntpPort", ntp.port);
    preferences.putInt("timeOffset", ntp.timeOffset);

    // Save system settings
    preferences.putBool("enabled", enabled);

    // Save network changes flag
    preferences.putBool("netFlag", network_changes_flag);

    // Save NTP changes flag
    preferences.putBool("ntpFlag", ntp_changes_flag);

    // Save channel modes
    Serial.printf("Saving channel modes: 1=%d, 2=%d, 3=%d, 4=%d, 5=%d, 6=%d, 7=%d, 8=%d\n", 
                  channel_1_mode, channel_2_mode, channel_3_mode, channel_4_mode,
                  channel_5_mode, channel_6_mode, channel_7_mode, channel_8_mode);
    preferences.putUChar("channel_1_mode", channel_1_mode);
    preferences.putUChar("channel_2_mode", channel_2_mode);
    preferences.putUChar("channel_3_mode", channel_3_mode);
    preferences.putUChar("channel_4_mode", channel_4_mode);
    preferences.putUChar("channel_5_mode", channel_5_mode);
    preferences.putUChar("channel_6_mode", channel_6_mode);
    preferences.putUChar("channel_7_mode", channel_7_mode);
    preferences.putUChar("channel_8_mode", channel_8_mode);

    preferences.end();

    Serial.println("Settings saved successfully");
    return true;
}

Settings::NetworkConfig Settings::getDefaultNetwork() {
    NetworkConfig config;
    config.dhcp = true;
    config.ip = "192.168.1.100";
    config.subnet = "255.255.255.0";
    config.gateway = "192.168.1.1";
    config.dns = "8.8.8.8";
    return config;
}

Settings::NTPConfig Settings::getDefaultNTP() {
    NTPConfig config;
    config.server = "pool.ntp.org";
    config.port = 123;
    config.timeOffset = 0;
    return config;
}

bool Settings::getDefaultEnabled() {
    return true;
}

bool Settings::getDefaultNetworkChangesFlag() {
    return false;  // Default to false, no changes pending
}

bool Settings::getDefaultNTPChangesFlag() {
    return false;  // Default to false, no changes pending
}

uint8_t Settings::getDefaultChannelMode() {
    return 0;  // Default to FORMAT_B000_B120
}
