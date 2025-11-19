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
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <Update.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>
#include <esp_task_wdt.h>
#include "web_server.h"
#include "spiffs_manager.h"
#include "mqtt_manager.h"

// Global objects
AsyncWebServer server(80);
SPIFFSManager spiffsManager;
MQTTManager mqttManager;

// Mutex for SPIFFS access (prevents concurrent access issues)
SemaphoreHandle_t spiffsMutex = NULL;

// OTA update flags
bool otaUploadInProgress = false;
bool firstRequestAfterBoot = true;

// Configuration
struct Config {
  char ssid[32];
  char password[64];
  bool apMode;
} config;

// Function declarations
void setupWiFi();
void setupWebServer();
bool loadConfig();
void setDefaultConfig();
String getBuiltinHTML();
void serveStaticFile(AsyncWebServerRequest *request, const char* filepath, const char* contentType);
bool isValidESP32Firmware(uint8_t *data, size_t len);
void validateOTABoot();

void setup() {
  Serial.begin(115200);
  Serial.println("\n\n=== ESP32 File Manager (SPIFFS) ===");

  // Create mutex for SPIFFS access
  spiffsMutex = xSemaphoreCreateMutex();
  if (spiffsMutex == NULL) {
    Serial.println("Failed to create SPIFFS mutex!");
  }

  // Initialize SPIFFS
  Serial.println("Initializing LittleFS...");
  if (!spiffsManager.begin()) {
    Serial.println("SPIFFS initialization failed!");
    Serial.println("ERROR: Cannot continue without SPIFFS");
    while(1) {
      delay(1000);
      Serial.println("System halted - SPIFFS required");
    }
  } else {
    Serial.println("LittleFS initialized successfully");
  }

  // Load configuration from SPIFFS
  if (!loadConfig()) {
    Serial.println("Failed to load config, using defaults");
    setDefaultConfig();
  }

  // Setup WiFi
  setupWiFi();

  // Setup MQTT (after WiFi is connected)
  if (mqttManager.begin()) {
    mqttManager.connect();
  }

  // Setup web server
  setupWebServer();

  Serial.println("\n=== System Ready ===");
  Serial.print("Web interface: http://");
  Serial.print(WiFi.localIP());
  Serial.println("/");
  Serial.println("====================\n");
}

void loop() {
  // MQTT loop - handle reconnection and message processing
  mqttManager.loop();

  // Small delay to prevent watchdog issues
  delay(10);
}

void setupWiFi() {
  Serial.println("Setting up WiFi...");

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
      WiFi.softAP("ESP32-FileManager", "12345678");
      Serial.print("AP IP: ");
      Serial.println(WiFi.softAPIP());
    }
  }
}

/**
 * Validate ESP32 firmware binary format
 * ESP32 binaries start with magic byte 0xE9
 */
bool isValidESP32Firmware(uint8_t *data, size_t len) {
  if (len < 1) {
    Serial.println("Firmware validation failed: data too short");
    return false;
  }

  const uint8_t ESP32_MAGIC_BYTE = 0xE9;

  if (data[0] != ESP32_MAGIC_BYTE) {
    Serial.printf("Invalid firmware: magic byte is 0x%02X, expected 0xE9\n", data[0]);
    return false;
  }

  Serial.println("Firmware validation passed: ESP32 magic byte detected");
  return true;
}

/**
 * Validate OTA boot after firmware update
 */
void validateOTABoot() {
  if (!firstRequestAfterBoot) {
    return;
  }

  firstRequestAfterBoot = false;

  const esp_partition_t *running = esp_ota_get_running_partition();
  esp_ota_img_states_t ota_state;

  if (esp_ota_get_state_partition(running, &ota_state) != ESP_OK) {
    Serial.println("Failed to get OTA partition state");
    return;
  }

  if (ota_state == ESP_OTA_IMG_PENDING_VERIFY) {
    Serial.println("First boot after OTA update detected");
    Serial.println("Web server responding successfully - marking partition valid");

    if (esp_ota_mark_app_valid_cancel_rollback() == ESP_OK) {
      Serial.println("OTA update validated successfully - rollback cancelled");
    } else {
      Serial.println("Failed to mark OTA partition valid");
    }
  } else if (ota_state == ESP_OTA_IMG_VALID) {
    Serial.println("Running from valid OTA partition");
  } else if (ota_state == ESP_OTA_IMG_INVALID) {
    Serial.println("Running from invalid partition (should not happen)");
  }
}

