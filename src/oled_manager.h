/**
 * OLED Display Manager
 * Manages 128x64 I2C OLED display (SSD1306)
 */

#ifndef OLED_MANAGER_H
#define OLED_MANAGER_H

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ArduinoJson.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1  // Reset pin not used

class OLEDManager {
public:
    OLEDManager();
    ~OLEDManager();

    // Configuration structure
    struct OLEDConfig {
        bool enabled;
        uint8_t address;  // I2C address (usually 0x3C or 0x3D)
        uint8_t sda_pin;  // SDA pin
        uint8_t scl_pin;  // SCL pin
        int8_t rst_pin;   // RST (Reset) pin (-1 for no reset pin)
        bool auto_update; // Auto update with system info
        uint8_t brightness; // Brightness 0-255
        bool flip_display; // Rotate 180 degrees
    };

    // Display modes
    enum DisplayMode {
        MODE_OFF,
        MODE_LOGO,
        MODE_SYSTEM_INFO,
        MODE_NETWORK_INFO,
        MODE_MQTT_INFO,
        MODE_CUSTOM_TEXT
    };

    // Initialize display
    bool begin();

    // Load configuration from JSON
    bool loadConfig(const JsonDocument& doc);

    // Save configuration to JSON
    void saveConfig(JsonDocument& doc);

    // Update configuration
    void updateConfig(bool enabled, uint8_t address, uint8_t sda, uint8_t scl, int8_t rst,
                     bool autoUpdate, uint8_t brightness, bool flip);

    // Get configuration
    const OLEDConfig& getConfig() const { return config; }

    // Check if display is available
    bool isAvailable() const { return displayAvailable; }

    // Display control
    void clear();
    void turnOn();
    void turnOff();
    void setBrightness(uint8_t brightness);
    void setFlip(bool flip);

    // Display modes
    void showLogo();
    void showSystemInfo(const char* ip, unsigned long uptime, size_t freeHeap);
    void showNetworkInfo(const char* ssid, int rssi, const char* ip);
    void showMQTTInfo(bool connected, const char* server, const char* topic);
    void showCustomText(const char* line1, const char* line2 = nullptr,
                       const char* line3 = nullptr, const char* line4 = nullptr);

    // Auto update (call in loop)
    void update();

    // Set current mode
    void setMode(DisplayMode mode);
    DisplayMode getMode() const { return currentMode; }

    // Get last error
    String getLastError() const { return lastError; }

private:
    Adafruit_SSD1306 display;
    OLEDConfig config;
    bool displayAvailable;
    DisplayMode currentMode;
    String lastError;
    unsigned long lastUpdate;
    uint16_t updateInterval;

    // Helper functions
    void drawCenteredText(const char* text, int16_t y, uint8_t textSize = 1);
    void drawProgressBar(int16_t x, int16_t y, int16_t width, int16_t height, uint8_t progress);
    String formatUptime(unsigned long seconds);
    String formatBytes(size_t bytes);
};

#endif // OLED_MANAGER_H
