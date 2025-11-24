#include "web_server.h"
#include <Update.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>
#include <esp_task_wdt.h>

WebServerManager::WebServerManager() : server(80), ws("/ws"), otaUploadInProgress(false) {
}

void WebServerManager::begin(SPIFFSManager* spiffs, MQTTManager* mqtt, OLEDManager* oled, SensorManager* sensor, NTPManager* ntp, LoRaWANManager* lorawan, SemaphoreHandle_t* mutex) {
    this->spiffsManager = spiffs;
    this->mqttManager = mqtt;
    this->oledManager = oled;
    this->sensorManager = sensor;
    this->ntpManager = ntp;
    this->lorawanManager = lorawan;
    this->spiffsMutex = mutex;

    // Carregar credenciais web do config.json
    if (!loadWebCredentials()) {
        Serial.println("AVISO: Falha ao carregar credenciais web. Usando valores padrao.");
        webUsername = "admin";
        webPasswordHash = "8c6976e5b5410415bde908bd4dee15dfb167a9c873fc4bb8a81f6f2ab448a918"; // SHA256("admin")
        firstLogin = true;
    }

    ws.onEvent([this](AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
        onWsEvent(server, client, type, arg, data, len);
    });
    server.addHandler(&ws);

    setupRoutes();
    server.begin();
    Serial.println("Web server started");
}