void setupWebServer() {
  Serial.println("Setting up web server...");

  // Serve static files from SPIFFS
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    validateOTABoot();
    if (spiffsManager.isReady()) {
      request->send(LittleFS, "/web/index.html", "text/html");
    } else {
      request->send(200, "text/html", getBuiltinHTML());
    }
  });

  server.on("/unified.css", HTTP_GET, [](AsyncWebServerRequest *request) {
    serveStaticFile(request, "/web/unified.css", "text/css");
  });

  server.on("/app.js", HTTP_GET, [](AsyncWebServerRequest *request) {
    serveStaticFile(request, "/web/app.js", "application/javascript");
  });

  server.on("/header.html", HTTP_GET, [](AsyncWebServerRequest *request) {
    serveStaticFile(request, "/web/header.html", "text/html");
  });

  server.on("/header.js", HTTP_GET, [](AsyncWebServerRequest *request) {
    serveStaticFile(request, "/web/header.js", "application/javascript");
  });

  // Health check endpoint
  server.on("/api/health/status", HTTP_GET, [](AsyncWebServerRequest *request) {
    JsonDocument doc;

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

    // Memory information
    doc["memory"]["heap"]["total"] = ESP.getHeapSize();
    doc["memory"]["heap"]["free"] = ESP.getFreeHeap();
    doc["memory"]["heap"]["used"] = ESP.getHeapSize() - ESP.getFreeHeap();
    doc["memory"]["heap"]["usage_percent"] = ((float)(ESP.getHeapSize() - ESP.getFreeHeap()) / ESP.getHeapSize()) * 100;

    doc["memory"]["psram"]["total"] = ESP.getPsramSize();
    doc["memory"]["psram"]["free"] = ESP.getFreePsram();
    doc["memory"]["psram"]["used"] = ESP.getPsramSize() - ESP.getFreePsram();
    if (ESP.getPsramSize() > 0) {
      doc["memory"]["psram"]["usage_percent"] = ((float)(ESP.getPsramSize() - ESP.getFreePsram()) / ESP.getPsramSize()) * 100;
    }

    // WiFi information
    doc["wifi"]["connected"] = WiFi.status() == WL_CONNECTED;
    doc["wifi"]["ssid"] = WiFi.SSID();
    doc["wifi"]["rssi"] = WiFi.RSSI();
    doc["wifi"]["signal_strength"] = WiFi.RSSI() > -50 ? "Excellent" :
                                      WiFi.RSSI() > -60 ? "Good" :
                                      WiFi.RSSI() > -70 ? "Fair" : "Weak";
    doc["wifi"]["ip"] = WiFi.localIP().toString();
    doc["wifi"]["mac"] = WiFi.macAddress();
    doc["wifi"]["channel"] = WiFi.channel();

    // SPIFFS information
    doc["spiffs"]["ready"] = spiffsManager.isReady();
    if (spiffsManager.isReady()) {
      size_t totalBytes = LittleFS.totalBytes();
      size_t usedBytes = LittleFS.usedBytes();
      size_t freeBytes = totalBytes - usedBytes;

      doc["spiffs"]["total_bytes"] = totalBytes;
      doc["spiffs"]["used_bytes"] = usedBytes;
      doc["spiffs"]["free_bytes"] = freeBytes;
      doc["spiffs"]["usage_percent"] = totalBytes > 0 ? ((float)usedBytes / totalBytes) * 100 : 0;
    }

    // CPU information
    doc["cpu"]["frequency_mhz"] = ESP.getCpuFreqMHz();
    doc["cpu"]["cores"] = 2;
    doc["cpu"]["chip_model"] = ESP.getChipModel();
    doc["cpu"]["chip_revision"] = ESP.getChipRevision();
    doc["cpu"]["sdk_version"] = ESP.getSdkVersion();

    // Flash information
    doc["flash"]["size_mb"] = ESP.getFlashChipSize() / (1024 * 1024);
    doc["flash"]["speed_mhz"] = ESP.getFlashChipSpeed() / 1000000;

    // OTA status
    doc["ota"]["upload_in_progress"] = otaUploadInProgress;

    // Overall health status
    bool isHealthy = WiFi.status() == WL_CONNECTED &&
                     ESP.getFreeHeap() > 50000 &&
                     (spiffsManager.isReady() && LittleFS.totalBytes() > LittleFS.usedBytes());

    doc["status"] = isHealthy ? "healthy" : "degraded";
    doc["timestamp"] = uptimeMs;

    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
  });

  // Serve JS file for File Manager
  server.on("/filemanager.js", HTTP_GET, [](AsyncWebServerRequest *request) {
    serveStaticFile(request, "/web/filemanager.js", "application/javascript");
  });

  // Serve JS file for Health Monitor
  server.on("/health.js", HTTP_GET, [](AsyncWebServerRequest *request) {
    serveStaticFile(request, "/web/health.js", "application/javascript");
  });

  // Health Monitor page
  server.on("/health", HTTP_GET, [](AsyncWebServerRequest *request) {
    validateOTABoot();
    if (spiffsManager.isReady()) {
      request->send(LittleFS, "/web/health.html", "text/html");
    } else {
      request->send(503, "text/html",
        "<html><body><h1>Health Monitor unavailable</h1>"
        "<p>SPIFFS is required for Health Monitor functionality.</p>"
        "<a href='/'>Back to Home</a></body></html>");
    }
  });

  // File Manager page
  server.on("/filemanager", HTTP_GET, [](AsyncWebServerRequest *request) {
    validateOTABoot();
    if (spiffsManager.isReady()) {
      request->send(LittleFS, "/web/filemanager.html", "text/html");
    } else {
      request->send(503, "text/html",
        "<html><body><h1>File Manager unavailable</h1>"
        "<p>SPIFFS is required for File Manager functionality.</p>"
        "<a href='/'>Back to Home</a></body></html>");
    }
  });

  // Firmware update page
  server.on("/firmware", HTTP_GET, [](AsyncWebServerRequest *request) {
    validateOTABoot();
    if (spiffsManager.isReady()) {
      request->send(LittleFS, "/web/firmware.html", "text/html");
    } else {
      request->send(503, "text/html",
        "<html><body><h1>Firmware Update unavailable</h1>"
        "<p>SPIFFS is required for Firmware Update functionality.</p>"
        "<a href='/'>Back to Home</a></body></html>");
    }
  });

  // Serve JS file for Firmware Update
  server.on("/firmware.js", HTTP_GET, [](AsyncWebServerRequest *request) {
    serveStaticFile(request, "/web/firmware.js", "application/javascript");
  });

  // OTA Firmware Upload endpoint
  static String otaUploadError = "";

  server.on("/api/firmware/upload", HTTP_POST,
    [](AsyncWebServerRequest *request) {
      if (otaUploadInProgress) {
        xSemaphoreGive(spiffsMutex);
        otaUploadInProgress = false;
        Serial.println("OTA upload finished - SPIFFS mutex released");
      }

      if (otaUploadError.length() > 0) {
        Serial.printf("OTA Upload error: %s\n", otaUploadError.c_str());
        request->send(500, "application/json",
          "{\"error\":\"" + otaUploadError + "\"}");
        otaUploadError = "";
        return;
      }

      if (Update.hasError()) {
        String error = "Update failed. Error: ";
        error += Update.errorString();
        Serial.println(error);
        request->send(500, "application/json",
          "{\"error\":\"" + error + "\"}");
        return;
      }

      Serial.println("OTA Update successful! Rebooting...");
      request->send(200, "application/json",
        "{\"status\":\"ok\",\"message\":\"Firmware updated successfully. Device will reboot now.\"}");

      delay(2000);
      Serial.println("Restarting ESP32 now...");
      ESP.restart();
    },

    [](AsyncWebServerRequest *request, String filename, size_t index,
       uint8_t *data, size_t len, bool final) {

      if (index == 0) {
        Serial.printf("\n=== OTA Update started: %s ===\n", filename.c_str());
        Serial.printf("File size: %d bytes\n", request->contentLength());
        otaUploadError = "";

        esp_task_wdt_delete(xTaskGetIdleTaskHandleForCPU(0));
        esp_task_wdt_delete(xTaskGetIdleTaskHandleForCPU(1));

        Serial.printf("Free heap before OTA: %d bytes\n", ESP.getFreeHeap());

        if (xSemaphoreTake(spiffsMutex, pdMS_TO_TICKS(10000)) != pdTRUE) {
          Serial.println("ERROR: SPIFFS busy - mutex timeout");
          otaUploadError = "SPIFFS is busy";
          return;
        }
        otaUploadInProgress = true;

        if (!isValidESP32Firmware(data, len)) {
          Serial.println("ERROR: Invalid firmware file");
          otaUploadError = "Invalid ESP32 firmware file";
          xSemaphoreGive(spiffsMutex);
          otaUploadInProgress = false;
          return;
        }

        if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH)) {
          Serial.printf("ERROR: Update.begin() failed: %s\n", Update.errorString());
          otaUploadError = "Failed to begin OTA update: ";
          otaUploadError += Update.errorString();
          xSemaphoreGive(spiffsMutex);
          otaUploadInProgress = false;
          return;
        }

        Serial.println("=== OTA Update initialized ===\n");
      }

      if (len) {
        yield();
        size_t written = Update.write(data, len);
        if (written != len) {
          Serial.printf("ERROR: OTA Write failed - wrote %d of %d bytes\n", written, len);
          otaUploadError = "Failed to write firmware data";
          Update.abort();
          return;
        }
        yield();

        if (index % 32768 == 0 && index > 0) {
          Serial.printf("Progress: %d KB written (%.1f%%)\n",
                       (index + len) / 1024,
                       ((float)(index + len) / request->contentLength()) * 100);
        }
      }

      if (final) {
        Serial.println("\n=== Finalizing OTA update ===");
        Serial.printf("Total received: %d bytes\n", index + len);

        if (Update.end(true)) {
          Serial.println("SUCCESS: OTA Update completed!");
        } else {
          Serial.printf("ERROR: Update.end() failed: %s\n", Update.errorString());
          otaUploadError = "Failed to finalize OTA update: ";
          otaUploadError += Update.errorString();
        }
      }
    }
  );

  // List files in SPIFFS
  server.on("/api/files/list", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (otaUploadInProgress) {
      request->send(503, "application/json", "{\"error\":\"System busy - firmware update in progress\"}");
      return;
    }

    if (!spiffsManager.isReady()) {
      request->send(503, "application/json", "{\"error\":\"SPIFFS not ready\"}");
      return;
    }

    String path = "/";
    if (request->hasParam("dir")) {
      path = request->getParam("dir")->value();
    }

    File root = LittleFS.open(path);
    if (!root || !root.isDirectory()) {
      request->send(404, "application/json", "{\"error\":\"Directory not found\"}");
      return;
    }

    JsonDocument doc;
    JsonArray files = doc["files"].to<JsonArray>();

    File file = root.openNextFile();
    while (file) {
      JsonObject fileObj = files.add<JsonObject>();
      fileObj["name"] = String(file.name());
      fileObj["size"] = file.size();
      fileObj["isDir"] = file.isDirectory();
      file = root.openNextFile();
    }

    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
  });

  // Download file
  server.on("/api/files/download", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (otaUploadInProgress) {
      request->send(503, "text/plain", "System busy");
      return;
    }

    if (!spiffsManager.isReady()) {
      request->send(503, "text/plain", "SPIFFS not ready");
      return;
    }

    if (!request->hasParam("file")) {
      request->send(400, "text/plain", "Missing file parameter");
      return;
    }

    String filepath = request->getParam("file")->value();
    if (!LittleFS.exists(filepath)) {
      request->send(404, "text/plain", "File not found");
      return;
    }

    request->send(LittleFS, filepath, String(), true);
  });

  // View file
  server.on("/api/files/view", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (otaUploadInProgress) {
      request->send(503, "text/plain", "System busy");
      return;
    }

    if (!request->hasParam("file")) {
      request->send(400, "text/plain", "Missing file parameter");
      return;
    }

    String filepath = request->getParam("file")->value();
    if (!LittleFS.exists(filepath)) {
      request->send(404, "text/plain", "File not found");
      return;
    }

    request->send(LittleFS, filepath, "text/plain", false);
  });

  // Read file for editing
  server.on("/api/files/read", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (otaUploadInProgress) {
      request->send(503, "application/json", "{\"error\":\"System busy\"}");
      return;
    }

    if (!request->hasParam("file")) {
      request->send(400, "application/json", "{\"error\":\"Missing file parameter\"}");
      return;
    }

    String filepath = request->getParam("file")->value();
    if (!LittleFS.exists(filepath)) {
      request->send(404, "application/json", "{\"error\":\"File not found\"}");
      return;
    }

    if (xSemaphoreTake(spiffsMutex, pdMS_TO_TICKS(5000)) == pdTRUE) {
      File file = LittleFS.open(filepath, FILE_READ);
      if (!file) {
        xSemaphoreGive(spiffsMutex);
        request->send(500, "application/json", "{\"error\":\"Failed to open file\"}");
        return;
      }

      size_t fileSize = file.size();

      if (fileSize > 51200) {
        file.close();
        xSemaphoreGive(spiffsMutex);
        request->send(413, "application/json", "{\"error\":\"File too large (max 50KB)\"}");
        return;
      }

      String content = "";
      content.reserve(fileSize + 1);

      while (file.available()) {
        content += (char)file.read();
      }

      file.close();
      xSemaphoreGive(spiffsMutex);

      JsonDocument doc;
      doc["status"] = "ok";
      doc["content"] = content;
      doc["size"] = fileSize;

      String response;
      serializeJson(doc, response);
      request->send(200, "application/json", response);
    } else {
      request->send(503, "application/json", "{\"error\":\"SPIFFS busy\"}");
    }
  });

  // Write file
  server.on("/api/files/write", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (otaUploadInProgress) {
      request->send(503, "application/json", "{\"error\":\"System busy\"}");
      return;
    }

    if (!request->hasParam("file", true) || !request->hasParam("content", true)) {
      request->send(400, "application/json", "{\"error\":\"Missing parameters\"}");
      return;
    }

    String filepath = request->getParam("file", true)->value();
    String content = request->getParam("content", true)->value();

    if (xSemaphoreTake(spiffsMutex, pdMS_TO_TICKS(5000)) == pdTRUE) {
      File file = LittleFS.open(filepath, FILE_WRITE);
      if (!file) {
        xSemaphoreGive(spiffsMutex);
        request->send(500, "application/json", "{\"error\":\"Failed to open file\"}");
        return;
      }

      size_t written = file.print(content);
      file.close();
      xSemaphoreGive(spiffsMutex);

      if (written > 0) {
        JsonDocument doc;
        doc["status"] = "ok";
        doc["written"] = written;

        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
      } else {
        request->send(500, "application/json", "{\"error\":\"Failed to write\"}");
      }
    } else {
      request->send(503, "application/json", "{\"error\":\"SPIFFS busy\"}");
    }
  });

  // Delete file
  server.on("/api/files/delete", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (otaUploadInProgress) {
      request->send(503, "application/json", "{\"error\":\"System busy\"}");
      return;
    }

    if (!request->hasParam("file", true)) {
      request->send(400, "application/json", "{\"error\":\"Missing file parameter\"}");
      return;
    }

    String filepath = request->getParam("file", true)->value();

    File file = LittleFS.open(filepath);
    if (!file) {
      request->send(404, "application/json", "{\"error\":\"File not found\"}");
      return;
    }

    bool isDir = file.isDirectory();
    file.close();

    bool success = false;
    if (isDir) {
      success = LittleFS.rmdir(filepath);
    } else {
      success = LittleFS.remove(filepath);
    }

    if (success) {
      request->send(200, "application/json", "{\"status\":\"ok\"}");
    } else {
      request->send(500, "application/json", "{\"error\":\"Failed to delete\"}");
    }
  });

  // Upload file
  server.on("/api/files/upload", HTTP_POST,
    [](AsyncWebServerRequest *request) {
      request->send(200, "application/json", "{\"status\":\"ok\"}");
    },
    [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
      static File uploadFile;

      if (otaUploadInProgress) {
        return;
      }

      if (index == 0) {
        String path = "/";
        if (request->hasParam("dir", false)) {
          path = request->getParam("dir", false)->value();
          if (path != "/" && !path.endsWith("/")) {
            path += "/";
          }
        }

        String filepath = path + filename;
        Serial.printf("Upload start: %s\n", filepath.c_str());

        if (LittleFS.exists(filepath)) {
          LittleFS.remove(filepath);
        }

        uploadFile = LittleFS.open(filepath, FILE_WRITE);
        if (!uploadFile) {
          Serial.printf("Failed to open for writing: %s\n", filepath.c_str());
          return;
        }
      }

      if (uploadFile && len) {
        uploadFile.write(data, len);
        if (index % 8192 == 0) {
          delay(1);
        }
      }

      if (final) {
        if (uploadFile) {
          uploadFile.close();
          Serial.printf("Upload complete: %s (%d bytes)\n", filename.c_str(), index + len);
        }
      }
    }
  );

  // Create directory
  server.on("/api/files/mkdir", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (otaUploadInProgress) {
      request->send(503, "application/json", "{\"error\":\"System busy\"}");
      return;
    }

    if (!request->hasParam("dir", true)) {
      request->send(400, "application/json", "{\"error\":\"Missing dir parameter\"}");
      return;
    }

    String dirpath = request->getParam("dir", true)->value();

    if (LittleFS.mkdir(dirpath)) {
      request->send(200, "application/json", "{\"status\":\"ok\"}");
    } else {
      request->send(500, "application/json", "{\"error\":\"Failed to create directory\"}");
    }
  });

  // WiFi Manager page
  server.on("/wifi", HTTP_GET, [](AsyncWebServerRequest *request) {
    validateOTABoot();
    if (spiffsManager.isReady()) {
      request->send(LittleFS, "/web/wifi.html", "text/html");
    } else {
      request->send(503, "text/html",
        "<html><body><h1>WiFi Manager unavailable</h1>"
        "<p>SPIFFS is required for WiFi Manager functionality.</p>"
        "<a href='/'>Back to Home</a></body></html>");
    }
  });

  server.on("/wifi.js", HTTP_GET, [](AsyncWebServerRequest *request) {
    serveStaticFile(request, "/web/wifi.js", "application/javascript");
  });

  // WiFi Scan API
  server.on("/api/wifi/scan", HTTP_GET, [](AsyncWebServerRequest *request) {
    JsonDocument doc;

    int n = WiFi.scanNetworks();

    if (n == 0) {
      doc["networks"] = JsonArray();
    } else {
      JsonArray networks = doc["networks"].to<JsonArray>();

      for (int i = 0; i < n; i++) {
        JsonObject network = networks.add<JsonObject>();
        network["ssid"] = WiFi.SSID(i);
        network["rssi"] = WiFi.RSSI(i);
        network["encryption"] = (int)WiFi.encryptionType(i);
        network["channel"] = WiFi.channel(i);
      }
    }

    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);

    // Clean up
    WiFi.scanDelete();
  });

  // WiFi Connect API
  server.on("/api/wifi/connect", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL,
    [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
      if (index == 0) {
        // First chunk - parse JSON
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, data, len);

        if (error) {
          request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
          return;
        }

        const char* ssid = doc["ssid"];
        const char* password = doc["password"];

        if (!ssid || strlen(ssid) == 0) {
          request->send(400, "application/json", "{\"error\":\"SSID is required\"}");
          return;
        }

        // Read existing config.json
        JsonDocument configDoc;

        if (xSemaphoreTake(spiffsMutex, pdMS_TO_TICKS(5000)) == pdTRUE) {
          // Read existing config
          File configFile = LittleFS.open("/config.json", "r");
          if (configFile) {
            DeserializationError error = deserializeJson(configDoc, configFile);
            configFile.close();

            if (error) {
              Serial.println("Failed to parse existing config.json, using defaults");
            }
          }

          // Update only WiFi settings
          configDoc["wifi"]["ssid"] = ssid;
          configDoc["wifi"]["password"] = password ? password : "";
          configDoc["wifi"]["ap_mode"] = false;

          // Save updated config
          configFile = LittleFS.open("/config.json", "w");
          if (configFile) {
            serializeJson(configDoc, configFile);
            configFile.close();

            Serial.printf("WiFi config saved: SSID=%s\n", ssid);

            request->send(200, "application/json", "{\"status\":\"ok\",\"message\":\"Configuration saved. Rebooting...\"}");

            // Release mutex before reboot
            xSemaphoreGive(spiffsMutex);

            // Reboot after 2 seconds
            delay(2000);
            ESP.restart();
          } else {
            xSemaphoreGive(spiffsMutex);
            request->send(500, "application/json", "{\"error\":\"Failed to save configuration\"}");
          }
        } else {
          request->send(503, "application/json", "{\"error\":\"System busy\"}");
        }
      }
    }
  );

  // MQTT Manager page
  server.on("/mqtt", HTTP_GET, [](AsyncWebServerRequest *request) {
    validateOTABoot();
    if (spiffsManager.isReady()) {
      request->send(LittleFS, "/web/mqtt.html", "text/html");
    } else {
      request->send(503, "text/html",
        "<html><body><h1>MQTT Manager unavailable</h1>"
        "<p>SPIFFS is required for MQTT Manager functionality.</p>"
        "<a href='/'>Back to Home</a></body></html>");
    }
  });

  server.on("/mqtt.js", HTTP_GET, [](AsyncWebServerRequest *request) {
    serveStaticFile(request, "/web/mqtt.js", "application/javascript");
  });

  // MQTT Status API
  server.on("/api/mqtt/status", HTTP_GET, [](AsyncWebServerRequest *request) {
    JsonDocument doc;

    const MQTTManager::MQTTConfig& config = mqttManager.getConfig();

    doc["enabled"] = config.enabled;
    doc["connected"] = mqttManager.isConnected();
    doc["server"] = config.server;
    doc["port"] = config.port;
    doc["main_topic"] = config.mainTopic;
    doc["client_id"] = config.clientId;

    if (!mqttManager.isConnected() && config.enabled) {
      doc["error"] = mqttManager.getLastError();
      doc["state"] = mqttManager.getState();
    }

    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
  });

  // MQTT Configuration API - GET
  server.on("/api/mqtt/config", HTTP_GET, [](AsyncWebServerRequest *request) {
    JsonDocument doc;

    const MQTTManager::MQTTConfig& config = mqttManager.getConfig();

    doc["enabled"] = config.enabled;
    doc["server"] = config.server;
    doc["port"] = config.port;
    doc["username"] = config.username;
    doc["password"] = config.password;
    doc["main_topic"] = config.mainTopic;
    doc["client_id"] = config.clientId;

    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
  });

  // MQTT Configuration API - POST
  server.on("/api/mqtt/config", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL,
    [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
      if (index == 0) {
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, data, len);

        if (error) {
          request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
          return;
        }

        // Update MQTT configuration
        const char* server = doc["server"] | "";
        uint16_t port = doc["port"] | 1883;
        const char* username = doc["username"] | "";
        const char* password = doc["password"] | "";
        const char* mainTopic = doc["main_topic"] | "esp32/data";
        bool enabled = doc["enabled"] | false;

        mqttManager.updateConfig(server, port, username, password, mainTopic, enabled);

        // Save to config.json
        if (xSemaphoreTake(spiffsMutex, pdMS_TO_TICKS(5000)) == pdTRUE) {
          File configFile = LittleFS.open("/config.json", "r");
          JsonDocument configDoc;

          if (configFile) {
            deserializeJson(configDoc, configFile);
            configFile.close();
          }

          // Save MQTT config to JSON
          mqttManager.saveConfig(configDoc);

          // Write back to file
          configFile = LittleFS.open("/config.json", "w");
          if (configFile) {
            serializeJson(configDoc, configFile);
            configFile.close();

            Serial.println("MQTT configuration saved to file");

            request->send(200, "application/json", "{\"status\":\"ok\",\"message\":\"Configuration saved\"}");
          } else {
            request->send(500, "application/json", "{\"error\":\"Failed to save configuration\"}");
          }

          xSemaphoreGive(spiffsMutex);
        } else {
          request->send(503, "application/json", "{\"error\":\"System busy\"}");
        }
      }
    }
  );

  // MQTT Test Connection API
  server.on("/api/mqtt/test", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL,
    [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
      if (index == 0) {
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, data, len);

        if (error) {
          request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
          return;
        }

        // Test connection with provided settings
        const char* server = doc["server"];
        uint16_t port = doc["port"] | 1883;
        const char* username = doc["username"] | "";
        const char* password = doc["password"] | "";

        if (!server || strlen(server) == 0) {
          request->send(400, "application/json", "{\"error\":\"Server is required\"}");
          return;
        }

        // Create temporary MQTT client for testing
        WiFiClient testWifiClient;
        PubSubClient testMqttClient(testWifiClient);

        testMqttClient.setServer(server, port);

        bool connected = false;
        if (strlen(username) > 0) {
          connected = testMqttClient.connect("ESP32_TEST", username, password);
        } else {
          connected = testMqttClient.connect("ESP32_TEST");
        }

        if (connected) {
          testMqttClient.disconnect();
          request->send(200, "application/json", "{\"success\":true,\"message\":\"Connection successful\"}");
        } else {
          String errorMsg = "Connection failed (state=" + String(testMqttClient.state()) + ")";
          request->send(200, "application/json", "{\"success\":false,\"error\":\"" + errorMsg + "\"}");
        }
      }
    }
  );

  // MQTT Publish API
  server.on("/api/mqtt/publish", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL,
    [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
      if (index == 0) {
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, data, len);

        if (error) {
          request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
          return;
        }

        const char* topic = doc["topic"];
        const char* message = doc["message"];
        bool retained = doc["retained"] | false;

        if (!message) {
          request->send(400, "application/json", "{\"error\":\"Message is required\"}");
          return;
        }

        if (!mqttManager.isConnected()) {
          request->send(503, "application/json", "{\"error\":\"MQTT not connected\"}");
          return;
        }

        bool success;
        String usedTopic;

        if (topic && strlen(topic) > 0) {
          usedTopic = String(topic);
          success = mqttManager.publish(topic, message, retained);
        } else {
          usedTopic = String(mqttManager.getConfig().mainTopic);
          success = mqttManager.publishToMainTopic(message, retained);
        }

        if (success) {
          JsonDocument responseDoc;
          responseDoc["status"] = "ok";
          responseDoc["topic"] = usedTopic;

          String response;
          serializeJson(responseDoc, response);
          request->send(200, "application/json", response);
        } else {
          request->send(500, "application/json", "{\"error\":\"Failed to publish message\"}");
        }
      }
    }
  );

  // 404 handler
  server.onNotFound([](AsyncWebServerRequest *request) {
    validateOTABoot();
    request->send(404, "text/plain", "Not found");
  });

  server.begin();
  Serial.println("Web server started");
}

