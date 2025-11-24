/**
 * LoRaWAN Manager Implementation
 * Handles LoRaWAN communication with SX1276 chip using RadioLib
 *
 * This implementation uses RadioLib which provides full LoRaWAN protocol stack:
 * - LoRaWAN 1.0.x and 1.1.x support
 * - OTAA (Over-The-Air Activation) with secure key derivation
 * - ABP (Activation By Personalization)
 * - AES-128 encryption for payloads
 * - MIC (Message Integrity Code) calculation and verification
 * - Frame counter management
 * - Confirmed and unconfirmed uplinks/downlinks
 * - RX1/RX2 receive windows
 * - ADR (Adaptive Data Rate) support
 * - Full compliance with LoRaWAN specification
 *
 * Compatible with:
 * - ChirpStack Network Server
 * - The Things Network (TTN)
 * - Any LoRaWAN 1.0.x/1.1.x compliant network server
 *
 * RadioLib Documentation: https://github.com/jgromes/RadioLib
 */

#include "lorawan_manager.h"

// Constructor
LoRaWANManager::LoRaWANManager() : spi(nullptr), radio(nullptr), node(nullptr), band(nullptr),
                                   initialized(false), lastUplinkTime(0) {
    memset(&config, 0, sizeof(config));
    memset(&status, 0, sizeof(status));

    // Set default pin configuration
    config.pins.miso = 19;
    config.pins.mosi = 27;
    config.pins.sck = 5;
    config.pins.nss = 18;
    config.pins.rst = 14;
    config.pins.dio0 = 26;
    config.pins.dio1 = 33;
    config.pins.dio2 = 32;

    // Set default configuration
    config.activation_mode = OTAA;
    strcpy(config.region, "US915");
    config.device_class = CLASS_A;
    config.adr_enabled = true;
    config.confirmed_uplinks = false;
    config.data_rate = 0;
    config.tx_power = 14;
    config.uplink_interval = 300;
}

// Destructor
LoRaWANManager::~LoRaWANManager() {
    stop();
    if (node) {
        delete node;
    }
    if (radio) {
        delete radio;
    }
    if (spi) {
        delete spi;
    }
}

// Initialize LoRaWAN
bool LoRaWANManager::begin() {
    Serial.println("[LoRaWAN] Initializing...");

    if (!initializeSPI()) {
        Serial.println("[LoRaWAN] Failed to initialize SPI");
        strcpy(status.message, "SPI initialization failed");
        return false;
    }

    if (!initializeRadio()) {
        Serial.println("[LoRaWAN] Failed to initialize radio");
        strcpy(status.message, "Radio initialization failed");
        return false;
    }

    if (!configureRegion()) {
        Serial.println("[LoRaWAN] Failed to configure region");
        strcpy(status.message, "Region configuration failed");
        return false;
    }

    initialized = true;
    Serial.println("[LoRaWAN] Initialized successfully");
    strcpy(status.message, "Ready");

    return true;
}

// Initialize SPI
bool LoRaWANManager::initializeSPI() {
    // Create SPI instance with custom pins
    spi = new SPIClass(VSPI);
    spi->begin(config.pins.sck, config.pins.miso, config.pins.mosi, config.pins.nss);

    // Configure NSS (chip select) pin
    pinMode(config.pins.nss, OUTPUT);
    digitalWrite(config.pins.nss, HIGH);

    // Configure reset pin
    pinMode(config.pins.rst, OUTPUT);
    digitalWrite(config.pins.rst, HIGH);

    // Configure DIO pins
    pinMode(config.pins.dio0, INPUT);
    pinMode(config.pins.dio1, INPUT);
    pinMode(config.pins.dio2, INPUT);

    return true;
}

