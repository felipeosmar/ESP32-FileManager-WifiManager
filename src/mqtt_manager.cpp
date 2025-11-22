/**
 * MQTT Manager Implementation
 */

#include "mqtt_manager.h"

MQTTManager::MQTTManager() : mqttClient(wifiClient), lastReconnectAttempt(0) {
    // Initialize config with defaults
    strcpy(config.server, "");
    config.port = 1883;
    strcpy(config.username, "");
    strcpy(config.password, "");
    strcpy(config.hostname, "ESP32-Device");
    strcpy(config.mainTopic, "esp32/data");
    config.publish_interval = 60;  // Default: 60 seconds
    config.enabled = false;
    generateClientId();  // Always generate from MAC
}

MQTTManager::~MQTTManager() {
    if (mqttClient.connected()) {
        mqttClient.disconnect();
    }
}

bool MQTTManager::begin() {
    if (!config.enabled) {
        Serial.println("MQTT: Disabled in configuration");
        return false;
    }

    if (strlen(config.server) == 0) {
        lastError = "MQTT server not configured";
        Serial.println("MQTT: Server not configured");
        return false;
    }

    mqttClient.setServer(config.server, config.port);
    Serial.printf("MQTT: Initialized - Server: %s:%d\n", config.server, config.port);

    return true;
}

bool MQTTManager::loadConfig(const JsonDocument& doc) {
    if (!doc.containsKey("mqtt")) {
        Serial.println("MQTT: No configuration found in JSON");
        return false;
    }

    JsonObjectConst mqtt = doc["mqtt"];

    strlcpy(config.server, mqtt["server"] | "", sizeof(config.server));
    config.port = mqtt["port"] | 1883;
    strlcpy(config.username, mqtt["username"] | "", sizeof(config.username));
    strlcpy(config.password, mqtt["password"] | "", sizeof(config.password));
    strlcpy(config.hostname, mqtt["hostname"] | "ESP32-Device", sizeof(config.hostname));
    strlcpy(config.mainTopic, mqtt["main_topic"] | "esp32/data", sizeof(config.mainTopic));
    config.publish_interval = mqtt["publish_interval"] | 60;  // Default: 60 seconds
    config.enabled = mqtt["enabled"] | false;

    // Always generate Client ID from MAC address (ignore saved client_id)
    generateClientId();

    Serial.println("MQTT: Configuration loaded");
    Serial.printf("  Server: %s:%d\n", config.server, config.port);
    Serial.printf("  Username: %s\n", strlen(config.username) > 0 ? config.username : "(none)");
    Serial.printf("  Hostname: %s\n", config.hostname);
    Serial.printf("  Main Topic: %s\n", config.mainTopic);
    Serial.printf("  Client ID: %s (MAC-based)\n", config.clientId);
    Serial.printf("  Enabled: %s\n", config.enabled ? "Yes" : "No");

    return true;
}

void MQTTManager::saveConfig(JsonDocument& doc) {
    JsonObject mqtt = doc["mqtt"].to<JsonObject>();

    mqtt["server"] = config.server;
    mqtt["port"] = config.port;
    mqtt["username"] = config.username;
    mqtt["password"] = config.password;
    mqtt["hostname"] = config.hostname;
    mqtt["main_topic"] = config.mainTopic;
    mqtt["publish_interval"] = config.publish_interval;
    mqtt["client_id"] = config.clientId;  // Save for reference, but will be regenerated on load
    mqtt["enabled"] = config.enabled;
}

