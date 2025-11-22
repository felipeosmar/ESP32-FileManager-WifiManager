#include "web_server.h"
#include <Update.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>
#include <esp_task_wdt.h>

WebServerManager::WebServerManager() : server(80), ws("/ws"), otaUploadInProgress(false) {
}

void WebServerManager::begin(SPIFFSManager* spiffs, MQTTManager* mqtt, OLEDManager* oled, SensorManager* sensor, SemaphoreHandle_t* mutex) {
    this->spiffsManager = spiffs;
    this->mqttManager = mqtt;
    this->oledManager = oled;
    this->sensorManager = sensor;
    this->spiffsMutex = mutex;

    ws.onEvent([this](AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
        onWsEvent(server, client, type, arg, data, len);
    });
    server.addHandler(&ws);

    setupRoutes();
    server.begin();
    Serial.println("Web server started");
}

bool WebServerManager::checkAuth(AsyncWebServerRequest *request) {
    if (!request->authenticate(WEB_USERNAME, WEB_PASSWORD)) {
        request->requestAuthentication();
        return false;
    }
    return true;
}

void WebServerManager::validateOTABoot() {
    // Logic copied from main.cpp
    static bool firstRequestAfterBoot = true;
    
    #ifdef OTA_NO_ROLLBACK
      if (!firstRequestAfterBoot) return;
      firstRequestAfterBoot = false;
      Serial.println("OTA rollback protection: DISABLED (2MB flash mode)");
      return;
    #else
      if (!firstRequestAfterBoot) return;
      firstRequestAfterBoot = false;
    
      const esp_partition_t *running = esp_ota_get_running_partition();
      esp_ota_img_states_t ota_state;
    
      if (esp_ota_get_state_partition(running, &ota_state) != ESP_OK) {
        Serial.println("Failed to get OTA partition state");
        return;
      }
    
      if (ota_state == ESP_OTA_IMG_PENDING_VERIFY) {
        Serial.println("First boot after OTA update detected");
        if (esp_ota_mark_app_valid_cancel_rollback() == ESP_OK) {
          Serial.println("OTA update validated successfully - rollback cancelled");
        } else {
          Serial.println("Failed to mark OTA partition valid");
        }
      }
    #endif
}

void WebServerManager::serveStaticFile(AsyncWebServerRequest *request, const char* filepath, const char* contentType) {
    if (!checkAuth(request)) return;

    if (!spiffsManager->isReady()) {
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
        // GZIP compression if supported by client and file (AsyncWebServer handles this automatically if file has .gz extension, 
        // but here we are serving specific files. We can enable it globally or per response)
        // For now, we rely on standard serving.
        request->send(response);
    } else {
        request->send(500, "text/plain", "Failed to serve file");
    }
}