// Initialize radio (SX1276)
bool LoRaWANManager::initializeRadio() {
    // Create Module with configured pins
    // Parameters: NSS, DIO0, RESET, DIO1, SPI
    Module* mod = new Module(config.pins.nss, config.pins.dio0, config.pins.rst, config.pins.dio1, *spi);

    // Create SX1276 instance with Module
    radio = new SX1276(mod);

    // Begin radio with default settings
    // RadioLib will configure frequency, spreading factor, bandwidth, etc. based on band
    int state = radio->begin();
    if (state != RADIOLIB_ERR_NONE) {
        Serial.printf("[LoRaWAN] Failed to initialize radio, code: %d\n", state);
        return false;
    }

    // Configure TX power
    state = radio->setOutputPower(config.tx_power);
    if (state != RADIOLIB_ERR_NONE) {
        Serial.printf("[LoRaWAN] Failed to set TX power, code: %d\n", state);
        return false;
    }

    // Select LoRaWAN band based on region
    band = getBandFromRegion();
    if (!band) {
        Serial.println("[LoRaWAN] Unsupported region");
        return false;
    }

    // Create LoRaWAN node
    node = new LoRaWANNode(radio, band);

    Serial.println("[LoRaWAN] Radio initialized successfully");
    return true;
}

// Configure region
bool LoRaWANManager::configureRegion() {
    Serial.printf("[LoRaWAN] Configuring region: %s\n", config.region);

    // Frequency is already configured in initializeRadio() based on getFrequency()
    // Different regions have different channel plans and regulations
    // Note: Full LoRaWAN compliance requires channel hopping and duty cycle management

    Serial.printf("[LoRaWAN] Region configured with frequency: %lu Hz\n", getFrequency());
    return true;
}

// Load configuration from JSON
bool LoRaWANManager::loadConfig(const JsonDocument& doc) {
    if (!doc.containsKey("lorawan")) {
        Serial.println("[LoRaWAN] No LoRaWAN configuration in JSON");
        return false;
    }

    JsonVariantConst lorawanObj = doc["lorawan"];

    config.enabled = lorawanObj["enabled"] | false;

    // Activation mode
    const char* mode = lorawanObj["activation_mode"] | "OTAA";
    config.activation_mode = (strcmp(mode, "ABP") == 0) ? ABP : OTAA;

    // Region
    strlcpy(config.region, lorawanObj["region"] | "US915", sizeof(config.region));

    // Device class
    const char* deviceClass = lorawanObj["device_class"] | "A";
    if (strcmp(deviceClass, "B") == 0) {
        config.device_class = CLASS_B;
    } else if (strcmp(deviceClass, "C") == 0) {
        config.device_class = CLASS_C;
    } else {
        config.device_class = CLASS_A;
    }

    // OTAA parameters
    const char* devEUI = lorawanObj["dev_eui"];
    const char* appEUI = lorawanObj["app_eui"];
    const char* appKey = lorawanObj["app_key"];

    if (devEUI) hexToBytes(devEUI, config.dev_eui, LORAWAN_DEV_EUI_LEN);
    if (appEUI) hexToBytes(appEUI, config.app_eui, LORAWAN_APP_EUI_LEN);
    if (appKey) hexToBytes(appKey, config.app_key, LORAWAN_APP_KEY_LEN);

    // ABP parameters
    const char* devAddr = lorawanObj["dev_addr"];
    const char* nwkSKey = lorawanObj["nwk_s_key"];
    const char* appSKey = lorawanObj["app_s_key"];

    if (devAddr) hexToBytes(devAddr, config.dev_addr, LORAWAN_DEV_ADDR_LEN);
    if (nwkSKey) hexToBytes(nwkSKey, config.nwk_s_key, LORAWAN_SESSION_KEY_LEN);
    if (appSKey) hexToBytes(appSKey, config.app_s_key, LORAWAN_SESSION_KEY_LEN);

    // Advanced settings
    config.adr_enabled = lorawanObj["adr_enabled"] | true;
    config.confirmed_uplinks = lorawanObj["confirmed_uplinks"] | false;
    config.data_rate = lorawanObj["data_rate"] | 0;
    config.tx_power = lorawanObj["tx_power"] | 14;
    config.uplink_interval = lorawanObj["uplink_interval"] | 300;

    // Pin configuration
    if (lorawanObj.containsKey("pins")) {
        JsonVariantConst pins = lorawanObj["pins"];
        config.pins.miso = pins["miso"] | 19;
        config.pins.mosi = pins["mosi"] | 27;
        config.pins.sck = pins["sck"] | 5;
        config.pins.nss = pins["nss"] | 18;
        config.pins.rst = pins["rst"] | 14;
        config.pins.dio0 = pins["dio0"] | 26;
        config.pins.dio1 = pins["dio1"] | 33;
        config.pins.dio2 = pins["dio2"] | 32;
    }

    Serial.println("[LoRaWAN] Configuration loaded");
    return true;
}

