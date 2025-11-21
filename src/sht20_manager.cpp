/**
 * SHT20 Temperature and Humidity Sensor Manager Implementation
 */

#include "sht20_manager.h"

SHT20Manager::SHT20Manager()
    : wire(nullptr),
      i2cMutex(nullptr),
      sensorAvailable(false),
      lastReadTime(0) {

    // Default configuration
    config.enabled = true;
    config.read_interval = 180;  // 180 seconds
    config.fahrenheit = false;

    // Initialize data
    data.temperature = 0.0;
    data.humidity = 0.0;
    data.valid = false;
    data.timestamp = 0;
}

SHT20Manager::~SHT20Manager() {
    // Nothing to clean up
}

bool SHT20Manager::begin(TwoWire* wireInterface, SemaphoreHandle_t mutex) {
    if (!config.enabled) {
        Serial.println("SHT20: Disabled in configuration");
        return false;
    }

    wire = wireInterface;
    i2cMutex = mutex;

    if (wire == nullptr) {
        lastError = "Wire interface is null";
        Serial.println("SHT20: Wire interface is null");
        return false;
    }

    Serial.printf("SHT20: Starting initialization (mutex: %s)\n", i2cMutex ? "enabled" : "disabled");

    // Check if sensor is present
    if (!checkSensor()) {
        lastError = "Sensor not found at address 0x40";
        Serial.printf("SHT20: %s\n", lastError.c_str());
        sensorAvailable = false;
        return false;
    }

    // Soft reset the sensor
    if (!softReset()) {
        lastError = "Soft reset failed";
        Serial.printf("SHT20: %s\n", lastError.c_str());
        sensorAvailable = false;
        return false;
    }

    delay(50); // Wait for sensor to be ready

    sensorAvailable = true;
    Serial.println("SHT20: Initialized successfully");

    // Do initial reading
    readSensor();

    return true;
}

bool SHT20Manager::loadConfig(const JsonDocument& doc) {
    if (!doc.containsKey("sht20")) {
        Serial.println("SHT20: No configuration found in JSON");
        return false;
    }

    JsonObjectConst sht20 = doc["sht20"];

    config.enabled = sht20["enabled"] | true;
    config.read_interval = sht20["read_interval"] | 180;
    config.fahrenheit = sht20["fahrenheit"] | false;

    Serial.println("SHT20: Configuration loaded");
    Serial.printf("  Enabled: %s\n", config.enabled ? "Yes" : "No");
    Serial.printf("  Read Interval: %d seconds\n", config.read_interval);
    Serial.printf("  Unit: %s\n", config.fahrenheit ? "Fahrenheit" : "Celsius");

    return true;
}

void SHT20Manager::saveConfig(JsonDocument& doc) {
    JsonObject sht20 = doc["sht20"].to<JsonObject>();

    sht20["enabled"] = config.enabled;
    sht20["read_interval"] = config.read_interval;
    sht20["fahrenheit"] = config.fahrenheit;
}

void SHT20Manager::updateConfig(bool enabled, uint16_t interval, bool fahrenheit) {
    config.enabled = enabled;
    config.read_interval = interval;
    config.fahrenheit = fahrenheit;

    Serial.println("SHT20: Configuration updated");
}

bool SHT20Manager::checkSensor() {
    if (wire == nullptr) return false;

    wire->beginTransmission(SHT20_I2C_ADDRESS);
    return (wire->endTransmission() == 0);
}

bool SHT20Manager::softReset() {
    if (wire == nullptr) return false;

    wire->beginTransmission(SHT20_I2C_ADDRESS);
    wire->write(SHT20_SOFT_RESET);
    return (wire->endTransmission() == 0);
}