bool WebServerManager::checkAuth(AsyncWebServerRequest *request) {
    // Serial.println("=== checkAuth() CALLED ===");

    // Extrair credenciais da requisição HTTP Basic Auth
    if (!request->hasHeader("Authorization")) {
        Serial.println("No Authorization header - requesting Basic authentication");
        // Use Basic authentication instead of Digest (default)
        request->requestAuthentication(NULL, false);  // NULL realm, false = Basic auth
        return false;
    }

    String authHeader = request->header("Authorization");
    if (!authHeader.startsWith("Basic ")) {
        Serial.println("Not Basic auth - requesting Basic authentication");
        request->requestAuthentication(NULL, false);  // NULL realm, false = Basic auth
        return false;
    }

    // Decodificar Base64 manualmente
    String base64Credentials = authHeader.substring(6);
    base64Credentials.trim();

    // Validar tamanho do input (segurança contra ataques)
    // Max: username(32) + ':' + password(64) = 97 bytes → base64 ~130 bytes
    const size_t MAX_CREDENTIALS_SIZE = 256;  // Buffer fixo seguro
    int inputLen = base64Credentials.length();

    if (inputLen > MAX_CREDENTIALS_SIZE * 4 / 3) {  // Validação antes de alocar
        Serial.printf("Auth: Input too large (%d bytes), rejecting\n", inputLen);
        request->requestAuthentication(NULL, false);
        return false;
    }

    // Buffer FIXO (não VLA) - previne stack overflow
    char decoded[MAX_CREDENTIALS_SIZE];
    int decodedLen = (inputLen * 3) / 4;

    if (decodedLen >= (int)MAX_CREDENTIALS_SIZE) {  // Double-check
        request->requestAuthentication(NULL, false);
        return false;
    }

    // Decodificar base64
    const char* base64Chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    int val = 0, valb = -8;
    int outIdx = 0;

    for (int i = 0; i < inputLen; i++) {
        const char* p = strchr(base64Chars, base64Credentials[i]);
        if (!p) continue;
        val = (val << 6) + (p - base64Chars);
        valb += 6;
        if (valb >= 0) {
            decoded[outIdx++] = char((val >> valb) & 0xFF);
            valb -= 8;
        }
    }
    decoded[outIdx] = '\0';

    // Encontrar separador ':' diretamente em decoded
    char* separator = strchr(decoded, ':');
    if (separator == nullptr) {
        request->requestAuthentication(NULL, false);
        return false;
    }

    // Separar username e password
    *separator = '\0';  // Terminar username
    const char* username = decoded;
    const char* password = separator + 1;

    // Debug: mostrar credenciais recebidas
    // Serial.printf("Auth attempt - Username: '%s', Password: '%s'\n", username, password);
    // Serial.printf("Expected - Username: '%s', Hash: '%s'\n", webUsername.c_str(), webPasswordHash.c_str());

    // Validar username e hash de senha
    if (strcmp(username, webUsername.c_str()) != 0) {
        Serial.printf("Auth FAILED: Username mismatch (got '%s', expected '%s')\n", username, webUsername.c_str());
        request->requestAuthentication(NULL, false);
        return false;
    }

    if (!AuthManager::verifyPassword(password, webPasswordHash.c_str())) {
        Serial.println("Auth FAILED: Password verification failed");

        // Debug: gerar hash da senha fornecida para comparar
        char passwordHash[65];
        if (AuthManager::hashPassword(password, passwordHash, sizeof(passwordHash))) {
            Serial.printf("Password hash generated: '%s'\n", passwordHash);
            Serial.printf("Expected hash:          '%s'\n", webPasswordHash.c_str());
        }

        request->requestAuthentication(NULL, false);
        return false;
    }

    // Serial.println("Auth SUCCESS!");
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

bool WebServerManager::loadWebCredentials() {
    if (!spiffsManager->isReady()) {
        Serial.println("SPIFFS não disponível para carregar credenciais web");
        return false;
    }

    File file = LittleFS.open("/config.json", FILE_READ);
    if (!file) {
        Serial.println("Falha ao abrir config.json para ler credenciais web");
        return false;
    }

    StaticJsonDocument<1536> doc;  // Config: wifi + mqtt + oled + sensor + web
    DeserializationError error = deserializeJson(doc, file);
    file.close();

    if (error) {
        Serial.print("Falha ao parsear config.json: ");
        Serial.println(error.c_str());
        return false;
    }

    // Carregar credenciais web
    if (doc.containsKey("web")) {
        webUsername = doc["web"]["username"].as<String>();
        webPasswordHash = doc["web"]["password_hash"].as<String>();
        firstLogin = doc["web"]["first_login"] | true; // Default true

        Serial.println("==============================================");
        Serial.println("Credenciais web carregadas do config.json");
        Serial.print("Username: ");
        Serial.println(webUsername);
        Serial.print("Password Hash: ");
        Serial.println(webPasswordHash);
        Serial.print("Primeiro login: ");
        Serial.println(firstLogin ? "SIM" : "NAO");
        Serial.println("==============================================");
        return true;
    }

    Serial.println("ERRO: Seção 'web' não encontrada no config.json");
    return false;
}

bool WebServerManager::saveWebCredentials(const String& username, const String& passwordHash, bool firstLoginFlag) {
    if (!spiffsManager->isReady()) {
        Serial.println("SPIFFS não disponível para salvar credenciais web");
        return false;
    }

    // Ler config atual
    File file = LittleFS.open("/config.json", FILE_READ);
    if (!file) {
        Serial.println("Falha ao abrir config.json para atualizar credenciais");
        return false;
    }

    StaticJsonDocument<1536> doc;  // Config: wifi + mqtt + oled + sensor + web
    DeserializationError error = deserializeJson(doc, file);
    file.close();

    if (error) {
        Serial.print("Falha ao parsear config.json: ");
        Serial.println(error.c_str());
        return false;
    }

    // Atualizar seção web
    doc["web"]["username"] = username;
    doc["web"]["password_hash"] = passwordHash;
    doc["web"]["first_login"] = firstLoginFlag;

    // Salvar de volta
    file = LittleFS.open("/config.json", FILE_WRITE);
    if (!file) {
        Serial.println("Falha ao abrir config.json para escrita");
        return false;
    }

    if (serializeJson(doc, file) == 0) {
        Serial.println("Falha ao escrever config.json");
        file.close();
        return false;
    }

    file.close();

    // Atualizar variáveis em memória
    webUsername = username;
    webPasswordHash = passwordHash;
    firstLogin = firstLoginFlag;

    Serial.println("Credenciais web salvas com sucesso");
    return true;
}

bool WebServerManager::isValidPath(const String& path) {
    // Debug path validation
    // Serial.printf("Validating path: '%s' (len: %d)\n", path.c_str(), path.length());

    // Verificar se o path está vazio
    if (path.length() == 0) {
        Serial.println("Path Traversal bloqueado: path vazio");
        return false;
    }

    // Verificar se contém sequências de path traversal
    if (path.indexOf("..") >= 0) {
        Serial.print("Path Traversal bloqueado: '..' detectado em: ");
        Serial.println(path);
        return false;
    }

    // REMOVIDO: Verificação de null byte causando falsos positivos
    // if (path.indexOf('\0') >= 0) { ... }

    // Verificar se começa com /
    if (!path.startsWith("/")) {
        Serial.print("Path Traversal bloqueado: path não começa com '/': ");
        Serial.println(path);
        return false;
    }

    // Verificar se contém barras invertidas (Windows-style paths)
    if (path.indexOf('\\') >= 0) {
        Serial.println("Path Traversal bloqueado: barra invertida detectada");
        return false;
    }

    // Limitar tamanho do path (prevenir buffer overflow)
    if (path.length() > 128) {
        Serial.println("Path Traversal bloqueado: path muito longo");
        return false;
    }

    // Validar caracteres permitidos (alphanumerico, /, -, _, ., espaço)
    for (size_t i = 0; i < path.length(); i++) {
        char c = path.charAt(i);
        // Adicionado suporte a espaço (' ')
        if (!isalnum(c) && c != '/' && c != '-' && c != '_' && c != '.' && c != ' ') {
            Serial.print("Path Traversal bloqueado: caractere invalido '");
            Serial.print(c);
            Serial.print("' (0x");
            Serial.print((int)c, HEX);
            Serial.print(") em: ");
            Serial.println(path);
            return false;
        }
    }

    // Path válido
    return true;
}

void WebServerManager::serveStaticFile(AsyncWebServerRequest *request, const char* filepath, const char* contentType) {
    if (!checkAuth(request)) return;

    if (!spiffsManager->isReady()) {
        request->send(503, "text/plain", "SPIFFS not available");
        return;
    }

    // Try to serve gzipped version if available and client supports it
    bool clientAcceptsGzip = false;
    if (request->hasHeader("Accept-Encoding")) {
        String encoding = request->header("Accept-Encoding");
        clientAcceptsGzip = (encoding.indexOf("gzip") != -1);
    }

    // Build .gz filename
    char gzFilepath[128];
    snprintf(gzFilepath, sizeof(gzFilepath), "%s.gz", filepath);

    // Serve .gz if available and client supports it
    if (clientAcceptsGzip && LittleFS.exists(gzFilepath)) {
        AsyncWebServerResponse *response = request->beginResponse(LittleFS, gzFilepath, contentType);
        if (response) {
            response->addHeader("Content-Encoding", "gzip");
            response->addHeader("Cache-Control", "public, max-age=3600");
            request->send(response);
            return;
        }
    }

    // Fallback to uncompressed file
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

    server.on("/auth", HTTP_GET, [this](AsyncWebServerRequest *request) {
        if (!checkAuth(request)) return;
        validateOTABoot();
        if (spiffsManager->isReady()) request->send(LittleFS, "/web/auth.html", "text/html");
        else request->send(503, "text/plain", "SPIFFS not ready");
    });
    server.on("/auth.js", HTTP_GET, [this](AsyncWebServerRequest *request) {
        serveStaticFile(request, "/web/auth.js", "application/javascript");
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

    server.on("/ntp", HTTP_GET, [this](AsyncWebServerRequest *request) {
        if (!checkAuth(request)) return;
        validateOTABoot();
        if (spiffsManager->isReady()) request->send(LittleFS, "/web/ntp.html", "text/html");
        else request->send(503, "text/plain", "SPIFFS not ready");
    });
    server.on("/ntp.js", HTTP_GET, [this](AsyncWebServerRequest *request) {
        serveStaticFile(request, "/web/ntp.js", "application/javascript");
    });

    server.on("/api/ntp/config", HTTP_GET, [this](AsyncWebServerRequest *request) { handleNTPConfigGet(request); });
    server.on("/api/ntp/config", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL,
        [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            handleNTPConfigPost(request, data, len, index, total);
        }
    );
    server.on("/api/ntp/time", HTTP_GET, [this](AsyncWebServerRequest *request) { handleNTPTime(request); });

    // LoRaWAN endpoints
    server.on("/lorawan", HTTP_GET, [this](AsyncWebServerRequest *request) {
        if (!checkAuth(request)) return;
        serveStaticFile(request, "/web/lorawan.html", "text/html");
    });
    server.on("/lorawan.js", HTTP_GET, [this](AsyncWebServerRequest *request) {
        serveStaticFile(request, "/web/lorawan.js", "application/javascript");
    });
    server.on("/api/lorawan/config", HTTP_GET, [this](AsyncWebServerRequest *request) { handleLoRaWANConfigGet(request); });
    server.on("/api/lorawan/config", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL,
        [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            handleLoRaWANConfigPost(request, data, len, index, total);
        }
    );
    server.on("/api/lorawan/status", HTTP_GET, [this](AsyncWebServerRequest *request) { handleLoRaWANStatus(request); });
    server.on("/api/lorawan/join", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL,
        [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            handleLoRaWANJoin(request, data, len, index, total);
        }
    );
    server.on("/api/lorawan/uplink", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL,
        [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            handleLoRaWANUplink(request, data, len, index, total);
        }
    );

    // Authentication endpoints
    server.on("/api/auth/status", HTTP_GET, [this](AsyncWebServerRequest *request) { handleAuthStatus(request); });
    server.on("/api/auth/change-password", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL,
        [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            handleChangePassword(request, data, len, index, total);
        }
    );

    server.on("/api/firmware/upload", HTTP_POST,
        [this](AsyncWebServerRequest *request) {
             if (otaUploadInProgress) {
                otaUploadInProgress = false;
                Serial.println("OTA upload finished - MutexGuard will auto-release mutex");
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
    StaticJsonDocument<1024> doc;  // System status response
    
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

      // Validação de Path Traversal
      if (!isValidPath(path)) {
          request->send(400, "application/json", "{\"error\":\"Invalid directory path\"}");
          return;
      }
    }

    File root = LittleFS.open(path);
    if (!root || !root.isDirectory()) {
      request->send(404, "application/json", "{\"error\":\"Directory not found\"}");
      return;
    }

    StaticJsonDocument<1024> doc;  // File list response
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

    // Validação de Path Traversal
    if (!isValidPath(filepath)) {
        request->send(400, "text/plain", "Invalid file path");
        return;
    }

    if (!LittleFS.exists(filepath)) { request->send(404, "text/plain", "File not found"); return; }

    request->send(LittleFS, filepath, String(), true);
}

void WebServerManager::handleFileView(AsyncWebServerRequest *request) {
    if (!checkAuth(request)) return;
    if (otaUploadInProgress) { request->send(503, "text/plain", "System busy"); return; }
    if (!request->hasParam("file")) { request->send(400, "text/plain", "Missing file parameter"); return; }

    String filepath = request->getParam("file")->value();

    // Validação de Path Traversal
    if (!isValidPath(filepath)) {
        request->send(400, "text/plain", "Invalid file path");
        return;
    }

    if (!LittleFS.exists(filepath)) { request->send(404, "text/plain", "File not found"); return; }

    request->send(LittleFS, filepath, "text/plain", false);
}

void WebServerManager::handleFileRead(AsyncWebServerRequest *request) {
    if (!checkAuth(request)) return;
    if (otaUploadInProgress) { request->send(503, "application/json", "{\"error\":\"System busy\"}"); return; }
    if (!request->hasParam("file")) { request->send(400, "application/json", "{\"error\":\"Missing file parameter\"}"); return; }

    String filepath = request->getParam("file")->value();

    // Validação de Path Traversal
    if (!isValidPath(filepath)) {
        request->send(400, "application/json", "{\"error\":\"Invalid file path\"}");
        return;
    }

    if (!LittleFS.exists(filepath)) { request->send(404, "application/json", "{\"error\":\"File not found\"}"); return; }

    // Use MutexGuard for automatic mutex management
    MutexGuard guard(*spiffsMutex, 5000, "fileRead");
    if (!guard.acquired()) {
        request->send(503, "application/json", "{\"error\":\"SPIFFS busy\"}");
        return;
    }

    File file = LittleFS.open(filepath, FILE_READ);
    if (!file) {
        request->send(500, "application/json", "{\"error\":\"Failed to open file\"}");
        return;
        // Mutex automatically released by MutexGuard destructor
    }

    size_t fileSize = file.size();
    if (fileSize > 51200) {
        file.close();
        request->send(413, "application/json", "{\"error\":\"File too large (max 50KB)\"}");
        return;
        // Mutex automatically released by MutexGuard destructor
    }

    // Check if we have enough heap memory before allocating
    // Need: buffer + String overhead + JsonDocument + response String
    size_t requiredHeap = (fileSize * 3) + 2048; // Conservative estimate
    if (ESP.getFreeHeap() < requiredHeap) {
        file.close();
        Serial.printf("ERROR: Insufficient heap for file read. Need: %u, Available: %u\n",
                      requiredHeap, ESP.getFreeHeap());
        request->send(503, "application/json", "{\"error\":\"Insufficient memory\"}");
        return;
        // Mutex automatically released by MutexGuard destructor
    }

    // Allocate buffer and read file in one operation (prevents fragmentation)
    char* buffer = (char*)malloc(fileSize + 1);
    if (!buffer) {
        file.close();
        Serial.println("ERROR: Failed to allocate buffer for file read");
        request->send(500, "application/json", "{\"error\":\"Memory allocation failed\"}");
        return;
        // Mutex automatically released by MutexGuard destructor
    }

    // Read entire file at once
    size_t bytesRead = file.readBytes(buffer, fileSize);
    buffer[bytesRead] = '\0';

    String content = String(buffer);
    free(buffer); // Free buffer immediately after use

    file.close();
    // Mutex automatically released by MutexGuard destructor when function returns

    StaticJsonDocument<256> doc;  // File read response
    doc["status"] = "ok";
    doc["content"] = content;
    doc["size"] = fileSize;

    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
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

    // Validação de Path Traversal
    if (!isValidPath(filepath)) {
        request->send(400, "application/json", "{\"error\":\"Invalid file path\"}");
        return;
    }

    // Use MutexGuard for automatic mutex management
    MutexGuard guard(*spiffsMutex, 5000, "fileWrite");
    if (!guard.acquired()) {
        request->send(503, "application/json", "{\"error\":\"SPIFFS busy\"}");
        return;
    }

    File file = LittleFS.open(filepath, FILE_WRITE);
    if (!file) {
        request->send(500, "application/json", "{\"error\":\"Failed to open file\"}");
        return;
        // Mutex automatically released
    }

    size_t written = file.print(content);
    file.close();
    // Mutex automatically released

    if (written > 0) {
        request->send(200, "application/json", "{\"status\":\"ok\"}");
    } else {
        request->send(500, "application/json", "{\"error\":\"Failed to write\"}");
    }
}

void WebServerManager::handleFileDelete(AsyncWebServerRequest *request) {
    if (!checkAuth(request)) return;
    if (otaUploadInProgress) { request->send(503, "application/json", "{\"error\":\"System busy\"}"); return; }
    if (!request->hasParam("file", true)) { request->send(400, "application/json", "{\"error\":\"Missing file parameter\"}"); return; }

    String filepath = request->getParam("file", true)->value();

    // Validação de Path Traversal
    if (!isValidPath(filepath)) {
        request->send(400, "application/json", "{\"error\":\"Invalid file path\"}");
        return;
    }

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

    // Validação de Path Traversal
    if (!isValidPath(dirpath)) {
        request->send(400, "application/json", "{\"error\":\"Invalid directory path\"}");
        return;
    }

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

          // Validação de Path Traversal no diretório
          if (!isValidPath(path)) {
              Serial.println("Path Traversal bloqueado em upload: path inválido");
              return;
          }

          if (path != "/" && !path.endsWith("/")) path += "/";
        }

        String filepath = path + filename;

        // Validação de Path Traversal no filepath completo
        if (!isValidPath(filepath)) {
            Serial.println("Path Traversal bloqueado em upload: filepath inválido");
            return;
        }

        if (LittleFS.exists(filepath)) LittleFS.remove(filepath);
        uploadFile = LittleFS.open(filepath, FILE_WRITE);
    }

    if (uploadFile && len) uploadFile.write(data, len);
    if (final && uploadFile) uploadFile.close();
}

void WebServerManager::handleWiFiScan(AsyncWebServerRequest *request) {
    if (!checkAuth(request)) return;
    StaticJsonDocument<1024> doc;  // WiFi scan list
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
        StaticJsonDocument<256> doc;  // WiFi connect response
        if (deserializeJson(doc, data, len)) { request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}"); return; }

        const char* ssid = doc["ssid"];
        const char* password = doc["password"];

        MutexGuard guard(*spiffsMutex, 5000, "wifiConnect");
        if (!guard.acquired()) {
            request->send(503, "application/json", "{\"error\":\"System busy\"}");
            return;
        }

        File configFile = LittleFS.open("/config.json", "r");
        StaticJsonDocument<1536> configDoc;  // Full config read
        if (configFile) { deserializeJson(configDoc, configFile); configFile.close(); }

        configDoc["wifi"]["ssid"] = ssid;
        configDoc["wifi"]["password"] = password ? password : "";
        configDoc["wifi"]["ap_mode"] = false;

        configFile = LittleFS.open("/config.json", "w");
        if (!configFile) {
            request->send(500, "application/json", "{\"error\":\"Failed to save configuration\"}");
            return;
        }

        serializeJson(configDoc, configFile);
        configFile.close();
        request->send(200, "application/json", "{\"status\":\"ok\",\"message\":\"Configuration saved. Rebooting...\"}");
        // Mutex will be automatically released by destructor before ESP.restart()
        delay(2000);
        ESP.restart();
    }
}

void WebServerManager::handleMQTTStatus(AsyncWebServerRequest *request) {
    if (!checkAuth(request)) return;
    StaticJsonDocument<512> doc;  // MQTT status
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
    StaticJsonDocument<512> doc;  // MQTT config get
    const MQTTManager::MQTTConfig& config = mqttManager->getConfig();
    doc["enabled"] = config.enabled;
    doc["server"] = config.server;
    doc["port"] = config.port;
    doc["username"] = config.username;
    doc["password"] = config.password;
    doc["hostname"] = config.hostname;
    doc["main_topic"] = config.mainTopic;
    doc["publish_interval"] = config.publish_interval;
    doc["client_id"] = config.clientId;
    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}

void WebServerManager::handleMQTTConfigPost(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    if (!checkAuth(request)) return;
    if (index == 0) {
        StaticJsonDocument<256> doc;  // MQTT config POST response
        if (deserializeJson(doc, data, len)) { request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}"); return; }

        mqttManager->updateConfig(doc["server"] | "", doc["port"] | 1883, doc["username"] | "", doc["password"] | "", doc["hostname"] | "ESP32-Device", doc["main_topic"] | "esp32/data", doc["publish_interval"] | 60, doc["enabled"] | false);

        MutexGuard guard(*spiffsMutex, 5000, "mqttConfigSave");
        if (!guard.acquired()) {
            request->send(503, "application/json", "{\"error\":\"System busy\"}");
            return;
        }

        File configFile = LittleFS.open("/config.json", "r");
        StaticJsonDocument<1536> configDoc;  // Full config read
        if (configFile) { deserializeJson(configDoc, configFile); configFile.close(); }

        mqttManager->saveConfig(configDoc);

        configFile = LittleFS.open("/config.json", "w");
        if (!configFile) {
            request->send(500, "application/json", "{\"error\":\"Failed to save configuration\"}");
            return;
        }

        serializeJson(configDoc, configFile);
        configFile.close();
        request->send(200, "application/json", "{\"status\":\"ok\",\"message\":\"Configuration saved\"}");
    }
}

void WebServerManager::handleMQTTTest(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    if (!checkAuth(request)) return;
    if (index == 0) {
        StaticJsonDocument<256> doc;  // MQTT test response
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
        StaticJsonDocument<256> doc;  // MQTT publish response
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
    StaticJsonDocument<512> doc;  // Display status
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
    StaticJsonDocument<512> doc;  // Display config get
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
        StaticJsonDocument<256> doc;  // Display config POST response
        if (deserializeJson(doc, data, len)) { request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}"); return; }

        oledManager->updateConfig(doc["enabled"]|false, doc["address"]|0x3C, doc["sda_pin"]|21, doc["scl_pin"]|22, doc["rst_pin"]|-1, doc["auto_update"]|true, doc["brightness"]|128, doc["flip_display"]|false);

        MutexGuard guard(*spiffsMutex, 5000, "displayConfigSave");
        if (!guard.acquired()) {
            request->send(503, "application/json", "{\"error\":\"System busy\"}");
            return;
        }

        File configFile = LittleFS.open("/config.json", "r");
        StaticJsonDocument<1536> configDoc;  // Full config read
        if (configFile) { deserializeJson(configDoc, configFile); configFile.close(); }

        oledManager->saveConfig(configDoc);

        configFile = LittleFS.open("/config.json", "w");
        if (!configFile) {
            request->send(500, "application/json", "{\"error\":\"Failed to save configuration\"}");
            return;
        }

        serializeJson(configDoc, configFile);
        configFile.close();
        request->send(200, "application/json", "{\"status\":\"ok\",\"message\":\"Configuration saved\"}");
    }
}

void WebServerManager::handleDisplayMode(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    if (!checkAuth(request)) return;
    if (index == 0) {
        StaticJsonDocument<256> doc;  // Display mode response
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
    StaticJsonDocument<512> doc;  // Sensor status
    const SensorManager::SensorConfig& cfg = sensorManager->getConfig();
    const SensorData& data = sensorManager->getData();
    doc["enabled"] = cfg.enabled;
    doc["available"] = sensorManager->isAvailable();
    doc["valid"] = data.valid;
    doc["sensor_type"] = sensorTypeToString(sensorManager->getDetectedSensorType());
    char sensorName[32];
    sensorManager->getDetectedSensorName(sensorName, sizeof(sensorName));
    doc["sensor_name"] = sensorName;
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
    StaticJsonDocument<512> doc;  // Sensor config get
    const SensorManager::SensorConfig& cfg = sensorManager->getConfig();
    doc["enabled"] = cfg.enabled;
    doc["read_interval"] = cfg.read_interval;
    doc["fahrenheit"] = cfg.fahrenheit;
    doc["sensor_type"] = sensorTypeToString(cfg.sensorType);
    char detectedSensor[32];
    sensorManager->getDetectedSensorName(detectedSensor, sizeof(detectedSensor));
    doc["detected_sensor"] = detectedSensor;
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
        StaticJsonDocument<256> doc;  // Sensor config POST response
        if (deserializeJson(doc, data, len)) { request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}"); return; }

        // Parse sensor type if provided
        SensorType sensorType = SensorType::AUTO;
        if (doc.containsKey("sensor_type")) {
            sensorType = stringToSensorType(doc["sensor_type"].as<String>());
        }

        sensorManager->updateConfig(doc["enabled"]|false, doc["read_interval"]|60, doc["fahrenheit"]|false, sensorType);

        MutexGuard guard(*spiffsMutex, 5000, "sensorConfigSave");
        if (!guard.acquired()) {
            request->send(503, "application/json", "{\"error\":\"System busy\"}");
            return;
        }

        File configFile = LittleFS.open("/config.json", "r");
        StaticJsonDocument<1536> configDoc;  // Full config read
        if (configFile) { deserializeJson(configDoc, configFile); configFile.close(); }

        sensorManager->saveConfig(configDoc);

        configFile = LittleFS.open("/config.json", "w");
        if (!configFile) {
            request->send(500, "application/json", "{\"error\":\"Failed to save configuration\"}");
            return;
        }

        serializeJson(configDoc, configFile);
        configFile.close();
        request->send(200, "application/json", "{\"status\":\"ok\",\"message\":\"Configuration saved\"}");
    }
}

void WebServerManager::handleAuthStatus(AsyncWebServerRequest *request) {
    if (!checkAuth(request)) return;

    StaticJsonDocument<512> doc;  // Auth status
    doc["username"] = webUsername;
    doc["first_login"] = firstLogin;
    doc["password_change_required"] = firstLogin;

    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}

void WebServerManager::handleChangePassword(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    if (!checkAuth(request)) return;

    if (index == 0) {
        StaticJsonDocument<256> doc;  // Change password request
        DeserializationError error = deserializeJson(doc, data, len);

        if (error) {
            request->send(400, "application/json", "{\"error\":\"JSON invalido\"}");
            return;
        }

        // Extrair dados
        const char* currentPassword = doc["current_password"] | "";
        const char* newPassword = doc["new_password"] | "";
        String newUsername = doc["username"] | webUsername; // Se não fornecido, manter atual

        // Validar senha atual
        if (!AuthManager::verifyPassword(currentPassword, webPasswordHash.c_str())) {
            request->send(401, "application/json", "{\"error\":\"Senha atual incorreta\"}");
            return;
        }

        // Validar força da nova senha
        char validationError[128];
        if (!AuthManager::getPasswordValidationError(newPassword, validationError, sizeof(validationError))) {
            // Senha inválida - validationError contém a mensagem
            StaticJsonDocument<256> errorDoc;  // Error response
            errorDoc["error"] = validationError;
            String errorResponse;
            serializeJson(errorDoc, errorResponse);
            request->send(400, "application/json", errorResponse);
            return;
        }

        // Gerar hash da nova senha
        char newPasswordHash[65];
        if (!AuthManager::hashPassword(newPassword, newPasswordHash, sizeof(newPasswordHash))) {
            request->send(500, "application/json", "{\"error\":\"Falha ao gerar hash de senha\"}");
            return;
        }

        // Salvar credenciais
        String newPasswordHashStr(newPasswordHash);  // Converter para String para saveWebCredentials
        if (saveWebCredentials(newUsername, newPasswordHashStr, false)) {
            StaticJsonDocument<256> successDoc;  // Success response
            successDoc["status"] = "ok";
            successDoc["message"] = "Senha alterada com sucesso";
            successDoc["username"] = webUsername;
            successDoc["first_login"] = false;
            String successResponse;
            serializeJson(successDoc, successResponse);
            request->send(200, "application/json", successResponse);

            Serial.println("Senha alterada com sucesso");
            Serial.print("Novo username: ");
            Serial.println(webUsername);
        } else {
            request->send(500, "application/json", "{\"error\":\"Falha ao salvar credenciais\"}");
        }
    }
}

void WebServerManager::handleNTPConfigGet(AsyncWebServerRequest *request) {
    if (!checkAuth(request)) return;

    const NTPManager::NTPConfig& config = ntpManager->getConfig();
    
    StaticJsonDocument<512> doc;
    doc["server"] = config.server;
    doc["offset"] = config.offset;
    doc["interval"] = config.interval;
    doc["enabled"] = config.enabled;
    
    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}

void WebServerManager::handleNTPConfigPost(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    if (!checkAuth(request)) return;
    if (index == 0) {
        StaticJsonDocument<256> doc;
        if (deserializeJson(doc, data, len)) { 
            request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}"); 
            return; 
        }

        const char* server = doc["server"] | "pool.ntp.org";
        long offset = doc["offset"] | -10800;
        int interval = doc["interval"] | 60000;
        bool enabled = doc["enabled"] | true;

        ntpManager->updateConfig(server, offset, interval, enabled);

        // Save to config.json
        MutexGuard guard(*spiffsMutex, 5000, "ntpConfigSave");
        if (!guard.acquired()) {
            request->send(503, "application/json", "{\"error\":\"System busy\"}");
            return;
        }

        File configFile = LittleFS.open("/config.json", "r");
        StaticJsonDocument<1536> configDoc;
        if (configFile) { 
            deserializeJson(configDoc, configFile); 
            configFile.close(); 
        }

        ntpManager->saveConfig(configDoc);

        configFile = LittleFS.open("/config.json", "w");
        if (!configFile) {
            request->send(500, "application/json", "{\"error\":\"Failed to save configuration\"}");
            return;
        }

        serializeJson(configDoc, configFile);
        configFile.close();
        request->send(200, "application/json", "{\"status\":\"ok\",\"message\":\"Configuration saved\"}");
    }
}

void WebServerManager::handleNTPTime(AsyncWebServerRequest *request) {
    if (!checkAuth(request)) return;

    StaticJsonDocument<256> doc;
    doc["time"] = ntpManager->getFormattedTime();
    doc["enabled"] = ntpManager->getConfig().enabled;
    doc["synced"] = ntpManager->isTimeSet();
    
    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}

// ===== LoRaWAN Handlers =====

void WebServerManager::handleLoRaWANConfigGet(AsyncWebServerRequest *request) {
    if (!checkAuth(request)) return;

    if (!lorawanManager) {
        request->send(500, "application/json", "{\"error\":\"LoRaWAN manager not initialized\"}");
        return;
    }

    // Get current configuration
    LoRaWANManager::LoRaWANConfig config = lorawanManager->getConfig();

    // Create JSON response
    StaticJsonDocument<1024> doc;

    doc["enabled"] = config.enabled;
    doc["activation_mode"] = (config.activation_mode == OTAA) ? "OTAA" : "ABP";
    doc["region"] = config.region;

    const char* deviceClass = "A";
    if (config.device_class == CLASS_B) deviceClass = "B";
    else if (config.device_class == CLASS_C) deviceClass = "C";
    doc["device_class"] = deviceClass;

    // OTAA parameters
    doc["dev_eui"] = LoRaWANManager::bytesToHex(config.dev_eui, LORAWAN_DEV_EUI_LEN);
    doc["app_eui"] = LoRaWANManager::bytesToHex(config.app_eui, LORAWAN_APP_EUI_LEN);
    doc["app_key"] = LoRaWANManager::bytesToHex(config.app_key, LORAWAN_APP_KEY_LEN);

    // ABP parameters
    doc["dev_addr"] = LoRaWANManager::bytesToHex(config.dev_addr, LORAWAN_DEV_ADDR_LEN);
    doc["nwk_s_key"] = LoRaWANManager::bytesToHex(config.nwk_s_key, LORAWAN_SESSION_KEY_LEN);
    doc["app_s_key"] = LoRaWANManager::bytesToHex(config.app_s_key, LORAWAN_SESSION_KEY_LEN);

    // Advanced settings
    doc["adr_enabled"] = config.adr_enabled;
    doc["confirmed_uplinks"] = config.confirmed_uplinks;
    doc["data_rate"] = config.data_rate;
    doc["tx_power"] = config.tx_power;
    doc["uplink_interval"] = config.uplink_interval;

    // Pin configuration
    JsonObject pins = doc.createNestedObject("pins");
    pins["miso"] = config.pins.miso;
    pins["mosi"] = config.pins.mosi;
    pins["sck"] = config.pins.sck;
    pins["nss"] = config.pins.nss;
    pins["rst"] = config.pins.rst;
    pins["dio0"] = config.pins.dio0;
    pins["dio1"] = config.pins.dio1;
    pins["dio2"] = config.pins.dio2;

    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}

void WebServerManager::handleLoRaWANConfigPost(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    if (!checkAuth(request)) return;

    if (!lorawanManager) {
        request->send(500, "application/json", "{\"error\":\"LoRaWAN manager not initialized\"}");
        return;
    }

    if (index == 0) {
        // Parse incoming JSON
        StaticJsonDocument<1024> doc;
        if (deserializeJson(doc, data, len)) {
            request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
            return;
        }

        // Create temporary doc with lorawan section for loadConfig
        StaticJsonDocument<1536> tempDoc;
        tempDoc["lorawan"] = doc;

        // Apply configuration to manager
        if (!lorawanManager->loadConfig(tempDoc)) {
            request->send(500, "application/json", "{\"error\":\"Failed to apply configuration\"}");
            return;
        }

        // Save to config.json
        MutexGuard guard(*spiffsMutex, 5000, "loraConfigSave");
        if (!guard.acquired()) {
            request->send(503, "application/json", "{\"error\":\"System busy\"}");
            return;
        }

        File configFile = LittleFS.open("/config.json", "r");
        StaticJsonDocument<2048> configDoc;
        if (configFile) {
            deserializeJson(configDoc, configFile);
            configFile.close();
        }

        lorawanManager->saveConfig(configDoc);

        configFile = LittleFS.open("/config.json", "w");
        if (!configFile) {
            request->send(500, "application/json", "{\"error\":\"Failed to save configuration\"}");
            return;
        }

        serializeJson(configDoc, configFile);
        configFile.close();
        request->send(200, "application/json", "{\"status\":\"ok\",\"message\":\"Configuration saved\"}");
    }
}

void WebServerManager::handleLoRaWANStatus(AsyncWebServerRequest *request) {
    if (!checkAuth(request)) return;

    if (!lorawanManager) {
        request->send(500, "application/json", "{\"error\":\"LoRaWAN manager not initialized\"}");
        return;
    }

    // Get current status
    LoRaWANManager::LoRaWANStatus status = lorawanManager->getStatus();

    // Create JSON response
    StaticJsonDocument<512> doc;

    doc["enabled"] = status.enabled;
    doc["joined"] = status.joined;
    doc["joining"] = status.joining;
    doc["uplink_count"] = status.uplink_count;
    doc["downlink_count"] = status.downlink_count;
    doc["last_rssi"] = status.last_rssi;
    doc["last_snr"] = status.last_snr;
    doc["data_rate"] = status.data_rate;
    doc["last_uplink_time"] = status.last_uplink_time;
    doc["message"] = status.message;

    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}

void WebServerManager::handleLoRaWANJoin(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    if (!checkAuth(request)) return;

    if (!lorawanManager) {
        request->send(500, "application/json", "{\"error\":\"LoRaWAN manager not initialized\"}");
        return;
    }

    // Start join procedure
    bool success = lorawanManager->join();

    if (success) {
        request->send(200, "application/json", "{\"message\":\"Join successful\"}");
    } else {
        request->send(500, "application/json", "{\"error\":\"Join failed\"}");
    }
}

void WebServerManager::handleLoRaWANUplink(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    if (!checkAuth(request)) return;

    if (!lorawanManager) {
        request->send(500, "application/json", "{\"error\":\"LoRaWAN manager not initialized\"}");
        return;
    }

    // Parse JSON
    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, (const char*)data, len);

    if (error) {
        request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
        return;
    }

    // Build payload from JSON
    // For now, we'll just send the JSON as a string
    String jsonStr;
    serializeJson(doc, jsonStr);

    // Convert to bytes
    uint8_t payload[LORAWAN_MAX_PAYLOAD_SIZE];
    size_t payloadLen = min((size_t)jsonStr.length(), (size_t)LORAWAN_MAX_PAYLOAD_SIZE);
    memcpy(payload, jsonStr.c_str(), payloadLen);

    // Send uplink
    bool success = lorawanManager->sendUplink(payload, payloadLen, false);

    if (success) {
        request->send(200, "application/json", "{\"message\":\"Uplink sent successfully\"}");
    } else {
        request->send(500, "application/json", "{\"error\":\"Failed to send uplink\"}");
    }
}