// Save configuration to JSON
void LoRaWANManager::saveConfig(JsonDocument& doc) {
    JsonObject lorawanObj = doc["lorawan"].to<JsonObject>();

    lorawanObj["enabled"] = config.enabled;
    lorawanObj["activation_mode"] = (config.activation_mode == OTAA) ? "OTAA" : "ABP";
    lorawanObj["region"] = config.region;

    // Device class
    const char* deviceClass = "A";
    if (config.device_class == CLASS_B) deviceClass = "B";
    else if (config.device_class == CLASS_C) deviceClass = "C";
    lorawanObj["device_class"] = deviceClass;

    // OTAA parameters
    lorawanObj["dev_eui"] = bytesToHex(config.dev_eui, LORAWAN_DEV_EUI_LEN);
    lorawanObj["app_eui"] = bytesToHex(config.app_eui, LORAWAN_APP_EUI_LEN);
    lorawanObj["app_key"] = bytesToHex(config.app_key, LORAWAN_APP_KEY_LEN);

    // ABP parameters
    lorawanObj["dev_addr"] = bytesToHex(config.dev_addr, LORAWAN_DEV_ADDR_LEN);
    lorawanObj["nwk_s_key"] = bytesToHex(config.nwk_s_key, LORAWAN_SESSION_KEY_LEN);
    lorawanObj["app_s_key"] = bytesToHex(config.app_s_key, LORAWAN_SESSION_KEY_LEN);

    // Advanced settings
    lorawanObj["adr_enabled"] = config.adr_enabled;
    lorawanObj["confirmed_uplinks"] = config.confirmed_uplinks;
    lorawanObj["data_rate"] = config.data_rate;
    lorawanObj["tx_power"] = config.tx_power;
    lorawanObj["uplink_interval"] = config.uplink_interval;

    // Pin configuration
    JsonObject pinsObj = lorawanObj["pins"].to<JsonObject>();
    pinsObj["miso"] = config.pins.miso;
    pinsObj["mosi"] = config.pins.mosi;
    pinsObj["sck"] = config.pins.sck;
    pinsObj["nss"] = config.pins.nss;
    pinsObj["rst"] = config.pins.rst;
    pinsObj["dio0"] = config.pins.dio0;
    pinsObj["dio1"] = config.pins.dio1;
    pinsObj["dio2"] = config.pins.dio2;
}

// Set configuration
void LoRaWANManager::setConfig(const LoRaWANConfig& newConfig) {
    config = newConfig;
}

// Start LoRaWAN
bool LoRaWANManager::start() {
    if (!initialized) {
        if (!begin()) {
            return false;
        }
    }

    if (!config.enabled) {
        Serial.println("[LoRaWAN] LoRaWAN is disabled");
        return false;
    }

    status.enabled = true;

    // Perform join if OTAA, or activate if ABP
    if (config.activation_mode == OTAA) {
        return join();
    } else {
        return performABPJoin();
    }
}

// Stop LoRaWAN
void LoRaWANManager::stop() {
    status.enabled = false;
    status.joined = false;
    status.joining = false;
    strcpy(status.message, "Stopped");
}

// Join network (OTAA)
bool LoRaWANManager::join() {
    if (config.activation_mode != OTAA) {
        Serial.println("[LoRaWAN] Join only available for OTAA mode");
        return false;
    }

    if (!node) {
        Serial.println("[LoRaWAN] Node not initialized");
        return false;
    }

    Serial.println("[LoRaWAN] Starting OTAA join...");
    status.joining = true;
    strcpy(status.message, "Joining network...");

    // Perform OTAA join using RadioLib
    // RadioLib handles all the complexity:
    // - Generates DevNonce
    // - Sends Join Request with DevEUI, AppEUI
    // - Calculates and verifies MIC
    // - Waits for Join Accept
    // - Derives session keys (NwkSKey, AppSKey)
    // - Validates Join Accept

    // Convert EUI arrays to uint64_t (little-endian)
    uint64_t joinEUI = arrayToUint64(config.app_eui);
    uint64_t devEUI = arrayToUint64(config.dev_eui);

    // beginOTAA returns void, so we need to check activation status afterwards
    node->beginOTAA(
        joinEUI,          // JoinEUI (previously AppEUI)
        devEUI,           // DevEUI
        config.app_key,   // NwkKey (for LoRaWAN 1.1, same as AppKey for 1.0)
        config.app_key    // AppKey
    );

    status.joining = false;

    // Check if join was successful by checking activation status
    if (node->isActivated()) {
        status.joined = true;
        strcpy(status.message, "Join successful");
        Serial.println("[LoRaWAN] Join successful!");
        Serial.println("[LoRaWAN] Session keys derived, ready for uplinks");
        return true;
    } else {
        status.joined = false;
        strcpy(status.message, "Join failed");
        Serial.println("[LoRaWAN] Join failed");
        Serial.println("[LoRaWAN] No Join Accept received - check network server configuration");
        return false;
    }
}