void WebServerManager::setupRoutes() {
    // Root
    server.on("/", HTTP_GET, [this](AsyncWebServerRequest *request) {
        handleRoot(request);
    });

    // Static files
    server.on("/unified.css", HTTP_GET, [this](AsyncWebServerRequest *request) {
        serveStaticFile(request, "/web/unified.css", "text/css");
    });
    server.on("/app.js", HTTP_GET, [this](AsyncWebServerRequest *request) {
        serveStaticFile(request, "/web/app.js", "application/javascript");
    });
    server.on("/header.html", HTTP_GET, [this](AsyncWebServerRequest *request) {
        serveStaticFile(request, "/web/header.html", "text/html");
    });
    server.on("/header.js", HTTP_GET, [this](AsyncWebServerRequest *request) {
        serveStaticFile(request, "/web/header.js", "application/javascript");
    });

    // Pages
    server.on("/status", HTTP_GET, [this](AsyncWebServerRequest *request) {
        if (!checkAuth(request)) return;
        validateOTABoot();
        if (spiffsManager->isReady()) request->send(LittleFS, "/web/status.html", "text/html");
        else request->send(503, "text/plain", "SPIFFS not ready");
    });
    server.on("/status.js", HTTP_GET, [this](AsyncWebServerRequest *request) {
        serveStaticFile(request, "/web/status.js", "application/javascript");
    });

    server.on("/filemanager", HTTP_GET, [this](AsyncWebServerRequest *request) {
        if (!checkAuth(request)) return;
        validateOTABoot();
        if (spiffsManager->isReady()) request->send(LittleFS, "/web/filemanager.html", "text/html");
        else request->send(503, "text/plain", "SPIFFS not ready");
    });
    server.on("/filemanager.js", HTTP_GET, [this](AsyncWebServerRequest *request) {
        serveStaticFile(request, "/web/filemanager.js", "application/javascript");
    });

    server.on("/firmware", HTTP_GET, [this](AsyncWebServerRequest *request) {
        if (!checkAuth(request)) return;
        validateOTABoot();
        if (spiffsManager->isReady()) request->send(LittleFS, "/web/firmware.html", "text/html");
        else request->send(503, "text/plain", "SPIFFS not ready");
    });
    server.on("/firmware.js", HTTP_GET, [this](AsyncWebServerRequest *request) {
        serveStaticFile(request, "/web/firmware.js", "application/javascript");
    });

    server.on("/wifi", HTTP_GET, [this](AsyncWebServerRequest *request) {
        if (!checkAuth(request)) return;
        validateOTABoot();
        if (spiffsManager->isReady()) request->send(LittleFS, "/web/wifi.html", "text/html");
        else request->send(503, "text/plain", "SPIFFS not ready");
    });
    server.on("/wifi.js", HTTP_GET, [this](AsyncWebServerRequest *request) {
        serveStaticFile(request, "/web/wifi.js", "application/javascript");
    });

    server.on("/mqtt", HTTP_GET, [this](AsyncWebServerRequest *request) {
        if (!checkAuth(request)) return;
        validateOTABoot();
        if (spiffsManager->isReady()) request->send(LittleFS, "/web/mqtt.html", "text/html");
        else request->send(503, "text/plain", "SPIFFS not ready");
    });
    server.on("/mqtt.js", HTTP_GET, [this](AsyncWebServerRequest *request) {
        serveStaticFile(request, "/web/mqtt.js", "application/javascript");
    });

    server.on("/display", HTTP_GET, [this](AsyncWebServerRequest *request) {
        if (!checkAuth(request)) return;
        validateOTABoot();
        if (spiffsManager->isReady()) request->send(LittleFS, "/web/display.html", "text/html");
        else request->send(503, "text/plain", "SPIFFS not ready");
    });
    server.on("/display.js", HTTP_GET, [this](AsyncWebServerRequest *request) {
        serveStaticFile(request, "/web/display.js", "application/javascript");
    });

    server.on("/sensor", HTTP_GET, [this](AsyncWebServerRequest *request) {
        if (!checkAuth(request)) return;
        validateOTABoot();
        if (spiffsManager->isReady()) request->send(LittleFS, "/web/sensor.html", "text/html");
        else request->send(503, "text/plain", "SPIFFS not ready");
    });
    server.on("/sensor.js", HTTP_GET, [this](AsyncWebServerRequest *request) {
        serveStaticFile(request, "/web/sensor.js", "application/javascript");
    });

    // API Endpoints
    server.on("/api/status", HTTP_GET, [this](AsyncWebServerRequest *request) { handleStatus(request); });
    
    server.on("/api/files/list", HTTP_GET, [this](AsyncWebServerRequest *request) { handleFileList(request); });
    server.on("/api/files/download", HTTP_GET, [this](AsyncWebServerRequest *request) { handleFileDownload(request); });
    server.on("/api/files/view", HTTP_GET, [this](AsyncWebServerRequest *request) { handleFileView(request); });
    server.on("/api/files/read", HTTP_GET, [this](AsyncWebServerRequest *request) { handleFileRead(request); });
    server.on("/api/files/write", HTTP_POST, [this](AsyncWebServerRequest *request) { handleFileWrite(request); });
    server.on("/api/files/delete", HTTP_POST, [this](AsyncWebServerRequest *request) { handleFileDelete(request); });
    server.on("/api/files/mkdir", HTTP_POST, [this](AsyncWebServerRequest *request) { handleFileCreateDir(request); });
    
    server.on("/api/files/upload", HTTP_POST, 
        [this](AsyncWebServerRequest *request) { request->send(200, "application/json", "{\"status\":\"ok\"}"); },
        [this](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
            handleFileUpload(request, filename, index, data, len, final);
        }
    );

    server.on("/api/wifi/scan", HTTP_GET, [this](AsyncWebServerRequest *request) { handleWiFiScan(request); });
    server.on("/api/wifi/connect", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL, 
        [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            handleWiFiConnect(request, data, len, index, total);
        }
    );

    server.on("/api/mqtt/status", HTTP_GET, [this](AsyncWebServerRequest *request) { handleMQTTStatus(request); });
    server.on("/api/mqtt/config", HTTP_GET, [this](AsyncWebServerRequest *request) { handleMQTTConfigGet(request); });
    server.on("/api/mqtt/config", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL,
        [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            handleMQTTConfigPost(request, data, len, index, total);
        }
    );
    server.on("/api/mqtt/test", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL,
        [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            handleMQTTTest(request, data, len, index, total);
        }
    );
    server.on("/api/mqtt/publish", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL,
        [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            handleMQTTPublish(request, data, len, index, total);
        }
    );

    server.on("/api/display/status", HTTP_GET, [this](AsyncWebServerRequest *request) { handleDisplayStatus(request); });
    server.on("/api/display/config", HTTP_GET, [this](AsyncWebServerRequest *request) { handleDisplayConfigGet(request); });
    server.on("/api/display/config", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL,
        [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            handleDisplayConfigPost(request, data, len, index, total);
        }
    );
    server.on("/api/display/mode", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL,
        [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            handleDisplayMode(request, data, len, index, total);
        }
    );

    server.on("/api/sensor/status", HTTP_GET, [this](AsyncWebServerRequest *request) { handleSensorStatus(request); });
    server.on("/api/sensor/config", HTTP_GET, [this](AsyncWebServerRequest *request) { handleSensorConfigGet(request); });
    server.on("/api/sensor/config", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL,
        [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            handleSensorConfigPost(request, data, len, index, total);
        }
    );

    server.on("/api/firmware/upload", HTTP_POST, 
        [this](AsyncWebServerRequest *request) {
             if (otaUploadInProgress) {
                xSemaphoreGive(*spiffsMutex);
                otaUploadInProgress = false;
                Serial.println("OTA upload finished - SPIFFS mutex released");
            }
            if (otaUploadError.length() > 0) {
                request->send(500, "application/json", "{\"error\":\"" + otaUploadError + "\"}");
                otaUploadError = "";
                return;
            }
            if (Update.hasError()) {
                request->send(500, "application/json", "{\"error\":\"Update failed\"}");
                return;
            }
            request->send(200, "application/json", "{\"status\":\"ok\",\"message\":\"Firmware updated successfully. Device will reboot now.\"}");
            delay(2000);
            ESP.restart();
        },
        [this](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
            handleOTA(request, filename, index, data, len, final);
        }
    );

    server.onNotFound([this](AsyncWebServerRequest *request) {
        if (!checkAuth(request)) return;
        validateOTABoot();
        request->send(404, "text/plain", "Not found");
    });
}