void MQTTManager::generateClientId() {
    // Always use full MAC address as Client ID
    uint8_t mac[6];
    WiFi.macAddress(mac);
    snprintf(config.clientId, sizeof(config.clientId),
             "%02X%02X%02X%02X%02X%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

bool MQTTManager::connect() {
    if (!config.enabled) {
        lastError = "MQTT is disabled";
        return false;
    }

    if (mqttClient.connected()) {
        return true;
    }

    Serial.printf("MQTT: Connecting to %s:%d as %s...\n",
                  config.server, config.port, config.clientId);

    bool connected = false;

    if (strlen(config.username) > 0) {
        // Connect with username and password
        connected = mqttClient.connect(config.clientId, config.username, config.password);
    } else {
        // Connect without authentication
        connected = mqttClient.connect(config.clientId);
    }

    if (connected) {
        Serial.println("MQTT: Connected successfully");
        lastError = "";

        // Publish connection status
        String statusTopic = String(config.mainTopic) + "/status";
        publish(statusTopic.c_str(), "online", true);

        return true;
    } else {
        int state = mqttClient.state();
        Serial.printf("MQTT: Connection failed, state=%d\n", state);

        switch (state) {
            case -4:
                lastError = "Connection timeout";
                break;
            case -3:
                lastError = "Connection lost";
                break;
            case -2:
                lastError = "Connect failed";
                break;
            case -1:
                lastError = "Disconnected";
                break;
            case 1:
                lastError = "Bad protocol";
                break;
            case 2:
                lastError = "Bad client ID";
                break;
            case 3:
                lastError = "Server unavailable";
                break;
            case 4:
                lastError = "Bad credentials";
                break;
            case 5:
                lastError = "Not authorized";
                break;
            default:
                lastError = "Unknown error";
                break;
        }

        return false;
    }
}

void MQTTManager::disconnect() {
    if (mqttClient.connected()) {
        // Publish offline status before disconnecting
        String statusTopic = String(config.mainTopic) + "/status";
        publish(statusTopic.c_str(), "offline", true);

        mqttClient.disconnect();
        Serial.println("MQTT: Disconnected");
    }
}

bool MQTTManager::isConnected() {
    return mqttClient.connected();
}

bool MQTTManager::publish(const char* topic, const char* payload, bool retained) {
    if (!mqttClient.connected()) {
        lastError = "Not connected to MQTT broker";
        return false;
    }

    bool success = mqttClient.publish(topic, payload, retained);

    if (success) {
        Serial.printf("MQTT: Published to %s: %s\n", topic, payload);
    } else {
        Serial.printf("MQTT: Failed to publish to %s\n", topic);
        lastError = "Publish failed";
    }

    return success;
}

bool MQTTManager::publishToMainTopic(const char* payload, bool retained) {
    return publish(config.mainTopic, payload, retained);
}

bool MQTTManager::publishToSubtopic(const char* subtopic, const char* payload, bool retained) {
    if (!mqttClient.connected()) {
        lastError = "Not connected to MQTT broker";
        return false;
    }

    // Build topic: mainTopic/hostname/subtopic
    char fullTopic[128];
    snprintf(fullTopic, sizeof(fullTopic), "%s/%s/%s",
             config.mainTopic, config.hostname, subtopic);

    return publish(fullTopic, payload, retained);
}

bool MQTTManager::subscribe(const char* topic) {
    if (!mqttClient.connected()) {
        lastError = "Not connected to MQTT broker";
        return false;
    }

    bool success = mqttClient.subscribe(topic);

    if (success) {
        Serial.printf("MQTT: Subscribed to %s\n", topic);
    } else {
        Serial.printf("MQTT: Failed to subscribe to %s\n", topic);
        lastError = "Subscribe failed";
    }

    return success;
}

bool MQTTManager::unsubscribe(const char* topic) {
    if (!mqttClient.connected()) {
        return false;
    }

    bool success = mqttClient.unsubscribe(topic);

    if (success) {
        Serial.printf("MQTT: Unsubscribed from %s\n", topic);
    }

    return success;
}

void MQTTManager::setCallback(std::function<void(char*, uint8_t*, unsigned int)> callback) {
    mqttClient.setCallback(callback);
}

bool MQTTManager::reconnect() {
    unsigned long now = millis();

    // Try to reconnect every 5 seconds
    if (now - lastReconnectAttempt < 5000) {
        return false;
    }

    lastReconnectAttempt = now;

    if (connect()) {
        lastReconnectAttempt = 0;
        return true;
    }

    return false;
}

void MQTTManager::loop() {
    if (!config.enabled) {
        return;
    }

    if (!mqttClient.connected()) {
        reconnect();
    } else {
        mqttClient.loop();
    }
}

int MQTTManager::getState() {
    return mqttClient.state();
}

void MQTTManager::updateConfig(const char* server, uint16_t port, const char* username,
                               const char* password, const char* hostname, const char* mainTopic,
                               uint16_t publish_interval, bool enabled) {
    bool wasConnected = mqttClient.connected();

    if (wasConnected) {
        disconnect();
    }

    strlcpy(config.server, server, sizeof(config.server));
    config.port = port;
    strlcpy(config.username, username, sizeof(config.username));
    strlcpy(config.password, password, sizeof(config.password));
    strlcpy(config.hostname, hostname, sizeof(config.hostname));
    strlcpy(config.mainTopic, mainTopic, sizeof(config.mainTopic));
    config.publish_interval = publish_interval;
    config.enabled = enabled;

    // Client ID is always based on MAC, so regenerate it
    generateClientId();

    Serial.println("MQTT: Configuration updated");
    Serial.printf("  Hostname: %s\n", config.hostname);
    Serial.printf("  Publish Interval: %d seconds\n", config.publish_interval);
    Serial.printf("  Client ID: %s (MAC-based)\n", config.clientId);

    if (config.enabled) {
        mqttClient.setServer(config.server, config.port);
        if (wasConnected || strlen(server) > 0) {
            connect();
        }
    }
}
