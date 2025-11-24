#include "ntp_manager.h"

NTPManager::NTPManager() : timeClient(nullptr), isInitialized(false) {
    // Default configuration
    strlcpy(config.server, "pool.ntp.org", sizeof(config.server));
    config.offset = -10800; // UTC-3 (Brazil)
    config.interval = 60000; // 60 seconds
    config.enabled = true;

    timeClient = new NTPClient(ntpUDP, config.server, config.offset, config.interval);
}

NTPManager::~NTPManager() {
    if (timeClient) {
        delete timeClient;
    }
}

void NTPManager::begin() {
    if (config.enabled) {
        timeClient->begin();
        isInitialized = true;
        Serial.println("NTP Client started");
    }
}

void NTPManager::update() {
    if (config.enabled && isInitialized) {
        timeClient->update();
    }
}

bool NTPManager::loadConfig(const JsonDocument& doc) {
    if (doc.containsKey("ntp")) {
        JsonObjectConst ntp = doc["ntp"].as<JsonObjectConst>();
        
        if (ntp.containsKey("server")) {
            strlcpy(config.server, ntp["server"], sizeof(config.server));
        }
        if (ntp.containsKey("offset")) {
            config.offset = ntp["offset"];
        }
        if (ntp.containsKey("interval")) {
            config.interval = ntp["interval"];
        }
        if (ntp.containsKey("enabled")) {
            config.enabled = ntp["enabled"];
        }

        applyConfig();
        return true;
    }
    return false;
}

void NTPManager::saveConfig(JsonDocument& doc) {
    JsonObject ntp = doc.createNestedObject("ntp");
    ntp["server"] = config.server;
    ntp["offset"] = config.offset;
    ntp["interval"] = config.interval;
    ntp["enabled"] = config.enabled;
}

void NTPManager::updateConfig(const char* server, long offset, int interval, bool enabled) {
    strlcpy(config.server, server, sizeof(config.server));
    config.offset = offset;
    config.interval = interval;
    config.enabled = enabled;
    applyConfig();
}

void NTPManager::applyConfig() {
    if (timeClient) {
        if (config.enabled) {
            timeClient->setPoolServerName(config.server);
            timeClient->setTimeOffset(config.offset);
            timeClient->setUpdateInterval(config.interval);
            if (!isInitialized) {
                begin();
            }
        } else {
            timeClient->end();
            isInitialized = false;
        }
    }
}

String NTPManager::getFormattedTime() {
    if (config.enabled && isInitialized) {
        return timeClient->getFormattedTime();
    }
    return "NTP Disabled";
}

bool NTPManager::isTimeSet() {
    // NTPClient doesn't have a direct "isTimeSet" method, but we can check if epoch is > 0
    // However, getEpochTime() returns 0 if not set.
    return config.enabled && isInitialized && timeClient->getEpochTime() > 0; 
}
