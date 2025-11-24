/**
 * LoRaWAN Manager
 * Handles LoRaWAN communication with SX1276 chip
 * Compatible with ChirpStack network server
 */

#ifndef LORAWAN_MANAGER_H
#define LORAWAN_MANAGER_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <SPI.h>
#include <RadioLib.h>

// LoRaWAN Configuration Limits
#define LORAWAN_DEV_EUI_LEN 8
#define LORAWAN_APP_EUI_LEN 8
#define LORAWAN_APP_KEY_LEN 16
#define LORAWAN_DEV_ADDR_LEN 4
#define LORAWAN_SESSION_KEY_LEN 16
#define LORAWAN_REGION_MAX_LEN 16
#define LORAWAN_MAX_PAYLOAD_SIZE 242

// Activation modes
enum ActivationMode {
    OTAA = 0,  // Over-The-Air Activation
    ABP = 1    // Activation By Personalization
};

// Device classes
enum DeviceClass {
    CLASS_A = 0,  // Low power, bi-directional communication
    CLASS_B = 1,  // Scheduled receive windows
    CLASS_C = 2   // Continuous receive windows
};

// Note: LoRaWAN regions are handled as strings in configuration
// RadioLib provides LoRaWANBand_t constants: US915, EU868, AS923, etc.

class LoRaWANManager {
public:
    LoRaWANManager();
    ~LoRaWANManager();

    // Pin configuration for SX1276
    struct PinConfig {
        uint8_t miso;
        uint8_t mosi;
        uint8_t sck;
        uint8_t nss;   // Chip select
        uint8_t rst;   // Reset
        uint8_t dio0;  // DIO0 interrupt pin
        uint8_t dio1;  // DIO1 interrupt pin
        uint8_t dio2;  // DIO2 interrupt pin
    };

    // LoRaWAN Configuration
    struct LoRaWANConfig {
        bool enabled;
        ActivationMode activation_mode;
        char region[LORAWAN_REGION_MAX_LEN];
        DeviceClass device_class;

        // OTAA parameters
        uint8_t dev_eui[LORAWAN_DEV_EUI_LEN];
        uint8_t app_eui[LORAWAN_APP_EUI_LEN];
        uint8_t app_key[LORAWAN_APP_KEY_LEN];

        // ABP parameters
        uint8_t dev_addr[LORAWAN_DEV_ADDR_LEN];
        uint8_t nwk_s_key[LORAWAN_SESSION_KEY_LEN];
        uint8_t app_s_key[LORAWAN_SESSION_KEY_LEN];

        // Advanced settings
        bool adr_enabled;            // Adaptive Data Rate
        bool confirmed_uplinks;      // Request confirmation for uplinks
        uint8_t data_rate;           // Data rate (0-5 for most regions)
        int8_t tx_power;             // TX power in dBm
        uint32_t uplink_interval;    // Uplink interval in seconds

        // Pin configuration
        PinConfig pins;
    };

    // Status information
    struct LoRaWANStatus {
        bool enabled;
        bool joined;           // Network joined status
        bool joining;          // Join in progress
        uint32_t uplink_count; // Number of uplinks sent
        uint32_t downlink_count;  // Number of downlinks received
        int16_t last_rssi;     // Last received RSSI
        int8_t last_snr;       // Last received SNR
        uint8_t data_rate;     // Current data rate
        uint32_t last_uplink_time;  // Last uplink timestamp
        char message[128];     // Status message
    };

    // Initialize LoRaWAN
    bool begin();

    // Load configuration from JSON
    bool loadConfig(const JsonDocument& doc);

    // Save configuration to JSON
    void saveConfig(JsonDocument& doc);

    // Get current configuration
    LoRaWANConfig getConfig() const { return config; }

    // Set configuration
    void setConfig(const LoRaWANConfig& newConfig);

    // Get current status
    LoRaWANStatus getStatus() const { return status; }

    // Start/stop LoRaWAN
    bool start();
    void stop();

    // Join network (OTAA)
    bool join();

    // Send uplink data
    bool sendUplink(const uint8_t* payload, size_t length, bool confirmed = false);

    // Check if data is available for downlink
    bool hasDownlink();

    // Read downlink data
    size_t readDownlink(uint8_t* buffer, size_t maxLength);

    // Update loop - must be called regularly
    void update();

    // Reset device
    void reset();

    // Get hexadecimal string from byte array
    static String bytesToHex(const uint8_t* bytes, size_t length);

    // Convert hex string to byte array
    static bool hexToBytes(const char* hex, uint8_t* bytes, size_t length);

    // Convert 8-byte array to uint64_t (little-endian)
    static uint64_t arrayToUint64(const uint8_t* array);

private:
    LoRaWANConfig config;
    LoRaWANStatus status;
    SPIClass* spi;
    bool initialized;
    unsigned long lastUplinkTime;

    // RadioLib objects
    SX1276* radio;
    LoRaWANNode* node;
    const LoRaWANBand_t* band;

    // Internal functions
    bool initializeSPI();
    bool initializeRadio();
    bool configureRegion();
    bool performOTAAJoin();
    bool performABPJoin();
    void processEvents();
    void handleJoinAccept();
    void handleDownlink();
    void updateDataRate();

    // Utility functions
    uint32_t getFrequency() const;
    uint8_t getSpreadingFactor() const;
    uint32_t getBandwidth() const;
    const LoRaWANBand_t* getBandFromRegion() const;

    // Encryption/decryption helpers
    void encryptPayload(uint8_t* payload, size_t length);
    void decryptPayload(uint8_t* payload, size_t length);
    void calculateMIC(const uint8_t* data, size_t length, uint8_t* mic);
};

#endif // LORAWAN_MANAGER_H
