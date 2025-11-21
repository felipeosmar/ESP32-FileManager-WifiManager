/**
 * OLED Display Manager Implementation
 */

#include "oled_manager.h"

OLEDManager::OLEDManager()
    : display(nullptr),
      i2cMutex(nullptr),
      displayAvailable(false),
      currentMode(MODE_OFF),
      lastUpdate(0),
      updateInterval(2000) {

    // Default configuration
    config.enabled = true;
    config.address = 0x3C;  // Most common I2C address
    config.sda_pin = 4;     // Custom SDA pin
    config.scl_pin = 15;    // Custom SCL pin
    config.rst_pin = 16;    // Custom RST pin
    config.auto_update = true;
    config.brightness = 128;
    config.flip_display = false;
}

OLEDManager::~OLEDManager() {
    if (display != nullptr) {
        if (displayAvailable) {
            display->clearDisplay();
            display->display();
        }
        delete display;
    }
}

bool OLEDManager::begin(SemaphoreHandle_t mutex) {
    if (!config.enabled) {
        Serial.println("OLED: Disabled in configuration");
        return false;
    }

    // Store mutex
    i2cMutex = mutex;

    // NOTE: Wire.begin() should be called BEFORE this function in main.cpp
    // to avoid conflicts with other I2C devices on the same bus

    // Create the display object AFTER Wire.begin() has been called
    display = new Adafruit_SSD1306(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

    // Configure reset pin if specified
    if (config.rst_pin >= 0) {
        pinMode(config.rst_pin, OUTPUT);
        digitalWrite(config.rst_pin, LOW);
        delay(10);
        digitalWrite(config.rst_pin, HIGH);
        delay(10);
    }

    // Try to initialize display
    bool initSuccess = false;
    if (i2cMutex != nullptr) {
        if (xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
            initSuccess = display->begin(SSD1306_SWITCHCAPVCC, config.address);
            xSemaphoreGive(i2cMutex);
        } else {
            Serial.println("OLED: Failed to acquire mutex for initialization");
        }
    } else {
        initSuccess = display->begin(SSD1306_SWITCHCAPVCC, config.address);
    }

    if (!initSuccess) {
        lastError = "Display not found at address 0x" + String(config.address, HEX);
        Serial.printf("OLED: %s\n", lastError.c_str());
        displayAvailable = false;
        delete display;
        display = nullptr;
        return false;
    }

    displayAvailable = true;
    Serial.printf("OLED: Initialized at address 0x%02X (SDA=%d, SCL=%d, RST=%d)\n",
                  config.address, config.sda_pin, config.scl_pin, config.rst_pin);

    // Configure display
    setBrightness(config.brightness);
    setFlip(config.flip_display);

    // Clear display
    display->clearDisplay();
    display->setTextColor(SSD1306_WHITE);
    
    if (i2cMutex != nullptr) {
        if (xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(100))) {
            display->display();
            xSemaphoreGive(i2cMutex);
        }
    } else {
        display->display();
    }

    // Show logo on startup
    showLogo();

    return true;
}

bool OLEDManager::loadConfig(const JsonDocument& doc) {
    if (!doc.containsKey("oled")) {
        Serial.println("OLED: No configuration found in JSON");
        return false;
    }

    JsonObjectConst oled = doc["oled"];

    config.enabled = oled["enabled"] | true;
    config.address = oled["address"] | 0x3C;
    config.sda_pin = oled["sda_pin"] | 4;
    config.scl_pin = oled["scl_pin"] | 15;
    config.rst_pin = oled["rst_pin"] | 16;
    config.auto_update = oled["auto_update"] | true;
    config.brightness = oled["brightness"] | 128;
    config.flip_display = oled["flip_display"] | false;

    Serial.println("OLED: Configuration loaded");
    Serial.printf("  Enabled: %s\n", config.enabled ? "Yes" : "No");
    Serial.printf("  Address: 0x%02X\n", config.address);
    Serial.printf("  SDA Pin: %d\n", config.sda_pin);
    Serial.printf("  SCL Pin: %d\n", config.scl_pin);
    Serial.printf("  RST Pin: %d\n", config.rst_pin);
    Serial.printf("  Auto Update: %s\n", config.auto_update ? "Yes" : "No");

    return true;
}