bool loadConfig() {
  if (!spiffsManager.isReady()) return false;

  File file = LittleFS.open("/config.json", FILE_READ);
  if (!file) {
    Serial.println("Config file not found");
    return false;
  }

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, file);
  file.close();

  if (error) {
    Serial.println("Failed to parse config");
    return false;
  }

  strlcpy(config.ssid, doc["wifi"]["ssid"] | "ESP32-FileManager", sizeof(config.ssid));
  strlcpy(config.password, doc["wifi"]["password"] | "12345678", sizeof(config.password));
  config.apMode = doc["wifi"]["ap_mode"] | true;

  // Load MQTT configuration
  mqttManager.loadConfig(doc);

  Serial.println("Configuration loaded from SPIFFS");
  return true;
}

void setDefaultConfig() {
  strcpy(config.ssid, "ESP32-FileManager");
  strcpy(config.password, "12345678");
  config.apMode = true;
}

void serveStaticFile(AsyncWebServerRequest *request, const char* filepath, const char* contentType) {
  if (!spiffsManager.isReady()) {
    request->send(503, "text/plain", "SPIFFS not available");
    return;
  }

  if (!LittleFS.exists(filepath)) {
    request->send(404, "text/plain", "File not found");
    return;
  }

  AsyncWebServerResponse *response = request->beginResponse(LittleFS, filepath, contentType);
  if (response) {
    response->addHeader("Cache-Control", "public, max-age=3600");
    request->send(response);
  } else {
    request->send(500, "text/plain", "Failed to serve file");
  }
}

String getBuiltinHTML() {
  return R"HTML(
<!DOCTYPE html>
<html>
<head>
  <title>ESP32 File Manager</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body { font-family: Arial; margin: 20px; background: #f0f0f0; }
    .container { max-width: 800px; margin: 0 auto; background: white; padding: 20px; border-radius: 10px; }
    h1 { color: #333; }
    .warning { color: #d9534f; padding: 10px; background: #f2dede; border-radius: 5px; margin: 10px 0; }
  </style>
</head>
<body>
  <div class="container">
    <h1>ESP32 File Manager</h1>
    <div class="warning">
      <strong>Error:</strong> SPIFFS not available
    </div>
    <p>System cannot function without SPIFFS. Please reflash the firmware.</p>
  </div>
</body>
</html>
  )HTML";
}