uint16_t SHT20Manager::readValue(uint8_t command) {
    if (wire == nullptr) return 0;

    // Acquire I2C bus mutex if available
    bool mutexTaken = false;
    if (i2cMutex != nullptr) {
        mutexTaken = (xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(1000)) == pdTRUE);
        if (!mutexTaken) {
            Serial.println("SHT20: Failed to acquire I2C mutex");
            return 0;
        }
    }

    uint16_t result = 0;

    // Send measurement command
    wire->beginTransmission(SHT20_I2C_ADDRESS);
    wire->write(command);
    uint8_t txResult = wire->endTransmission();

    if (txResult != 0) {
        Serial.printf("SHT20: Failed to send command 0x%02X, error: %d\n", command, txResult);
        if (mutexTaken) xSemaphoreGive(i2cMutex);
        return 0;
    }

    // Wait for measurement to complete
    // Temperature: max 85ms, Humidity: max 29ms for 12-bit resolution
    delay(100); // Extended delay for safety

    // Request data (2 bytes data + 1 byte CRC) with timeout
    size_t bytesReceived = wire->requestFrom((uint8_t)SHT20_I2C_ADDRESS, (uint8_t)3, (uint8_t)true);

    if (bytesReceived != 3) {
        Serial.printf("SHT20: requestFrom failed, expected 3 bytes, got %d\n", bytesReceived);
        if (mutexTaken) xSemaphoreGive(i2cMutex);
        return 0;
    }

    // Wait for data to be available with timeout
    unsigned long timeout = millis() + 100;
    while (wire->available() < 3 && millis() < timeout) {
        delay(1);
    }

    if (wire->available() < 3) {
        Serial.printf("SHT20: Data not available, only %d bytes\n", wire->available());
        if (mutexTaken) xSemaphoreGive(i2cMutex);
        return 0;
    }

    uint8_t msb = wire->read();
    uint8_t lsb = wire->read();
    uint8_t crc = wire->read();

    // Verify CRC
    uint8_t data[2] = {msb, lsb};
    if (!verifyCRC(data, 2, crc)) {
        Serial.printf("SHT20: CRC verification failed (MSB=0x%02X, LSB=0x%02X, CRC=0x%02X)\n", msb, lsb, crc);
        if (mutexTaken) xSemaphoreGive(i2cMutex);
        return 0;
    }

    // Combine bytes (clear status bits)
    uint16_t rawValue = (msb << 8) | lsb;
    rawValue &= 0xFFFC; // Clear status bits (last 2 bits)

    result = rawValue;

    // Release mutex
    if (mutexTaken) {
        xSemaphoreGive(i2cMutex);
    }

    return result;
}

float SHT20Manager::calculateTemperature(uint16_t rawValue) {
    // Formula from datasheet: T = -46.85 + 175.72 * (ST / 2^16)
    float temperature = -46.85 + 175.72 * ((float)rawValue / 65536.0);
    return temperature;
}

float SHT20Manager::calculateHumidity(uint16_t rawValue) {
    // Formula from datasheet: RH = -6 + 125 * (SRH / 2^16)
    float humidity = -6.0 + 125.0 * ((float)rawValue / 65536.0);

    // Clamp to valid range
    if (humidity < 0.0) humidity = 0.0;
    if (humidity > 100.0) humidity = 100.0;

    return humidity;
}

uint8_t SHT20Manager::calculateCRC(uint8_t data[], uint8_t len) {
    const uint16_t POLYNOMIAL = 0x131; // P(x) = x^8 + x^5 + x^4 + 1 = 100110001
    uint8_t crc = 0;

    for (uint8_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t bit = 8; bit > 0; --bit) {
            if (crc & 0x80) {
                crc = (crc << 1) ^ POLYNOMIAL;
            } else {
                crc = (crc << 1);
            }
        }
    }

    return crc;
}

bool SHT20Manager::verifyCRC(uint8_t data[], uint8_t len, uint8_t checksum) {
    return (calculateCRC(data, len) == checksum);
}

bool SHT20Manager::readSensor() {
    if (!sensorAvailable || wire == nullptr) {
        lastError = "Sensor not available";
        data.valid = false;
        return false;
    }

    // Read temperature
    uint16_t rawTemp = readValue(SHT20_TRIGGER_TEMP_MEASURE_NOHOLD);
    if (rawTemp == 0) {
        lastError = "Failed to read temperature";
        data.valid = false;
        return false;
    }

    // Read humidity
    uint16_t rawHumidity = readValue(SHT20_TRIGGER_HUMD_MEASURE_NOHOLD);
    if (rawHumidity == 0) {
        lastError = "Failed to read humidity";
        data.valid = false;
        return false;
    }

    // Calculate values
    data.temperature = calculateTemperature(rawTemp);
    data.humidity = calculateHumidity(rawHumidity);
    data.valid = true;
    data.timestamp = millis();

    Serial.printf("SHT20: T=%.2f°C, RH=%.2f%%\n", data.temperature, data.humidity);

    return true;
}

float SHT20Manager::getTemperature() const {
    if (!data.valid) return 0.0;

    if (config.fahrenheit) {
        return data.temperature * 9.0 / 5.0 + 32.0;
    }

    return data.temperature;
}

float SHT20Manager::getTemperatureFahrenheit() const {
    if (!data.valid) return 0.0;
    return data.temperature * 9.0 / 5.0 + 32.0;
}

float SHT20Manager::getHumidity() const {
    if (!data.valid) return 0.0;
    return data.humidity;
}

void SHT20Manager::update() {
    if (!config.enabled || !sensorAvailable) return;

    unsigned long now = millis();
    unsigned long interval = config.read_interval * 1000UL;

    if (now - lastReadTime >= interval) {
        readSensor();
        lastReadTime = now;
    }
}
