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
#include "config.h"
#include "web_server.h"
#include "spiffs_manager.h"
#include "mqtt_manager.h"
#include "oled_manager.h"
#include "sensor_manager.h"
#include "ntp_manager.h"
#include "lorawan_manager.h"

// Global objects
WebServerManager webServerManager;
SPIFFSManager spiffsManager;
MQTTManager mqttManager;
OLEDManager oledManager;
SensorManager sensorManager;
NTPManager ntpManager;
LoRaWANManager lorawanManager;

// Mutex for SPIFFS access (prevents concurrent access issues)
SemaphoreHandle_t spiffsMutex = NULL;

// Mutex for I2C bus access (prevents concurrent access between OLED and SHT20)
SemaphoreHandle_t i2cMutex = NULL;

// MQTT status publishing
unsigned long lastStatusPublish = 0;

// OLED display update timing
unsigned long lastOLEDUpdate = 0;
const unsigned long OLED_UPDATE_INTERVAL = 2000; // Update OLED every 2 seconds

// WiFi connection status
bool wifiConnectedToInternet = false;

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
void publishSystemStatus();

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

  // Initialize NTP (only if connected to internet)
  if (wifiConnectedToInternet) {
    ntpManager.begin();
  } else {
    log("NTP Client skipped (no internet connection)");
  }

  // Setup MQTT (only if connected to internet)
  if (wifiConnectedToInternet && mqttManager.begin()) {
    mqttManager.connect();
  } else if (!wifiConnectedToInternet) {
    log("MQTT skipped (no internet connection)");
  }

  // Setup OLED Display
  if (oledManager.begin(i2cMutex)) {
    // Show logo on startup
    oledManager.showLogo();
    delay(2000);
    // Switch to sensor info after delay
    oledManager.setMode(OLEDManager::MODE_SENSOR_INFO);
  }

  // Setup Temperature/Humidity Sensor (uses same I2C bus as OLED)
  // Supports: SHT20, SHT30, SHT40, AM2315 with auto-detection
  if (sensorManager.begin(&Wire, i2cMutex)) {
    char sensorName[32];
    sensorManager.getDetectedSensorName(sensorName, sizeof(sensorName));
    log(String("Sensor ready: ") + sensorName);  // Temporariamente usando String para log
  } else {
    log("No temperature/humidity sensor detected");
  }

  // Setup LoRaWAN (SX1276 radio via SPI)
  if (lorawanManager.begin()) {
    LoRaWANManager::LoRaWANConfig loraCfg = lorawanManager.getConfig();
    if (loraCfg.enabled) {
      log("LoRaWAN radio initialized");
    } else {
      log("LoRaWAN initialized but disabled in config");
    }
  } else {
    log("LoRaWAN radio not available");
  }

  // Setup web server
  webServerManager.begin(&spiffsManager, &mqttManager, &oledManager, &sensorManager, &ntpManager, &lorawanManager, &spiffsMutex);

  log("\n=== System Ready ===");
  Serial.print("Web interface: http://");
  Serial.print(WiFi.localIP());
  Serial.println("/");
  log("====================\n");
}

void loop() {
  // Update NTP
  ntpManager.update();

  // Update WebServer (WebSocket)
  webServerManager.loop();

  // MQTT loop - handle reconnection and message processing (only if connected to internet)
  if (wifiConnectedToInternet) {
    mqttManager.loop();
  }

  // OLED display update (rate-limited to prevent mutex contention)
  if (oledManager.isAvailable() && oledManager.getConfig().auto_update) {
    unsigned long now = millis();
    if (now - lastOLEDUpdate >= OLED_UPDATE_INTERVAL) {
      lastOLEDUpdate = now;
      
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
        const SensorManager::SensorConfig& sensorCfg = sensorManager.getConfig();
        oledManager.showSensorInfo(sensorManager.isAvailable(),
                                    sensorManager.getTemperature(),
                                    sensorManager.getHumidity(),
                                    sensorCfg.fahrenheit);
      }
    }
  }

  // Sensor update (auto-reads at configured interval)
  sensorManager.update();

  // LoRaWAN update (process events, handle downlinks, manage uplink timing)
  lorawanManager.update();

  // Publish system status to MQTT periodically (only if connected to internet)
  if (wifiConnectedToInternet) {
    unsigned long now = millis();
    const MQTTManager::MQTTConfig& mqttConfig = mqttManager.getConfig();
    unsigned long publishInterval = mqttConfig.publish_interval * 1000UL;  // Convert seconds to milliseconds

    if (mqttManager.isConnected() && (now - lastStatusPublish >= publishInterval)) {
      publishSystemStatus();
      lastStatusPublish = now;
    }
  }

  // Small delay to prevent watchdog issues
  delay(10);
}