void OLEDManager::saveConfig(JsonDocument& doc) {
    JsonObject oled = doc["oled"].to<JsonObject>();

    oled["enabled"] = config.enabled;
    oled["address"] = config.address;
    oled["sda_pin"] = config.sda_pin;
    oled["scl_pin"] = config.scl_pin;
    oled["rst_pin"] = config.rst_pin;
    oled["auto_update"] = config.auto_update;
    oled["brightness"] = config.brightness;
    oled["flip_display"] = config.flip_display;
}

void OLEDManager::updateConfig(bool enabled, uint8_t address, uint8_t sda, uint8_t scl, int8_t rst,
                               bool autoUpdate, uint8_t brightness, bool flip) {
    config.enabled = enabled;
    config.address = address;
    config.sda_pin = sda;
    config.scl_pin = scl;
    config.rst_pin = rst;
    config.auto_update = autoUpdate;
    config.brightness = brightness;
    config.flip_display = flip;

    if (displayAvailable) {
        setBrightness(brightness);
        setFlip(flip);
    }

    Serial.println("OLED: Configuration updated");
}

void OLEDManager::clear() {
    if (!displayAvailable) return;
    display->clearDisplay();
    if (i2cMutex != nullptr) {
        if (xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(100))) {
            display->display();
            xSemaphoreGive(i2cMutex);
        }
    } else {
        display->display();
    }
}

void OLEDManager::turnOn() {
    if (!displayAvailable) return;
    if (i2cMutex != nullptr) {
        if (xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(100))) {
            display->ssd1306_command(SSD1306_DISPLAYON);
            xSemaphoreGive(i2cMutex);
        }
    } else {
        display->ssd1306_command(SSD1306_DISPLAYON);
    }
}

void OLEDManager::turnOff() {
    if (!displayAvailable) return;
    if (i2cMutex != nullptr) {
        if (xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(100))) {
            display->ssd1306_command(SSD1306_DISPLAYOFF);
            xSemaphoreGive(i2cMutex);
        }
    } else {
        display->ssd1306_command(SSD1306_DISPLAYOFF);
    }
}

void OLEDManager::setBrightness(uint8_t brightness) {
    if (!displayAvailable) return;
    if (i2cMutex != nullptr) {
        if (xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(100))) {
            display->ssd1306_command(SSD1306_SETCONTRAST);
            display->ssd1306_command(brightness);
            xSemaphoreGive(i2cMutex);
        }
    } else {
        display->ssd1306_command(SSD1306_SETCONTRAST);
        display->ssd1306_command(brightness);
    }
    config.brightness = brightness;
}

void OLEDManager::setFlip(bool flip) {
    if (!displayAvailable) return;

    if (flip) {
        display->setRotation(2);  // 180 degrees
    } else {
        display->setRotation(0);  // Normal
    }

    config.flip_display = flip;
}

void OLEDManager::showLogo() {
    if (!displayAvailable) return;

    currentMode = MODE_LOGO;
    display->clearDisplay();

    // Draw ESP32 logo text
    display->setTextSize(2);
    drawCenteredText("ESP32", 10);

    display->setTextSize(1);
    drawCenteredText("File Manager", 30);
    drawCenteredText("+ MQTT Client", 42);
    drawCenteredText("+ OLED Display", 54);

    if (i2cMutex != nullptr) {
        if (xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(100))) {
            display->display();
            xSemaphoreGive(i2cMutex);
        }
    } else {
        display->display();
    }
}