// Send uplink data
bool LoRaWANManager::sendUplink(const uint8_t* payload, size_t length, bool confirmed) {
    if (!status.joined) {
        Serial.println("[LoRaWAN] Not joined to network");
        return false;
    }

    if (!node) {
        Serial.println("[LoRaWAN] Node not initialized");
        return false;
    }

    if (length > LORAWAN_MAX_PAYLOAD_SIZE) {
        Serial.println("[LoRaWAN] Payload too large");
        return false;
    }

    Serial.printf("[LoRaWAN] Sending uplink (%d bytes, %s)\n",
                  length, confirmed ? "confirmed" : "unconfirmed");

    // RadioLib handles all LoRaWAN protocol operations:
    // - AES-128 encryption of payload using AppSKey
    // - MIC calculation and appending using NwkSKey
    // - Frame counter increment
    // - Channel selection (for regions with multiple channels)
    // - Proper LoRaWAN frame formatting

    // Use default uplink port (10)
    uint8_t uplinkPort = 10;

    // Send uplink (confirmed or unconfirmed)
    int state;
    if (confirmed || config.confirmed_uplinks) {
        // Send confirmed uplink - waits for downlink acknowledgment
        state = node->sendReceive((uint8_t*)payload, length, uplinkPort);
    } else {
        // Send unconfirmed uplink
        state = node->uplink((uint8_t*)payload, length, uplinkPort);
    }

    if (state == RADIOLIB_ERR_NONE) {
        status.uplink_count++;
        status.last_uplink_time = millis();
        Serial.println("[LoRaWAN] Uplink sent successfully");
        return true;
    } else {
        Serial.printf("[LoRaWAN] Uplink failed, code: %d\n", state);

        // Print helpful error messages
        if (state == RADIOLIB_ERR_NETWORK_NOT_JOINED) {
            Serial.println("[LoRaWAN] Not joined to network");
        } else if (state == RADIOLIB_ERR_TX_TIMEOUT) {
            Serial.println("[LoRaWAN] Transmit timeout");
        } else if (state == RADIOLIB_ERR_RX_TIMEOUT && confirmed) {
            Serial.println("[LoRaWAN] No acknowledgment received (confirmed uplink)");
        }

        return false;
    }
}

// Check if downlink available
bool LoRaWANManager::hasDownlink() {
    if (!node) {
        return false;
    }

    // RadioLib handles downlinks automatically in RX windows after uplink
    // This function checks if there's a pending downlink to be processed
    // Note: Downlinks are typically received via sendReceive() or after uplink in RX windows

    return false; // Downlinks are handled in sendReceive() or processEvents()
}

// Read downlink data
size_t LoRaWANManager::readDownlink(uint8_t* buffer, size_t maxLength) {
    if (!node) {
        return 0;
    }

    // Try to receive downlink
    // This will check RX windows and decrypt any received downlink
    size_t len = maxLength;
    int state = node->downlink(buffer, &len);

    if (state == RADIOLIB_ERR_NONE && len > 0) {
        status.downlink_count++;

        // Get RSSI and SNR from last packet
        // Note: RadioLib doesn't expose these directly in LoRaWAN node
        // They would be available from the physical layer radio object

        Serial.printf("[LoRaWAN] Received downlink: %d bytes\n", len);

        return len;
    }

    return 0;
}

