#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include "config.h"
#include "spiffs_manager.h"
#include "mqtt_manager.h"
#include "oled_manager.h"
#include "sensor_manager.h"
#include "ntp_manager.h"
#include "lorawan_manager.h"
#include "auth_manager.h"
#include "raii_guards.h"
#include <AsyncWebSocket.h>

class WebServerManager {
public:
    WebServerManager();
    void begin(SPIFFSManager* spiffs, MQTTManager* mqtt, OLEDManager* oled, SensorManager* sensor, NTPManager* ntp, LoRaWANManager* lorawan, SemaphoreHandle_t* spiffsMutex);
    void loop();
    void broadcastLog(const String& message);

private:
    AsyncWebServer server;
    AsyncWebSocket ws;
    SPIFFSManager* spiffsManager;
    MQTTManager* mqttManager;
    OLEDManager* oledManager;
    SensorManager* sensorManager;
    NTPManager* ntpManager;
    LoRaWANManager* lorawanManager;
    SemaphoreHandle_t* spiffsMutex;
    
    bool otaUploadInProgress;
    String otaUploadError;

    // Web Authentication
    String webUsername;
    String webPasswordHash;
    bool firstLogin;

    // Helper functions
    void setupRoutes();
    void serveStaticFile(AsyncWebServerRequest *request, const char* filepath, const char* contentType);
    bool checkAuth(AsyncWebServerRequest *request);
    void validateOTABoot();
    bool loadWebCredentials();
    bool saveWebCredentials(const String& username, const String& passwordHash, bool firstLogin);
    bool isValidPath(const String& path);
    
    // Route handlers
    void handleRoot(AsyncWebServerRequest *request);
    void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len);
    void handleStatus(AsyncWebServerRequest *request);
    void handleFileList(AsyncWebServerRequest *request);
    void handleFileDownload(AsyncWebServerRequest *request);
    void handleFileView(AsyncWebServerRequest *request);
    void handleFileRead(AsyncWebServerRequest *request);
    void handleFileWrite(AsyncWebServerRequest *request);
    void handleFileDelete(AsyncWebServerRequest *request);
    void handleFileCreateDir(AsyncWebServerRequest *request);
    void handleFileUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final);
    
    void handleWiFiScan(AsyncWebServerRequest *request);
    void handleWiFiConnect(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total);
    
    void handleMQTTStatus(AsyncWebServerRequest *request);
    void handleMQTTConfigGet(AsyncWebServerRequest *request);
    void handleMQTTConfigPost(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total);
    void handleMQTTTest(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total);
    void handleMQTTPublish(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total);
    
    void handleDisplayStatus(AsyncWebServerRequest *request);
    void handleDisplayConfigGet(AsyncWebServerRequest *request);
    void handleDisplayConfigPost(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total);
    void handleDisplayMode(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total);
    
    void handleSensorStatus(AsyncWebServerRequest *request);
    void handleSensorConfigGet(AsyncWebServerRequest *request);
    void handleSensorConfigPost(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total);

    void handleNTPConfigGet(AsyncWebServerRequest *request);
    void handleNTPConfigPost(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total);
    void handleNTPTime(AsyncWebServerRequest *request);

    void handleLoRaWANConfigGet(AsyncWebServerRequest *request);
    void handleLoRaWANConfigPost(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total);
    void handleLoRaWANStatus(AsyncWebServerRequest *request);
    void handleLoRaWANJoin(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total);
    void handleLoRaWANUplink(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total);

    void handleAuthStatus(AsyncWebServerRequest *request);
    void handleChangePassword(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total);

    void handleOTA(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final);
    bool isValidESP32Firmware(uint8_t *data, size_t len);
};

#endif // WEB_SERVER_H