void OLEDManager::showSystemInfo(const char* ip, unsigned long uptime, size_t freeHeap) {
    if (!displayAvailable) return;

    currentMode = MODE_SYSTEM_INFO;
    display->clearDisplay();

    // Title
    display->setTextSize(1);
    display->setCursor(0, 0);
    display->println("System Info");
    display->drawLine(0, 10, SCREEN_WIDTH, 10, SSD1306_WHITE);

    // IP Address
    display->setCursor(0, 14);
    display->print("IP: ");
    display->println(ip);

    // Uptime
    display->setCursor(0, 26);
    display->print("Uptime: ");
    display->println(formatUptime(uptime).c_str());

    // Free Heap
    display->setCursor(0, 38);
    display->print("Heap: ");
    display->println(formatBytes(freeHeap).c_str());

    // Heap usage bar
    uint32_t totalHeap = ESP.getHeapSize();
    uint8_t heapUsage = ((totalHeap - freeHeap) * 100) / totalHeap;
    display->setCursor(0, 50);
    display->print("Usage: ");
    display->print(heapUsage);
    display->println("%");
    drawProgressBar(0, 62, SCREEN_WIDTH, 2, heapUsage);

    if (i2cMutex != nullptr) {
        if (xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(100))) {
            display->display();
            xSemaphoreGive(i2cMutex);
        }
    } else {
        display->display();
    }
}

void OLEDManager::showNetworkInfo(const char* ssid, int rssi, const char* ip) {
    if (!displayAvailable) return;

    currentMode = MODE_NETWORK_INFO;
    display->clearDisplay();

    // Title
    display->setTextSize(1);
    display->setCursor(0, 0);
    display->println("Network Info");
    display->drawLine(0, 10, SCREEN_WIDTH, 10, SSD1306_WHITE);

    // SSID
    display->setCursor(0, 14);
    display->print("SSID: ");
    display->println(ssid);

    // Signal strength
    display->setCursor(0, 26);
    display->print("Signal: ");
    display->print(rssi);
    display->println(" dBm");

    // Signal quality bar
    uint8_t quality = 0;
    if (rssi > -50) quality = 100;
    else if (rssi > -60) quality = 80;
    else if (rssi > -70) quality = 60;
    else if (rssi > -80) quality = 40;
    else quality = 20;

    drawProgressBar(0, 36, SCREEN_WIDTH, 4, quality);

    // IP Address
    display->setCursor(0, 44);
    display->print("IP: ");
    display->println(ip);

    if (i2cMutex != nullptr) {
        if (xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(100))) {
            display->display();
            xSemaphoreGive(i2cMutex);
        }
    } else {
        display->display();
    }
}

void OLEDManager::showMQTTInfo(bool connected, const char* server, const char* topic) {
    if (!displayAvailable) return;

    currentMode = MODE_MQTT_INFO;
    display->clearDisplay();

    // Title
    display->setTextSize(1);
    display->setCursor(0, 0);
    display->println("MQTT Status");
    display->drawLine(0, 10, SCREEN_WIDTH, 10, SSD1306_WHITE);

    // Connection status
    display->setCursor(0, 14);
    display->print("Status: ");
    if (connected) {
        display->println("Connected");
    } else {
        display->println("Disconnected");
    }

    // Server
    display->setCursor(0, 26);
    display->print("Server:");
    display->setCursor(0, 36);
    display->println(server);

    // Topic
    display->setCursor(0, 48);
    display->print("Topic:");
    display->setCursor(0, 56);
    display->println(topic);

    if (i2cMutex != nullptr) {
        if (xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(100))) {
            display->display();
            xSemaphoreGive(i2cMutex);
        }
    } else {
        display->display();
    }
}

void OLEDManager::showCustomText(const char* line1, const char* line2,
                                 const char* line3, const char* line4) {
    if (!displayAvailable) return;

    currentMode = MODE_CUSTOM_TEXT;
    display->clearDisplay();

    display->setTextSize(1);

    if (line1) {
        display->setCursor(0, 0);
        display->println(line1);
    }

    if (line2) {
        display->setCursor(0, 16);
        display->println(line2);
    }

    if (line3) {
        display->setCursor(0, 32);
        display->println(line3);
    }

    if (line4) {
        display->setCursor(0, 48);
        display->println(line4);
    }

    if (i2cMutex != nullptr) {
        if (xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(100))) {
            display->display();
            xSemaphoreGive(i2cMutex);
        }
    } else {
        display->display();
    }
}