void WebServerManager::handleRoot(AsyncWebServerRequest *request) {
    if (!checkAuth(request)) return;
    validateOTABoot();
    if (spiffsManager->isReady()) {
        request->send(LittleFS, "/web/index.html", "text/html");
    } else {
        request->send(200, "text/html", "<html><body><h1>System Initializing...</h1></body></html>");
    }
}

void WebServerManager::handleStatus(AsyncWebServerRequest *request) {
    if (!checkAuth(request)) return;
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
    doc["wifi"]["gateway"] = WiFi.gatewayIP().toString();
    doc["wifi"]["subnet"] = WiFi.subnetMask().toString();
    doc["wifi"]["dns"] = WiFi.dnsIP().toString();
    doc["wifi"]["bssid"] = WiFi.BSSIDstr();
    doc["wifi"]["channel"] = WiFi.channel();

    // Signal strength description
    int rssi = WiFi.RSSI();
    String signalStrength;
    if (rssi >= -50) {
        signalStrength = "Excelente";
    } else if (rssi >= -60) {
        signalStrength = "Bom";
    } else if (rssi >= -70) {
        signalStrength = "Razoável";
    } else {
        signalStrength = "Fraco";
    }
    doc["wifi"]["signal_strength"] = signalStrength;

    // SPIFFS
    doc["spiffs"]["ready"] = spiffsManager->isReady();
    if (spiffsManager->isReady()) {
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
    doc["cpu"]["chip_revision"] = ESP.getChipRevision();
    doc["cpu"]["sdk_version"] = ESP.getSdkVersion();

    // Flash
    doc["flash"]["size_mb"] = ESP.getFlashChipSize() / (1024 * 1024);
    doc["flash"]["speed_mhz"] = ESP.getFlashChipSpeed() / 1000000;

    // System
    doc["system"]["reset_reason"] = esp_reset_reason();
    doc["system"]["compile_date"] = __DATE__;
    doc["system"]["compile_time"] = __TIME__;

    // OTA
    doc["ota"]["upload_in_progress"] = otaUploadInProgress;
    #ifdef OTA_NO_ROLLBACK
    doc["ota"]["rollback_enabled"] = false;
    #else
    doc["ota"]["rollback_enabled"] = true;
    #endif

    bool isHealthy = WiFi.status() == WL_CONNECTED && ESP.getFreeHeap() > 50000;
    doc["status"] = isHealthy ? "healthy" : "degraded";
    doc["timestamp"] = uptimeMs;

    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}

void WebServerManager::handleFileList(AsyncWebServerRequest *request) {
    if (!checkAuth(request)) return;
    if (otaUploadInProgress) {
      request->send(503, "application/json", "{\"error\":\"System busy\"}");
      return;
    }
    if (!spiffsManager->isReady()) {
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
}

void WebServerManager::handleFileDownload(AsyncWebServerRequest *request) {
    if (!checkAuth(request)) return;
    if (otaUploadInProgress) { request->send(503, "text/plain", "System busy"); return; }
    if (!spiffsManager->isReady()) { request->send(503, "text/plain", "SPIFFS not ready"); return; }
    if (!request->hasParam("file")) { request->send(400, "text/plain", "Missing file parameter"); return; }

    String filepath = request->getParam("file")->value();
    if (!LittleFS.exists(filepath)) { request->send(404, "text/plain", "File not found"); return; }

    request->send(LittleFS, filepath, String(), true);
}

void WebServerManager::handleFileView(AsyncWebServerRequest *request) {
    if (!checkAuth(request)) return;
    if (otaUploadInProgress) { request->send(503, "text/plain", "System busy"); return; }
    if (!request->hasParam("file")) { request->send(400, "text/plain", "Missing file parameter"); return; }

    String filepath = request->getParam("file")->value();
    if (!LittleFS.exists(filepath)) { request->send(404, "text/plain", "File not found"); return; }

    request->send(LittleFS, filepath, "text/plain", false);
}

void WebServerManager::handleFileRead(AsyncWebServerRequest *request) {
    if (!checkAuth(request)) return;
    if (otaUploadInProgress) { request->send(503, "application/json", "{\"error\":\"System busy\"}"); return; }
    if (!request->hasParam("file")) { request->send(400, "application/json", "{\"error\":\"Missing file parameter\"}"); return; }

    String filepath = request->getParam("file")->value();
    if (!LittleFS.exists(filepath)) { request->send(404, "application/json", "{\"error\":\"File not found\"}"); return; }

    if (xSemaphoreTake(*spiffsMutex, pdMS_TO_TICKS(5000)) == pdTRUE) {
      File file = LittleFS.open(filepath, FILE_READ);
      if (!file) {
        xSemaphoreGive(*spiffsMutex);
        request->send(500, "application/json", "{\"error\":\"Failed to open file\"}");
        return;
      }

      size_t fileSize = file.size();
      if (fileSize > 51200) {
        file.close();
        xSemaphoreGive(*spiffsMutex);
        request->send(413, "application/json", "{\"error\":\"File too large (max 50KB)\"}");
        return;
      }

      // Check if we have enough heap memory before allocating
      // Need: buffer + String overhead + JsonDocument + response String
      size_t requiredHeap = (fileSize * 3) + 2048; // Conservative estimate
      if (ESP.getFreeHeap() < requiredHeap) {
        file.close();
        xSemaphoreGive(*spiffsMutex);
        Serial.printf("ERROR: Insufficient heap for file read. Need: %u, Available: %u\n",
                      requiredHeap, ESP.getFreeHeap());
        request->send(503, "application/json", "{\"error\":\"Insufficient memory\"}");
        return;
      }

      // Allocate buffer and read file in one operation (prevents fragmentation)
      char* buffer = (char*)malloc(fileSize + 1);
      if (!buffer) {
        file.close();
        xSemaphoreGive(*spiffsMutex);
        Serial.println("ERROR: Failed to allocate buffer for file read");
        request->send(500, "application/json", "{\"error\":\"Memory allocation failed\"}");
        return;
      }

      // Read entire file at once
      size_t bytesRead = file.readBytes(buffer, fileSize);
      buffer[bytesRead] = '\0';

      String content = String(buffer);
      free(buffer); // Free buffer immediately after use

      file.close();
      xSemaphoreGive(*spiffsMutex);

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
}

void WebServerManager::handleFileWrite(AsyncWebServerRequest *request) {
    if (!checkAuth(request)) return;
    if (otaUploadInProgress) { request->send(503, "application/json", "{\"error\":\"System busy\"}"); return; }
    if (!request->hasParam("file", true) || !request->hasParam("content", true)) {
      request->send(400, "application/json", "{\"error\":\"Missing parameters\"}");
      return;
    }

    String filepath = request->getParam("file", true)->value();
    String content = request->getParam("content", true)->value();

    if (xSemaphoreTake(*spiffsMutex, pdMS_TO_TICKS(5000)) == pdTRUE) {
      File file = LittleFS.open(filepath, FILE_WRITE);
      if (!file) {
        xSemaphoreGive(*spiffsMutex);
        request->send(500, "application/json", "{\"error\":\"Failed to open file\"}");
        return;
      }

      size_t written = file.print(content);
      file.close();
      xSemaphoreGive(*spiffsMutex);

      if (written > 0) {
        request->send(200, "application/json", "{\"status\":\"ok\"}");
      } else {
        request->send(500, "application/json", "{\"error\":\"Failed to write\"}");
      }
    } else {
      request->send(503, "application/json", "{\"error\":\"SPIFFS busy\"}");
    }
}

void WebServerManager::handleFileDelete(AsyncWebServerRequest *request) {
    if (!checkAuth(request)) return;
    if (otaUploadInProgress) { request->send(503, "application/json", "{\"error\":\"System busy\"}"); return; }
    if (!request->hasParam("file", true)) { request->send(400, "application/json", "{\"error\":\"Missing file parameter\"}"); return; }

    String filepath = request->getParam("file", true)->value();
    File file = LittleFS.open(filepath);
    if (!file) { request->send(404, "application/json", "{\"error\":\"File not found\"}"); return; }

    bool isDir = file.isDirectory();
    file.close();

    bool success = isDir ? LittleFS.rmdir(filepath) : LittleFS.remove(filepath);
    if (success) request->send(200, "application/json", "{\"status\":\"ok\"}");
    else request->send(500, "application/json", "{\"error\":\"Failed to delete\"}");
}

void WebServerManager::handleFileCreateDir(AsyncWebServerRequest *request) {
    if (!checkAuth(request)) return;
    if (otaUploadInProgress) { request->send(503, "application/json", "{\"error\":\"System busy\"}"); return; }
    if (!request->hasParam("dir", true)) { request->send(400, "application/json", "{\"error\":\"Missing dir parameter\"}"); return; }

    String dirpath = request->getParam("dir", true)->value();
    if (LittleFS.mkdir(dirpath)) request->send(200, "application/json", "{\"status\":\"ok\"}");
    else request->send(500, "application/json", "{\"error\":\"Failed to create directory\"}");
}

void WebServerManager::handleFileUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
    if (!checkAuth(request)) return;
    static File uploadFile;
    if (otaUploadInProgress) return;

    if (index == 0) {
        String path = "/";
        if (request->hasParam("dir", false)) {
          path = request->getParam("dir", false)->value();
          if (path != "/" && !path.endsWith("/")) path += "/";
        }
        String filepath = path + filename;
        if (LittleFS.exists(filepath)) LittleFS.remove(filepath);
        uploadFile = LittleFS.open(filepath, FILE_WRITE);
    }

    if (uploadFile && len) uploadFile.write(data, len);
    if (final && uploadFile) uploadFile.close();
}

void WebServerManager::handleWiFiScan(AsyncWebServerRequest *request) {
    if (!checkAuth(request)) return;
    JsonDocument doc;
    int n = WiFi.scanNetworks();
    JsonArray networks = doc["networks"].to<JsonArray>();
    for (int i = 0; i < n; i++) {
        JsonObject network = networks.add<JsonObject>();
        network["ssid"] = WiFi.SSID(i);
        network["rssi"] = WiFi.RSSI(i);
        network["encryption"] = (int)WiFi.encryptionType(i);
        network["channel"] = WiFi.channel(i);
    }
    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
    WiFi.scanDelete();
}

void WebServerManager::handleWiFiConnect(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    if (!checkAuth(request)) return;
    if (index == 0) {
        JsonDocument doc;
        if (deserializeJson(doc, data, len)) { request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}"); return; }

        const char* ssid = doc["ssid"];
        const char* password = doc["password"];

        if (xSemaphoreTake(*spiffsMutex, pdMS_TO_TICKS(5000)) == pdTRUE) {
            File configFile = LittleFS.open("/config.json", "r");
            JsonDocument configDoc;
            if (configFile) { deserializeJson(configDoc, configFile); configFile.close(); }
            
            configDoc["wifi"]["ssid"] = ssid;
            configDoc["wifi"]["password"] = password ? password : "";
            configDoc["wifi"]["ap_mode"] = false;

            configFile = LittleFS.open("/config.json", "w");
            if (configFile) {
                serializeJson(configDoc, configFile);
                configFile.close();
                request->send(200, "application/json", "{\"status\":\"ok\",\"message\":\"Configuration saved. Rebooting...\"}");
                xSemaphoreGive(*spiffsMutex);
                delay(2000);
                ESP.restart();
            } else {
                xSemaphoreGive(*spiffsMutex);
                request->send(500, "application/json", "{\"error\":\"Failed to save configuration\"}");
            }
        } else {
            request->send(503, "application/json", "{\"error\":\"System busy\"}");
        }
    }
}

void WebServerManager::handleMQTTStatus(AsyncWebServerRequest *request) {
    if (!checkAuth(request)) return;
    JsonDocument doc;
    const MQTTManager::MQTTConfig& config = mqttManager->getConfig();
    doc["enabled"] = config.enabled;
    doc["connected"] = mqttManager->isConnected();
    doc["server"] = config.server;
    doc["port"] = config.port;
    doc["main_topic"] = config.mainTopic;
    doc["client_id"] = config.clientId;
    if (!mqttManager->isConnected() && config.enabled) {
      doc["error"] = mqttManager->getLastError();
      doc["state"] = mqttManager->getState();
    }
    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}

void WebServerManager::handleMQTTConfigGet(AsyncWebServerRequest *request) {
    if (!checkAuth(request)) return;
    JsonDocument doc;
    const MQTTManager::MQTTConfig& config = mqttManager->getConfig();
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
}

void WebServerManager::handleMQTTConfigPost(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    if (!checkAuth(request)) return;
    if (index == 0) {
        JsonDocument doc;
        if (deserializeJson(doc, data, len)) { request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}"); return; }

        mqttManager->updateConfig(doc["server"] | "", doc["port"] | 1883, doc["username"] | "", doc["password"] | "", doc["main_topic"] | "esp32/data", doc["enabled"] | false);

        if (xSemaphoreTake(*spiffsMutex, pdMS_TO_TICKS(5000)) == pdTRUE) {
            File configFile = LittleFS.open("/config.json", "r");
            JsonDocument configDoc;
            if (configFile) { deserializeJson(configDoc, configFile); configFile.close(); }
            
            mqttManager->saveConfig(configDoc);
            
            configFile = LittleFS.open("/config.json", "w");
            if (configFile) {
                serializeJson(configDoc, configFile);
                configFile.close();
                request->send(200, "application/json", "{\"status\":\"ok\",\"message\":\"Configuration saved\"}");
            } else {
                request->send(500, "application/json", "{\"error\":\"Failed to save configuration\"}");
            }
            xSemaphoreGive(*spiffsMutex);
        } else {
            request->send(503, "application/json", "{\"error\":\"System busy\"}");
        }
    }
}

void WebServerManager::handleMQTTTest(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    if (!checkAuth(request)) return;
    if (index == 0) {
        JsonDocument doc;
        if (deserializeJson(doc, data, len)) { request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}"); return; }

        const char* server = doc["server"];
        uint16_t port = doc["port"] | 1883;
        const char* username = doc["username"] | "";
        const char* password = doc["password"] | "";

        if (!server || strlen(server) == 0) { request->send(400, "application/json", "{\"error\":\"Server is required\"}"); return; }

        WiFiClient testWifiClient;
        PubSubClient testMqttClient(testWifiClient);
        testMqttClient.setServer(server, port);

        bool connected = (strlen(username) > 0) ? testMqttClient.connect("ESP32_TEST", username, password) : testMqttClient.connect("ESP32_TEST");

        if (connected) {
            testMqttClient.disconnect();
            request->send(200, "application/json", "{\"success\":true,\"message\":\"Connection successful\"}");
        } else {
            request->send(200, "application/json", "{\"success\":false,\"error\":\"Connection failed\"}");
        }
    }
}

void WebServerManager::handleMQTTPublish(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    if (!checkAuth(request)) return;
    if (index == 0) {
        JsonDocument doc;
        if (deserializeJson(doc, data, len)) { request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}"); return; }

        const char* topic = doc["topic"];
        const char* message = doc["message"];
        bool retained = doc["retained"] | false;

        if (!message) { request->send(400, "application/json", "{\"error\":\"Message is required\"}"); return; }
        if (!mqttManager->isConnected()) { request->send(503, "application/json", "{\"error\":\"MQTT not connected\"}"); return; }

        bool success;
        if (topic && strlen(topic) > 0) success = mqttManager->publish(topic, message, retained);
        else success = mqttManager->publishToMainTopic(message, retained);

        if (success) request->send(200, "application/json", "{\"status\":\"ok\"}");
        else request->send(500, "application/json", "{\"error\":\"Failed to publish\"}");
    }
}

void WebServerManager::handleDisplayStatus(AsyncWebServerRequest *request) {
    if (!checkAuth(request)) return;
    JsonDocument doc;
    const OLEDManager::OLEDConfig& config = oledManager->getConfig();
    doc["enabled"] = config.enabled;
    doc["available"] = oledManager->isAvailable();
    doc["address"] = config.address;
    doc["sda_pin"] = config.sda_pin;
    doc["scl_pin"] = config.scl_pin;
    doc["mode"] = (int)oledManager->getMode();
    if (!oledManager->isAvailable() && config.enabled) doc["error"] = oledManager->getLastError();
    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}

void WebServerManager::handleDisplayConfigGet(AsyncWebServerRequest *request) {
    if (!checkAuth(request)) return;
    JsonDocument doc;
    const OLEDManager::OLEDConfig& config = oledManager->getConfig();
    doc["enabled"] = config.enabled;
    doc["address"] = config.address;
    doc["sda_pin"] = config.sda_pin;
    doc["scl_pin"] = config.scl_pin;
    doc["rst_pin"] = config.rst_pin;
    doc["brightness"] = config.brightness;
    doc["flip_display"] = config.flip_display;
    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}

void WebServerManager::handleDisplayConfigPost(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    if (!checkAuth(request)) return;
    if (index == 0) {
        JsonDocument doc;
        if (deserializeJson(doc, data, len)) { request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}"); return; }

        oledManager->updateConfig(doc["enabled"]|false, doc["address"]|0x3C, doc["sda_pin"]|21, doc["scl_pin"]|22, doc["rst_pin"]|-1, doc["auto_update"]|true, doc["brightness"]|128, doc["flip_display"]|false);

        if (xSemaphoreTake(*spiffsMutex, pdMS_TO_TICKS(5000)) == pdTRUE) {
            File configFile = LittleFS.open("/config.json", "r");
            JsonDocument configDoc;
            if (configFile) { deserializeJson(configDoc, configFile); configFile.close(); }
            
            oledManager->saveConfig(configDoc);
            
            configFile = LittleFS.open("/config.json", "w");
            if (configFile) {
                serializeJson(configDoc, configFile);
                configFile.close();
                request->send(200, "application/json", "{\"status\":\"ok\",\"message\":\"Configuration saved\"}");
            } else {
                request->send(500, "application/json", "{\"error\":\"Failed to save configuration\"}");
            }
            xSemaphoreGive(*spiffsMutex);
        } else {
            request->send(503, "application/json", "{\"error\":\"System busy\"}");
        }
    }
}

void WebServerManager::handleDisplayMode(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    if (!checkAuth(request)) return;
    if (index == 0) {
        JsonDocument doc;
        if (deserializeJson(doc, data, len)) { request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}"); return; }
        if (!oledManager->isAvailable()) { request->send(503, "application/json", "{\"error\":\"Display not available\"}"); return; }

        int mode = doc["mode"] | 0;
        switch (mode) {
            case 0: oledManager->setMode(OLEDManager::MODE_OFF); break;
            case 1: oledManager->setMode(OLEDManager::MODE_LOGO); oledManager->showLogo(); break;
            case 2: oledManager->setMode(OLEDManager::MODE_SYSTEM_INFO); break;
            case 3: oledManager->setMode(OLEDManager::MODE_NETWORK_INFO); break;
            case 4: oledManager->setMode(OLEDManager::MODE_MQTT_INFO); break;
            case 5: oledManager->setMode(OLEDManager::MODE_CUSTOM_TEXT); oledManager->showCustomText(doc["line1"]|"", doc["line2"]|"", doc["line3"]|"", doc["line4"]|""); break;
            case 6: oledManager->setMode(OLEDManager::MODE_SENSOR_INFO); break;
            default: request->send(400, "application/json", "{\"error\":\"Invalid mode\"}"); return;
        }
        request->send(200, "application/json", "{\"status\":\"ok\"}");
    }
}

void WebServerManager::handleSensorStatus(AsyncWebServerRequest *request) {
    if (!checkAuth(request)) return;
    JsonDocument doc;
    const SensorManager::SensorConfig& cfg = sensorManager->getConfig();
    const SensorData& data = sensorManager->getData();
    doc["enabled"] = cfg.enabled;
    doc["available"] = sensorManager->isAvailable();
    doc["valid"] = data.valid;
    doc["sensor_type"] = sensorTypeToString(sensorManager->getDetectedSensorType());
    doc["sensor_name"] = sensorManager->getDetectedSensorName();
    if (data.valid) {
        doc["temperature"] = sensorManager->getTemperature();
        doc["temperature_f"] = sensorManager->getTemperatureFahrenheit();
        doc["humidity"] = sensorManager->getHumidity();
        doc["timestamp"] = data.timestamp;
        doc["fahrenheit"] = cfg.fahrenheit;
    }
    if (!sensorManager->isAvailable()) doc["error"] = sensorManager->getLastError();
    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}

void WebServerManager::handleSensorConfigGet(AsyncWebServerRequest *request) {
    if (!checkAuth(request)) return;
    JsonDocument doc;
    const SensorManager::SensorConfig& cfg = sensorManager->getConfig();
    doc["enabled"] = cfg.enabled;
    doc["read_interval"] = cfg.read_interval;
    doc["fahrenheit"] = cfg.fahrenheit;
    doc["sensor_type"] = sensorTypeToString(cfg.sensorType);
    doc["detected_sensor"] = sensorManager->getDetectedSensorName();
    if (cfg.customAddress > 0) {
        doc["custom_address"] = cfg.customAddress;
    }
    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}

void WebServerManager::handleSensorConfigPost(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    if (!checkAuth(request)) return;
    if (index == 0) {
        JsonDocument doc;
        if (deserializeJson(doc, data, len)) { request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}"); return; }

        // Parse sensor type if provided
        SensorType sensorType = SensorType::AUTO;
        if (doc.containsKey("sensor_type")) {
            sensorType = stringToSensorType(doc["sensor_type"].as<String>());
        }

        sensorManager->updateConfig(doc["enabled"]|false, doc["read_interval"]|60, doc["fahrenheit"]|false, sensorType);

        if (xSemaphoreTake(*spiffsMutex, pdMS_TO_TICKS(5000)) == pdTRUE) {
            File configFile = LittleFS.open("/config.json", "r");
            JsonDocument configDoc;
            if (configFile) { deserializeJson(configDoc, configFile); configFile.close(); }

            sensorManager->saveConfig(configDoc);
            
            configFile = LittleFS.open("/config.json", "w");
            if (configFile) {
                serializeJson(configDoc, configFile);
                configFile.close();
                request->send(200, "application/json", "{\"status\":\"ok\",\"message\":\"Configuration saved\"}");
            } else {
                request->send(500, "application/json", "{\"error\":\"Failed to save configuration\"}");
            }
            xSemaphoreGive(*spiffsMutex);
        } else {
            request->send(503, "application/json", "{\"error\":\"System busy\"}");
        }
    }
}

void WebServerManager::handleOTA(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
    if (!checkAuth(request)) return;
    if (index == 0) {
        otaUploadError = "";
        esp_task_wdt_delete(xTaskGetIdleTaskHandleForCPU(0));
        esp_task_wdt_delete(xTaskGetIdleTaskHandleForCPU(1));

        if (xSemaphoreTake(*spiffsMutex, pdMS_TO_TICKS(10000)) != pdTRUE) {
            otaUploadError = "SPIFFS is busy";
            return;
        }
        otaUploadInProgress = true;

        if (!isValidESP32Firmware(data, len)) {
            otaUploadError = "Invalid ESP32 firmware file";
            xSemaphoreGive(*spiffsMutex);
            otaUploadInProgress = false;
            return;
        }

        if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH)) {
            otaUploadError = "Failed to begin OTA update: " + String(Update.errorString());
            xSemaphoreGive(*spiffsMutex);
            otaUploadInProgress = false;
            return;
        }
    }

    if (len) {
        yield();
        if (Update.write(data, len) != len) {
            otaUploadError = "Failed to write firmware data";
            Update.abort();
        }
        yield();
    }

    if (final) {
        if (!Update.end(true)) {
            otaUploadError = "Failed to finalize OTA update: " + String(Update.errorString());
        }
    }
}

bool WebServerManager::isValidESP32Firmware(uint8_t *data, size_t len) {
    if (len < 1) return false;
    return data[0] == 0xE9;
}

void WebServerManager::loop() {
    ws.cleanupClients();
}

void WebServerManager::broadcastLog(const String& message) {
    if (ws.count() > 0) {
        ws.textAll("LOG:" + message);
    }
}

void WebServerManager::onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
    if (type == WS_EVT_CONNECT) {
        Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
        client->text("Connected to ESP32 Log Console");
    } else if (type == WS_EVT_DISCONNECT) {
        Serial.printf("WebSocket client #%u disconnected\n", client->id());
    }
}