void setupWiFi() {
  log("Setting up WiFi...");

  if (config.apMode) {
    // Access Point mode
    log("Starting in AP mode (configured)");
    WiFi.mode(WIFI_AP);
    WiFi.softAP(config.ssid, config.password);
    Serial.print("AP Mode - SSID: ");
    Serial.println(config.ssid);
    Serial.print("IP Address: ");
    Serial.println(WiFi.softAPIP());
    wifiConnectedToInternet = false;
  } else {
    // Station mode
    log("Attempting to connect to WiFi...");
    WiFi.mode(WIFI_STA);
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
      wifiConnectedToInternet = true;
    } else {
      Serial.println("\nFailed to connect, switching to AP mode");

      // IMPORTANT: Disconnect from station mode completely before starting AP
      WiFi.disconnect(true);
      delay(100);

      // Switch to AP mode only
      WiFi.mode(WIFI_AP);
      WiFi.softAP(WIFI_SSID_DEFAULT, WIFI_PASS_DEFAULT);
      Serial.print("AP IP: ");
      Serial.println(WiFi.softAPIP());
      wifiConnectedToInternet = false;

      log("Now in fallback AP mode - no internet connection");
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

  StaticJsonDocument<1536> doc;  // Config: wifi + mqtt + oled + sensor
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

  // Load Sensor configuration
  sensorManager.loadConfig(doc);

  // Load NTP configuration
  ntpManager.loadConfig(doc);

  // Load LoRaWAN configuration
  lorawanManager.loadConfig(doc);

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

void publishSystemStatus() {
  StaticJsonDocument<1024> doc;  // System status: uptime, memory, wifi, spiffs, cpu, sensor

  // System uptime
  unsigned long uptimeMs = millis();
  unsigned long uptimeSec = uptimeMs / 1000;
  unsigned long days = uptimeSec / 86400;
  unsigned long hours = (uptimeSec % 86400) / 3600;
  unsigned long minutes = (uptimeSec % 3600) / 60;
  unsigned long seconds = uptimeSec % 60;

  doc["uptime"]["milliseconds"] = uptimeMs;
  doc["uptime"]["formatted"] = String(days) + "d " + String(hours) + "h " +
                                String(minutes) + "m " + String(seconds) + "s";

  // Memory
  uint32_t heapTotal = ESP.getHeapSize();
  uint32_t heapFree = ESP.getFreeHeap();
  uint32_t heapUsed = heapTotal - heapFree;
  doc["memory"]["heap"]["total"] = heapTotal;
  doc["memory"]["heap"]["free"] = heapFree;
  doc["memory"]["heap"]["used"] = heapUsed;
  doc["memory"]["heap"]["usage_percent"] = ((float)heapUsed / heapTotal) * 100;

  if (psramFound()) {
    uint32_t psramTotal = ESP.getPsramSize();
    uint32_t psramFree = ESP.getFreePsram();
    uint32_t psramUsed = psramTotal - psramFree;
    doc["memory"]["psram"]["total"] = psramTotal;
    doc["memory"]["psram"]["free"] = psramFree;
    doc["memory"]["psram"]["used"] = psramUsed;
    doc["memory"]["psram"]["usage_percent"] = ((float)psramUsed / psramTotal) * 100;
  }

  // Sketch
  uint32_t sketchSize = ESP.getSketchSize();
  uint32_t sketchFree = ESP.getFreeSketchSpace();
  uint32_t sketchTotal = sketchSize + sketchFree;
  doc["memory"]["sketch"]["total"] = sketchTotal;
  doc["memory"]["sketch"]["used"] = sketchSize;
  doc["memory"]["sketch"]["free"] = sketchFree;
  doc["memory"]["sketch"]["usage_percent"] = ((float)sketchSize / sketchTotal) * 100;

  // WiFi
  doc["wifi"]["connected"] = WiFi.status() == WL_CONNECTED;
  doc["wifi"]["ssid"] = WiFi.SSID();
  doc["wifi"]["rssi"] = WiFi.RSSI();
  doc["wifi"]["ip"] = WiFi.localIP().toString();
  doc["wifi"]["mac"] = WiFi.macAddress();

  // SPIFFS
  doc["spiffs"]["ready"] = spiffsManager.isReady();
  if (spiffsManager.isReady()) {
    size_t totalBytes = LittleFS.totalBytes();
    size_t usedBytes = LittleFS.usedBytes();
    doc["spiffs"]["total_bytes"] = totalBytes;
    doc["spiffs"]["used_bytes"] = usedBytes;
    doc["spiffs"]["free_bytes"] = totalBytes - usedBytes;
    doc["spiffs"]["usage_percent"] = totalBytes > 0 ? ((float)usedBytes / totalBytes) * 100 : 0;
  }

  // CPU
  doc["cpu"]["frequency_mhz"] = ESP.getCpuFreqMHz();
  doc["cpu"]["chip_model"] = ESP.getChipModel();

  // Sensor data (if available)
  if (sensorManager.isAvailable()) {
    const SensorData& sensorData = sensorManager.getData();
    if (sensorData.valid) {
      doc["sensor"]["type"] = sensorData.sensorName;
      doc["sensor"]["temperature"] = sensorData.temperature;
      doc["sensor"]["humidity"] = sensorData.humidity;
      doc["sensor"]["timestamp"] = sensorData.timestamp;
    }
  }

  // Serialize to string
  String payload;
  serializeJson(doc, payload);

  // Log payload size for debugging
  Serial.printf("MQTT: Status payload size: %d bytes\n", payload.length());

  // Publish to: mainTopic/hostname/status
  bool success = mqttManager.publishToSubtopic("status", payload.c_str(), false);

  if (success) {
    Serial.println("MQTT: System status published");
  } else {
    Serial.println("MQTT: Failed to publish system status");
  }
}
