/**
 * ESP32 File Manager with WiFi Manager and OTA Updates
 *
 * Hardware:
 * - ESP32 (any board)
 * - NO SD Card needed - uses internal SPIFFS/LittleFS
 *
 * Features:
 * - WiFi Manager (AP mode and Station mode)
 * - Web interface served from SPIFFS
 * - File manager (upload, download, edit, delete) on SPIFFS
 * - Configuration via JSON file on SPIFFS
 * - Over-the-air (OTA) firmware updates
 */

#include <Arduino.h>
#include <WiFi.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <esp_task_wdt.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include "config.h"
#include "web_server.h"
#include "spiffs_manager.h"
#include "mqtt_manager.h"
#include "oled_manager.h"
#include "sht20_manager.h"

// Global objects
WebServerManager webServerManager;
SPIFFSManager spiffsManager;
MQTTManager mqttManager;
OLEDManager oledManager;
SHT20Manager sht20Manager;

// NTP Client
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", -10800, 60000); // UTC-3 (Brazil), update every 60s

// Mutex for SPIFFS access (prevents concurrent access issues)
SemaphoreHandle_t spiffsMutex = NULL;

// Mutex for I2C bus access (prevents concurrent access between OLED and SHT20)
SemaphoreHandle_t i2cMutex = NULL;

// Configuration
struct Config {
  char ssid[32];
  char password[64];
  bool apMode;
} config;

// Function declarations
void setupWiFi();
bool loadConfig();
void setDefaultConfig();
void scanI2CBus();
void log(const String& msg);

void setup() {
  Serial.begin(SERIAL_BAUD_RATE);
  log("\n\n=== ESP32 File Manager (SPIFFS) ===");

  // Create mutex for SPIFFS access
  spiffsMutex = xSemaphoreCreateMutex();
  if (spiffsMutex == NULL) {
    log("Failed to create SPIFFS mutex!");
  }

  // Create mutex for I2C bus access
  i2cMutex = xSemaphoreCreateMutex();
  if (i2cMutex == NULL) {
    log("Failed to create I2C mutex!");
  }

  // Initialize SPIFFS
  log("Initializing LittleFS...");
  if (!spiffsManager.begin()) {
    log("SPIFFS initialization failed!");
    log("ERROR: Cannot continue without SPIFFS");
    while(1) {
      delay(1000);
      Serial.println("System halted - SPIFFS required");
    }
  } else {
    log("LittleFS initialized successfully");
  }

  // Load configuration from SPIFFS
  if (!loadConfig()) {
    log("Failed to load config, using defaults");
    setDefaultConfig();
  }

  // Initialize I2C bus FIRST, before any I2C device initialization
  // Both OLED and SHT20 share the same I2C bus (SDA=4, SCL=15)
  // CRITICAL: This must be done BEFORE creating/initializing I2C devices
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  Wire.setClock(I2C_CLOCK_SPEED);
  Serial.printf("I2C: Initialized (SDA=%d, SCL=%d, %dHz)\n", PIN_I2C_SDA, PIN_I2C_SCL, I2C_CLOCK_SPEED);

  // Scan I2C bus for devices
  scanI2CBus();

  // Setup WiFi
  setupWiFi();

  // Initialize NTP
  timeClient.begin();
  log("NTP Client started");

  // Setup MQTT (after WiFi is connected)
  if (mqttManager.begin()) {
    mqttManager.connect();
  }

  // Setup OLED Display
  if (oledManager.begin(i2cMutex)) {
    // Show logo on startup
    oledManager.showLogo();
    delay(2000);
    // Switch to sensor info after delay
    oledManager.setMode(OLEDManager::MODE_SENSOR_INFO);
  }

  // Setup SHT20 Sensor (uses same I2C bus as OLED)
  if (sht20Manager.begin(&Wire, i2cMutex)) {
    log("SHT20 sensor ready");
  }

  // Setup web server
  webServerManager.begin(&spiffsManager, &mqttManager, &oledManager, &sht20Manager, &spiffsMutex);

  log("\n=== System Ready ===");
  Serial.print("Web interface: http://");
  Serial.print(WiFi.localIP());
  Serial.println("/");
  log("====================\n");
}