void OLEDManager::showSensorInfo(bool available, float temperature, float humidity, bool fahrenheit) {
    if (!displayAvailable) return;

    currentMode = MODE_SENSOR_INFO;
    display->clearDisplay();

    // Title
    display->setTextSize(1);
    display->setCursor(0, 0);
    display->println("SHT20 Sensor");
    display->drawLine(0, 10, SCREEN_WIDTH, 10, SSD1306_WHITE);

    if (!available) {
        display->setCursor(0, 24);
        display->println("Sensor not detected");
        if (i2cMutex != nullptr) {
            if (xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(100))) {
                display->display();
                xSemaphoreGive(i2cMutex);
            }
        } else {
            display->display();
        }
        return;
    }

    // Temperature
    display->setCursor(0, 14);
    display->print("Temp");
    display->setCursor(0, 26);
    display->setTextSize(2);
    display->print(temperature, 1);
    if (fahrenheit) {
        display->print("F");
    } else {
        display->print("C");
    }

    // Humidity
    display->setTextSize(1);
    display->setCursor(64, 14);
    display->print("Hum");
    display->setCursor(64, 26);
    display->setTextSize(2);
    display->print(humidity, 1);
    display->print("%");

    if (i2cMutex != nullptr) {
        if (xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(100))) {
            display->display();
            xSemaphoreGive(i2cMutex);
        }
    } else {
        display->display();
    }
}

void OLEDManager::update() {
    if (!displayAvailable || !config.auto_update) return;

    unsigned long now = millis();
    if (now - lastUpdate < updateInterval) return;

    lastUpdate = now;

    // Auto-update based on current mode
    switch (currentMode) {
        case MODE_SYSTEM_INFO:
            // Will be updated from main loop with current data
            break;
        case MODE_NETWORK_INFO:
            // Will be updated from main loop with current data
            break;
        case MODE_MQTT_INFO:
            // Will be updated from main loop with current data
            break;
        default:
            break;
    }
}

void OLEDManager::setMode(DisplayMode mode) {
    currentMode = mode;

    if (mode == MODE_OFF) {
        turnOff();
    } else {
        turnOn();
    }
}

// Helper functions
void OLEDManager::drawCenteredText(const char* text, int16_t y, uint8_t textSize) {
    display->setTextSize(textSize);

    int16_t x1, y1;
    uint16_t w, h;
    display->getTextBounds(text, 0, 0, &x1, &y1, &w, &h);

    int16_t x = (SCREEN_WIDTH - w) / 2;
    display->setCursor(x, y);
    display->println(text);
}

void OLEDManager::drawProgressBar(int16_t x, int16_t y, int16_t width, int16_t height, uint8_t progress) {
    // Clamp progress to 0-100
    if (progress > 100) progress = 100;

    // Draw border
    display->drawRect(x, y, width, height, SSD1306_WHITE);

    // Fill progress
    int16_t fillWidth = ((width - 2) * progress) / 100;
    if (fillWidth > 0) {
        display->fillRect(x + 1, y + 1, fillWidth, height - 2, SSD1306_WHITE);
    }
}

String OLEDManager::formatUptime(unsigned long seconds) {
    unsigned long days = seconds / 86400;
    unsigned long hours = (seconds % 86400) / 3600;
    unsigned long minutes = (seconds % 3600) / 60;

    String result = "";
    if (days > 0) {
        result += String(days) + "d ";
    }
    result += String(hours) + "h " + String(minutes) + "m";

    return result;
}

String OLEDManager::formatBytes(size_t bytes) {
    if (bytes < 1024) {
        return String(bytes) + " B";
    } else if (bytes < 1024 * 1024) {
        return String(bytes / 1024) + " KB";
    } else {
        return String(bytes / (1024 * 1024)) + " MB";
    }
}