void WebServerManager::handleOTA(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
    if (!checkAuth(request)) return;

    // Static guards - created on first chunk, destroyed when upload ends
    static WatchdogGuard* watchdogGuard = nullptr;
    static MutexGuard* mutexGuard = nullptr;

    if (index == 0) {
        otaUploadError = "";

        // Create WatchdogGuard to disable watchdog during OTA
        if (watchdogGuard == nullptr) {
            watchdogGuard = new WatchdogGuard();
        }

        // Create MutexGuard to hold SPIFFS mutex during entire OTA upload
        if (mutexGuard == nullptr) {
            mutexGuard = new MutexGuard(*spiffsMutex, 10000, "otaUpload");
        }

        if (!mutexGuard->acquired()) {
            otaUploadError = "SPIFFS is busy";
            // Clean up guards on error
            delete watchdogGuard;
            watchdogGuard = nullptr;
            delete mutexGuard;
            mutexGuard = nullptr;
            return;
        }

        otaUploadInProgress = true;

        if (!isValidESP32Firmware(data, len)) {
            otaUploadError = "Invalid ESP32 firmware file";
            otaUploadInProgress = false;
            // Clean up guards on error
            delete watchdogGuard;
            watchdogGuard = nullptr;
            delete mutexGuard;
            mutexGuard = nullptr;
            return;
        }

        if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH)) {
            otaUploadError = "Failed to begin OTA update: " + String(Update.errorString());
            otaUploadInProgress = false;
            // Clean up guards on error
            delete watchdogGuard;
            watchdogGuard = nullptr;
            delete mutexGuard;
            mutexGuard = nullptr;
            return;
        }
    }

    if (len) {
        yield();
        if (Update.write(data, len) != len) {
            otaUploadError = "Failed to write firmware data";
            Update.abort();
            // Note: Don't delete guards here - wait for final chunk
        }
        yield();
    }

    if (final) {
        if (!Update.end(true)) {
            otaUploadError = "Failed to finalize OTA update: " + String(Update.errorString());
        }

        // Always clean up guards when upload ends (success or failure)
        // WatchdogGuard destructor will re-enable the watchdog
        // MutexGuard destructor will release the mutex
        if (watchdogGuard != nullptr) {
            delete watchdogGuard;
            watchdogGuard = nullptr;
            Serial.println("OTA upload completed - WatchdogGuard destroyed");
        }

        if (mutexGuard != nullptr) {
            delete mutexGuard;
            mutexGuard = nullptr;
            Serial.println("OTA upload completed - MutexGuard destroyed");
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
