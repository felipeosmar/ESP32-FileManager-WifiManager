/**
 * MQTT Manager
 * Handles MQTT client connection and messaging
 */

#ifndef MQTT_MANAGER_H
#define MQTT_MANAGER_H

#include <Arduino.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include <ArduinoJson.h>

// MQTT Configuration Limits
#define MQTT_SERVER_MAX_LEN 64
#define MQTT_USERNAME_MAX_LEN 32
#define MQTT_PASSWORD_MAX_LEN 64
#define MQTT_CLIENTID_MAX_LEN 32
#define MQTT_HOSTNAME_MAX_LEN 32
#define MQTT_MAINTOPIC_MAX_LEN 64
#define MQTT_TOPIC_BUFFER_SIZE 256  // Buffer for constructed topics (mainTopic/hostname/subtopic)
#define MQTT_MAX_SUBTOPIC_LEN 128   // Maximum length for subtopic parameter

class MQTTManager {
public:
    MQTTManager();
    ~MQTTManager();

    // Configuration
    struct MQTTConfig {
        char server[MQTT_SERVER_MAX_LEN];
        uint16_t port;
        char username[MQTT_USERNAME_MAX_LEN];
        char password[MQTT_PASSWORD_MAX_LEN];
        char clientId[MQTT_CLIENTID_MAX_LEN];
        char hostname[MQTT_HOSTNAME_MAX_LEN];
        char mainTopic[MQTT_MAINTOPIC_MAX_LEN];
        uint16_t publish_interval;  // Status publish interval in seconds
        bool enabled;
    };

    // Initialize MQTT client
    bool begin();

    // Load configuration from JSON
    bool loadConfig(const JsonDocument& doc);

    // Save configuration to JSON
    void saveConfig(JsonDocument& doc);

    // Connect to MQTT broker
    bool connect();

    // Disconnect from broker
    void disconnect();

    // Check if connected
    bool isConnected();

    // Publish message to topic
    bool publish(const char* topic, const char* payload, bool retained = false);

    // Publish to main topic
    bool publishToMainTopic(const char* payload, bool retained = false);

    // Publish to subtopic (hostname-based)
    bool publishToSubtopic(const char* subtopic, const char* payload, bool retained = false);

    // Subscribe to topic
    bool subscribe(const char* topic);

    // Unsubscribe from topic
    bool unsubscribe(const char* topic);

    // Set callback for incoming messages
    void setCallback(std::function<void(char*, uint8_t*, unsigned int)> callback);

    // Must be called in loop()
    void loop();

    // Get configuration
    const MQTTConfig& getConfig() const { return config; }

    // Update configuration
    void updateConfig(const char* server, uint16_t port, const char* username,
                     const char* password, const char* hostname, const char* mainTopic,
                     uint16_t publish_interval, bool enabled);

    // Get connection state
    int getState();

    // Get last error message
    const char* getLastError() const { return lastError; }

private:
    WiFiClient wifiClient;
    PubSubClient mqttClient;
    MQTTConfig config;
    char lastError[128];  // ✅ Buffer estático em vez de String dinâmica
    unsigned long lastReconnectAttempt;

    // Helper para definir erro com formatação
    void setError(const char* format, ...) {
        va_list args;
        va_start(args, format);
        vsnprintf(lastError, sizeof(lastError), format, args);
        va_end(args);
    }

    // Generate client ID
    void generateClientId();

    // Reconnect logic
    bool reconnect();
};

#endif // MQTT_MANAGER_H