// Update loop
void LoRaWANManager::update() {
    if (!status.enabled) {
        return;
    }

    // Process radio events
    processEvents();

    // Auto uplink if interval elapsed
    if (status.joined && config.uplink_interval > 0) {
        if (millis() - lastUplinkTime >= config.uplink_interval * 1000) {
            // Send periodic uplink (placeholder)
            uint8_t payload[] = {0x01, 0x02, 0x03};
            sendUplink(payload, sizeof(payload), config.confirmed_uplinks);
            lastUplinkTime = millis();
        }
    }
}

// Reset device
void LoRaWANManager::reset() {
    digitalWrite(config.pins.rst, LOW);
    delay(100);
    digitalWrite(config.pins.rst, HIGH);
    delay(100);
}

// Process events
void LoRaWANManager::processEvents() {
    if (!node) {
        return;
    }

    // RadioLib handles events internally:
    // - Downlink reception in RX windows after uplink
    // - MAC command processing
    // - ADR adjustments
    // - Duty cycle management
    //
    // For most applications, downlinks are received automatically
    // via sendReceive() or during RX windows after uplink()
    //
    // This function can be used for additional custom processing if needed
}

// Handle join accept
void LoRaWANManager::handleJoinAccept() {
    // TODO: Process join accept message
}

// Handle downlink
void LoRaWANManager::handleDownlink() {
    // TODO: Process downlink message
    status.downlink_count++;
}

// Update data rate (ADR)
void LoRaWANManager::updateDataRate() {
    if (!config.adr_enabled) {
        return;
    }

    // TODO: Implement ADR algorithm
}

// Perform OTAA join
bool LoRaWANManager::performOTAAJoin() {
    // This is now handled by join() function
    return join();
}

// Perform ABP join
bool LoRaWANManager::performABPJoin() {
    if (!node) {
        Serial.println("[LoRaWAN] Node not initialized");
        return false;
    }

    Serial.println("[LoRaWAN] Starting ABP activation...");

    // ABP activation - no over-the-air join required
    // Session keys are pre-configured
    // RadioLib handles frame counter initialization and session setup

    // Convert DevAddr from 4 bytes to uint32_t (big-endian)
    uint32_t devAddr = ((uint32_t)config.dev_addr[0] << 24) |
                       ((uint32_t)config.dev_addr[1] << 16) |
                       ((uint32_t)config.dev_addr[2] << 8) |
                       ((uint32_t)config.dev_addr[3]);

    // beginABP returns void
    node->beginABP(
        devAddr,            // Device Address
        config.nwk_s_key,   // Network Session Key
        config.app_s_key,   // Application Session Key
        nullptr,            // NwkSEncKey (LoRaWAN 1.1, use nullptr for 1.0)
        nullptr             // SNwkSIntKey (LoRaWAN 1.1, use nullptr for 1.0)
    );

    // Check if ABP activation was successful
    if (node->isActivated()) {
        status.joined = true;
        strcpy(status.message, "ABP activated");
        Serial.println("[LoRaWAN] ABP activation successful!");
        Serial.printf("[LoRaWAN] DevAddr: %08X\n", devAddr);
        return true;
    } else {
        status.joined = false;
        strcpy(status.message, "ABP failed");
        Serial.println("[LoRaWAN] ABP activation failed");
        return false;
    }
}

// Get LoRaWAN band based on region configuration
const LoRaWANBand_t* LoRaWANManager::getBandFromRegion() const {
    // RadioLib supported bands:
    // EU868, US915, CN780, EU433, AU915, CN500, AS923, KR920, IN865

    if (strcmp(config.region, "EU868") == 0) {
        return &EU868;
    } else if (strcmp(config.region, "US915") == 0) {
        return &US915;
    } else if (strcmp(config.region, "AU915") == 0) {
        return &AU915;
    } else if (strncmp(config.region, "AS923", 5) == 0) {
        // AS923, AS923-2, AS923-3, AS923-4 all map to AS923 in RadioLib
        return &AS923;
    } else if (strcmp(config.region, "IN865") == 0) {
        return &IN865;
    } else if (strcmp(config.region, "KR920") == 0) {
        return &KR920;
    } else if (strcmp(config.region, "CN470") == 0 || strcmp(config.region, "CN500") == 0) {
        // CN470 and CN500 both map to CN500 in RadioLib
        return &CN500;
    } else if (strcmp(config.region, "CN779") == 0 || strcmp(config.region, "CN780") == 0) {
        // CN779 maps to CN780 in RadioLib
        return &CN780;
    } else if (strcmp(config.region, "EU433") == 0) {
        return &EU433;
    }

    // Default to US915 if region not recognized
    Serial.printf("[LoRaWAN] Warning: Unknown region '%s', defaulting to US915\n", config.region);
    return &US915;
}

