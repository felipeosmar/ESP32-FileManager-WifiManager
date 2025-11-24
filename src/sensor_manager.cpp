/**
 * Unified Sensor Manager Implementation
 */

#include "sensor_manager.h"

SensorManager::SensorManager()
    : wire(nullptr),
      i2cMutex(nullptr),
      currentSensor(nullptr),
      lastReadTime(0) {

    // Default configuration
    config.enabled = true;
    config.sensorType = SensorType::AUTO;  // Auto-detect by default
    config.read_interval = 180;  // 180 seconds
    config.fahrenheit = false;
    config.customAddress = 0;

    // Initialize dummy data
    dummyData.temperature = 0.0;
    dummyData.humidity = 0.0;
    dummyData.valid = false;
    dummyData.timestamp = 0;
    dummyData.sensorType = SensorType::NONE;
    dummyData.sensorName = "None";
}

SensorManager::~SensorManager() {
    releaseSensor();
}

void SensorManager::releaseSensor() {
    if (currentSensor != nullptr) {
        delete currentSensor;
        currentSensor = nullptr;
    }
}

ISensor* SensorManager::createSensor(SensorType type, uint8_t address) {
    switch (type) {
        case SensorType::SHT20:
            return new SensorSHT20();

        case SensorType::SHT30:
            if (address == 0) address = SHT30_I2C_ADDRESS_A;
            return new SensorSHT30(address);

        case SensorType::SHT40:
            return new SensorSHT40();

        case SensorType::AM2315:
            return new SensorAM2315();

        default:
            return nullptr;
    }
}

bool SensorManager::initializeSensor(SensorType type, uint8_t address) {
    // Release any existing sensor
    releaseSensor();

    // Create new sensor instance
    currentSensor = createSensor(type, address);

    if (currentSensor == nullptr) {
        strlcpy(lastError, "Failed to create sensor instance", sizeof(lastError));
        Serial.printf("SensorManager: %s\n", lastError);
        return false;
    }

    // Initialize sensor
    if (!currentSensor->begin(wire, i2cMutex)) {
        strlcpy(lastError, currentSensor->getLastError().c_str(), sizeof(lastError));
        Serial.printf("SensorManager: Failed to initialize %s: %s\n",
                      sensorTypeToString(type).c_str(), lastError);
        releaseSensor();
        return false;
    }

    Serial.printf("SensorManager: Successfully initialized %s\n", currentSensor->getName().c_str());
    return true;
}

bool SensorManager::autoDetect() {
    if (!config.enabled) {
        Serial.println("SensorManager: Disabled in configuration");
        return false;
    }

    if (wire == nullptr) {
        strlcpy(lastError, "Wire interface is null", sizeof(lastError));
        Serial.println("SensorManager: Wire interface is null");
        return false;
    }

    Serial.println("SensorManager: Starting auto-detection...");

    // Try sensors in order of preference
    SensorType typesToTry[] = {
        SensorType::SHT40,   // Try SHT40 first (newer, better)
        SensorType::SHT30,   // Then SHT30
        SensorType::SHT20,   // Then SHT20
        SensorType::AM2315   // Finally AM2315
    };

    for (SensorType type : typesToTry) {
        Serial.printf("SensorManager: Trying %s...\n", sensorTypeToString(type).c_str());

        ISensor* testSensor = createSensor(type, config.customAddress);
        if (testSensor == nullptr) continue;

        if (testSensor->begin(wire, i2cMutex)) {
            // Sensor found and initialized
            releaseSensor();  // Release any previous sensor
            currentSensor = testSensor;
            config.sensorType = type;

            Serial.printf("SensorManager: Auto-detected %s at address 0x%02X\n",
                          currentSensor->getName().c_str(), currentSensor->getAddress());

            // Do initial reading
            readSensor();

            return true;
        } else {
            delete testSensor;
        }
    }

    // Try SHT30 with alternate address (0x45)
    Serial.println("SensorManager: Trying SHT30 at alternate address (0x45)...");
    ISensor* testSensor = new SensorSHT30(SHT30_I2C_ADDRESS_B);
    if (testSensor->begin(wire, i2cMutex)) {
        releaseSensor();
        currentSensor = testSensor;
        config.sensorType = SensorType::SHT30;
        config.customAddress = SHT30_I2C_ADDRESS_B;

        Serial.printf("SensorManager: Auto-detected SHT30 at address 0x%02X\n",
                      currentSensor->getAddress());

        readSensor();
        return true;
    } else {
        delete testSensor;
    }

    strlcpy(lastError, "No supported sensor found", sizeof(lastError));
    Serial.printf("SensorManager: %s\n", lastError);
    return false;
}

bool SensorManager::begin(TwoWire* wireInterface, SemaphoreHandle_t mutex) {
    if (!config.enabled) {
        Serial.println("SensorManager: Disabled in configuration");
        return false;
    }

    wire = wireInterface;
    i2cMutex = mutex;

    if (wire == nullptr) {
        strlcpy(lastError, "Wire interface is null", sizeof(lastError));
        Serial.println("SensorManager: Wire interface is null");
        return false;
    }

    Serial.printf("SensorManager: Starting initialization (mutex: %s)\n",
                  i2cMutex ? "enabled" : "disabled");

    // Auto-detect or use configured sensor type
    if (config.sensorType == SensorType::AUTO || config.sensorType == SensorType::NONE) {
        return autoDetect();
    } else {
        return initializeSensor(config.sensorType, config.customAddress);
    }
}