void loop() {
  // Update NTP
  timeClient.update();
  
  // Update WebServer (WebSocket)
  webServerManager.loop();

  // MQTT loop - handle reconnection and message processing
  mqttManager.loop();

  // OLED display update
  if (oledManager.isAvailable() && oledManager.getConfig().auto_update) {
    // Update display with current system info based on mode
    if (oledManager.getMode() == OLEDManager::MODE_SYSTEM_INFO) {
      oledManager.showSystemInfo(WiFi.localIP().toString().c_str(),
                                  millis() / 1000,
                                  ESP.getFreeHeap());
    } else if (oledManager.getMode() == OLEDManager::MODE_NETWORK_INFO) {
      oledManager.showNetworkInfo(WiFi.SSID().c_str(),
                                   WiFi.RSSI(),
                                   WiFi.localIP().toString().c_str());
    } else if (oledManager.getMode() == OLEDManager::MODE_MQTT_INFO) {
      const MQTTManager::MQTTConfig& mqttCfg = mqttManager.getConfig();
      oledManager.showMQTTInfo(mqttManager.isConnected(),
                               mqttCfg.server,
                               mqttCfg.mainTopic);
    } else if (oledManager.getMode() == OLEDManager::MODE_SENSOR_INFO) {
      const SHT20Manager::SHT20Config& sensorCfg = sht20Manager.getConfig();
      oledManager.showSensorInfo(sht20Manager.isAvailable(),
                                  sht20Manager.getTemperature(),
                                  sht20Manager.getHumidity(),
                                  sensorCfg.fahrenheit);
    }
  }

  // SHT20 sensor update
  sht20Manager.update();

  // Small delay to prevent watchdog issues
  delay(10);
}

void setupWiFi() {
  log("Setting up WiFi...");

  if (config.apMode) {
    // Access Point mode
    WiFi.softAP(config.ssid, config.password);
    Serial.print("AP Mode - SSID: ");
    Serial.println(config.ssid);
    Serial.print("IP Address: ");
    Serial.println(WiFi.softAPIP());
  } else {
    // Station mode
    WiFi.begin(config.ssid, config.password);
    Serial.print("Connecting to WiFi");

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 30) {
      delay(500);
      Serial.print(".");
      attempts++;
    }

    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\nConnected!");
      Serial.print("IP Address: ");
      Serial.println(WiFi.localIP());
    } else {
      Serial.println("\nFailed to connect, switching to AP mode");
      WiFi.softAP(WIFI_SSID_DEFAULT, WIFI_PASS_DEFAULT);
      Serial.print("AP IP: ");
      Serial.println(WiFi.softAPIP());
    }
  }
}

void scanI2CBus() {
    byte error, address;
    int nDevices;
 
    log("Scanning I2C bus...");
 
    nDevices = 0;
    for(address = 1; address < 127; address++ )
    {
        // The i2c_scanner uses the return value of
        // the Write.endTransmisstion to see if
        // a device did acknowledge to the address.
        Wire.beginTransmission(address);
        error = Wire.endTransmission();
 
        if (error == 0)
        {
            Serial.print("I2C device found at address 0x");
            if (address<16)
                Serial.print("0");
            Serial.print(address,HEX);
            Serial.println("  !");
 
            nDevices++;
        }
        else if (error==4)
        {
            Serial.print("Unknown error at address 0x");
            if (address<16)
                Serial.print("0");
            Serial.println(address,HEX);
        }    
    }
    if (nDevices == 0)
        log("No I2C devices found\n");
    else
        log("done\n");
}

bool loadConfig() {
  if (!spiffsManager.isReady()) return false;

  File file = LittleFS.open("/config.json", FILE_READ);
  if (!file) {
    log("Config file not found");
    return false;
  }

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, file);
  file.close();

  if (error) {
    log("Failed to parse config");
    return false;
  }

  strlcpy(config.ssid, doc["wifi"]["ssid"] | WIFI_SSID_DEFAULT, sizeof(config.ssid));
  strlcpy(config.password, doc["wifi"]["password"] | WIFI_PASS_DEFAULT, sizeof(config.password));
  config.apMode = doc["wifi"]["ap_mode"] | WIFI_AP_MODE_DEFAULT;

  // Load MQTT configuration
  mqttManager.loadConfig(doc);

  // Load OLED configuration
  oledManager.loadConfig(doc);

  // Load SHT20 configuration
  sht20Manager.loadConfig(doc);

  log("Configuration loaded from SPIFFS");
  return true;
}

void setDefaultConfig() {
  strcpy(config.ssid, WIFI_SSID_DEFAULT);
  strcpy(config.password, WIFI_PASS_DEFAULT);
  config.apMode = WIFI_AP_MODE_DEFAULT;
}

void log(const String& msg) {
  Serial.println(msg);
  webServerManager.broadcastLog(msg);
}