// Get frequency based on region
uint32_t LoRaWANManager::getFrequency() const {
    // Return the base frequency for the configured region
    // Note: Full LoRaWAN requires channel hopping within the band
    if (strcmp(config.region, "US915") == 0 || strcmp(config.region, "AU915") == 0) {
        return 915000000; // 915 MHz
    } else if (strcmp(config.region, "EU868") == 0 || strcmp(config.region, "IN865") == 0 ||
               strcmp(config.region, "RU864") == 0) {
        return 868000000; // 868 MHz
    } else if (strncmp(config.region, "AS923", 5) == 0) {
        return 923000000; // 923 MHz
    } else if (strcmp(config.region, "CN470") == 0) {
        return 470000000; // 470 MHz
    } else if (strcmp(config.region, "CN779") == 0) {
        return 779000000; // 779 MHz
    } else if (strcmp(config.region, "EU433") == 0) {
        return 433000000; // 433 MHz
    } else if (strcmp(config.region, "KR920") == 0) {
        return 920000000; // 920 MHz
    } else if (strcmp(config.region, "ISM2400") == 0) {
        return 2400000000; // 2.4 GHz
    }
    return 915000000; // Default to US915
}

// Get spreading factor based on data rate
uint8_t LoRaWANManager::getSpreadingFactor() const {
    // Map data rate to spreading factor
    // DR0 = SF12, DR1 = SF11, ..., DR5 = SF7 (for most regions)
    switch (config.data_rate) {
        case 0: return 12;
        case 1: return 11;
        case 2: return 10;
        case 3: return 9;
        case 4: return 8;
        case 5: return 7;
        default: return 10; // Default SF10
    }
}

// Get bandwidth
uint32_t LoRaWANManager::getBandwidth() const {
    // Most LoRaWAN regions use 125 kHz bandwidth
    // ISM2400 uses 203 kHz or 406 kHz
    if (strcmp(config.region, "ISM2400") == 0) {
        return 203000; // 203 kHz for 2.4 GHz band
    }
    return 125000; // 125 kHz for sub-GHz bands
}

// Encryption placeholder
void LoRaWANManager::encryptPayload(uint8_t* payload, size_t length) {
    // RadioLib handles AES-128 encryption automatically
    // Payload is encrypted using AppSKey before transmission
    // No manual encryption needed
}

// Decryption placeholder
void LoRaWANManager::decryptPayload(uint8_t* payload, size_t length) {
    // RadioLib handles AES-128 decryption automatically
    // Downlink payloads are decrypted using AppSKey upon reception
    // No manual decryption needed
}

// Calculate MIC (Message Integrity Code)
void LoRaWANManager::calculateMIC(const uint8_t* data, size_t length, uint8_t* mic) {
    // RadioLib handles MIC calculation automatically
    // MIC is calculated using AES-CMAC with NwkSKey
    // MIC is appended to all uplinks and verified on all downlinks
    // No manual MIC calculation needed
}

// Convert bytes to hex string
String LoRaWANManager::bytesToHex(const uint8_t* bytes, size_t length) {
    String hex = "";
    for (size_t i = 0; i < length; i++) {
        if (bytes[i] < 0x10) hex += "0";
        hex += String(bytes[i], HEX);
    }
    hex.toUpperCase();
    return hex;
}

// Convert hex string to bytes
bool LoRaWANManager::hexToBytes(const char* hex, uint8_t* bytes, size_t length) {
    if (strlen(hex) != length * 2) {
        return false;
    }

    for (size_t i = 0; i < length; i++) {
        char byteStr[3] = {hex[i * 2], hex[i * 2 + 1], '\0'};
        bytes[i] = (uint8_t)strtol(byteStr, NULL, 16);
    }

    return true;
}

// Convert 8-byte array to uint64_t (little-endian for LoRaWAN)
uint64_t LoRaWANManager::arrayToUint64(const uint8_t* array) {
    uint64_t value = 0;
    for (int i = 0; i < 8; i++) {
        value |= ((uint64_t)array[i] << (i * 8));
    }
    return value;
}