bool SensorManager::loadConfig(const JsonDocument& doc) {
    // Support both old "sht20" key and new "sensor" key for backward compatibility
    JsonObjectConst sensorConfig;

    if (doc.containsKey("sensor")) {
        sensorConfig = doc["sensor"];
    } else if (doc.containsKey("sht20")) {
        // Backward compatibility with old config
        sensorConfig = doc["sht20"];
        Serial.println("SensorManager: Loading legacy 'sht20' configuration");
    } else {
        Serial.println("SensorManager: No configuration found in JSON");
        return false;
    }

    config.enabled = sensorConfig["enabled"] | true;
    config.read_interval = sensorConfig["read_interval"] | 180;
    config.fahrenheit = sensorConfig["fahrenheit"] | false;

    // Load sensor type
    String typeStr = sensorConfig["type"] | "AUTO";
    config.sensorType = stringToSensorType(typeStr);

    // Load custom address for SHT30
    config.customAddress = sensorConfig["custom_address"] | 0;

    Serial.println("SensorManager: Configuration loaded");
    Serial.printf("  Enabled: %s\n", config.enabled ? "Yes" : "No");
    Serial.printf("  Sensor Type: %s\n", typeStr.c_str());
    Serial.printf("  Read Interval: %d seconds\n", config.read_interval);
    Serial.printf("  Unit: %s\n", config.fahrenheit ? "Fahrenheit" : "Celsius");
    if (config.customAddress > 0) {
        Serial.printf("  Custom Address: 0x%02X\n", config.customAddress);
    }

    return true;
}

void SensorManager::saveConfig(JsonDocument& doc) {
    JsonObject sensorObj = doc["sensor"].to<JsonObject>();

    sensorObj["enabled"] = config.enabled;
    sensorObj["type"] = sensorTypeToString(config.sensorType);
    sensorObj["read_interval"] = config.read_interval;
    sensorObj["fahrenheit"] = config.fahrenheit;

    if (config.customAddress > 0) {
        sensorObj["custom_address"] = config.customAddress;
    }
}

void SensorManager::updateConfig(bool enabled, uint16_t interval, bool fahrenheit, SensorType sensorType) {
    config.enabled = enabled;
    config.read_interval = interval;
    config.fahrenheit = fahrenheit;

    // If sensor type changed, reinitialize
    if (sensorType != config.sensorType && sensorType != SensorType::NONE) {
        config.sensorType = sensorType;

        if (wire != nullptr) {
            if (sensorType == SensorType::AUTO) {
                autoDetect();
            } else {
                initializeSensor(sensorType, config.customAddress);
            }
        }
    }

    Serial.println("SensorManager: Configuration updated");
}

bool SensorManager::isAvailable() const {
    if (currentSensor == nullptr) return false;
    return currentSensor->isAvailable();
}

bool SensorManager::readSensor() {
    if (!config.enabled) {
        strlcpy(lastError, "Sensor manager disabled", sizeof(lastError));
        return false;
    }

    if (currentSensor == nullptr) {
        strlcpy(lastError, "No sensor initialized", sizeof(lastError));
        return false;
    }

    return currentSensor->read();
}

const SensorData& SensorManager::getData() const {
    if (currentSensor == nullptr) {
        return dummyData;
    }
    return currentSensor->getData();
}

float SensorManager::getTemperature() const {
    const SensorData& data = getData();
    if (!data.valid) return 0.0;

    if (config.fahrenheit) {
        return data.temperature * 9.0 / 5.0 + 32.0;
    }

    return data.temperature;
}

float SensorManager::getTemperatureFahrenheit() const {
    const SensorData& data = getData();
    if (!data.valid) return 0.0;
    return data.temperature * 9.0 / 5.0 + 32.0;
}

float SensorManager::getHumidity() const {
    const SensorData& data = getData();
    if (!data.valid) return 0.0;
    return data.humidity;
}

SensorType SensorManager::getDetectedSensorType() const {
    if (currentSensor == nullptr) return SensorType::NONE;
    return currentSensor->getType();
}

void SensorManager::getDetectedSensorName(char* buffer, size_t bufferSize) const {
    if (buffer == nullptr || bufferSize == 0) return;

    if (currentSensor == nullptr) {
        strlcpy(buffer, "None", bufferSize);
    } else {
        strlcpy(buffer, currentSensor->getName().c_str(), bufferSize);
    }
}

bool SensorManager::softReset() {
    if (currentSensor == nullptr) {
        strlcpy(lastError, "No sensor initialized", sizeof(lastError));
        return false;
    }
    return currentSensor->reset();
}

void SensorManager::update() {
    if (!config.enabled || currentSensor == nullptr) return;

    unsigned long now = millis();
    unsigned long interval = config.read_interval * 1000UL;

    if (now - lastReadTime >= interval) {
        readSensor();
        lastReadTime = now;
    }
}
