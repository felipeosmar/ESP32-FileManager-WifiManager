#ifndef NTP_MANAGER_H
#define NTP_MANAGER_H

#include <Arduino.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <ArduinoJson.h>
#include <LittleFS.h>

#define NTP_SERVER_MAX_LEN 64

class NTPManager {
public:
    struct NTPConfig {
        char server[NTP_SERVER_MAX_LEN];
        long offset;      // Timezone offset in seconds
        int interval;     // Update interval in milliseconds
        bool enabled;
    };

    NTPManager();
    ~NTPManager();

    // Initialize NTP client
    void begin();

    // Update loop (must be called frequently)
    void update();

    // Load configuration from JSON
    bool loadConfig(const JsonDocument& doc);

    // Save configuration to JSON
    void saveConfig(JsonDocument& doc);

    // Get formatted time string
    String getFormattedTime();

    // Get current configuration
    const NTPConfig& getConfig() const { return config; }

    // Update configuration
    void updateConfig(const char* server, long offset, int interval, bool enabled);

    // Check if NTP is synchronized/valid (basic check)
    bool isTimeSet();

private:
    WiFiUDP ntpUDP;
    NTPClient* timeClient;
    NTPConfig config;
    bool isInitialized;

    void applyConfig();
};

#endif // NTP_MANAGER_H
