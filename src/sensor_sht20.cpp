/**
 * SHT20 Temperature and Humidity Sensor Driver Implementation
 */

#include "sensor_sht20.h"

SensorSHT20::SensorSHT20() {
    _data.temperature = 0.0;
    _data.humidity = 0.0;
    _data.valid = false;
    _data.timestamp = 0;
    _data.sensorType = SensorType::SHT20;
    _data.sensorName = "SHT20";  // const char* literal
}

SensorSHT20::~SensorSHT20() {
    // Nothing to clean up
}

bool SensorSHT20::begin(TwoWire* wire, SemaphoreHandle_t mutex) {
    _wire = wire;
    _i2cMutex = mutex;

    if (_wire == nullptr) {
        _lastError = "Wire interface is null";
        Serial.println("SHT20: Wire interface is null");
        return false;
    }

    Serial.printf("SHT20: Initializing at address 0x%02X (mutex: %s)\n",
                  SHT20_I2C_ADDRESS, mutex ? "enabled" : "disabled");

    // Check if sensor is present
    if (!checkSensor()) {
        _lastError = "Sensor not found at address 0x40";
        Serial.printf("SHT20: %s\n", _lastError.c_str());
        return false;
    }

    // Soft reset the sensor
    if (!reset()) {
        _lastError = "Soft reset failed";
        Serial.printf("SHT20: %s\n", _lastError.c_str());
        return false;
    }

    delay(50); // Wait for sensor to be ready

    Serial.println("SHT20: Initialized successfully");

    // Do initial reading
    read();

    return true;
}

bool SensorSHT20::checkSensor() {
    if (_wire == nullptr) return false;

    if (!acquireMutex()) {
        Serial.println("SHT20: Failed to acquire mutex for sensor check");
        return false;
    }

    _wire->beginTransmission(SHT20_I2C_ADDRESS);
    uint8_t result = _wire->endTransmission();

    releaseMutex();

    return (result == 0);
}

bool SensorSHT20::isAvailable() {
    return checkSensor();
}

bool SensorSHT20::reset() {
    if (_wire == nullptr) return false;

    if (!acquireMutex()) {
        Serial.println("SHT20: Failed to acquire mutex for reset");
        return false;
    }

    _wire->beginTransmission(SHT20_I2C_ADDRESS);
    _wire->write(SHT20_SOFT_RESET);
    uint8_t result = _wire->endTransmission();

    releaseMutex();

    return (result == 0);
}

uint16_t SensorSHT20::readValue(uint8_t command) {
    if (_wire == nullptr) return 0;

    // Acquire I2C bus mutex
    if (!acquireMutex()) {
        Serial.println("SHT20: Failed to acquire I2C mutex for read");
        return 0;
    }

    uint16_t result = 0;

    // Send measurement command
    _wire->beginTransmission(SHT20_I2C_ADDRESS);
    _wire->write(command);
    uint8_t txResult = _wire->endTransmission();

    if (txResult != 0) {
        Serial.printf("SHT20: Failed to send command 0x%02X, error: %d\n", command, txResult);
        releaseMutex();
        return 0;
    }

    // Wait for measurement to complete
    // Temperature: max 85ms, Humidity: max 29ms for 12-bit resolution
    delay(100);

    // Request data (2 bytes data + 1 byte CRC)
    size_t bytesReceived = _wire->requestFrom((uint8_t)SHT20_I2C_ADDRESS, (uint8_t)3, (uint8_t)true);

    if (bytesReceived != 3) {
        Serial.printf("SHT20: requestFrom failed, expected 3 bytes, got %d\n", bytesReceived);
        releaseMutex();
        return 0;
    }

    // Wait for data with timeout
    unsigned long timeout = millis() + 100;
    while (_wire->available() < 3 && millis() < timeout) {
        delay(1);
    }

    if (_wire->available() < 3) {
        Serial.printf("SHT20: Data not available, only %d bytes\n", _wire->available());
        releaseMutex();
        return 0;
    }

    uint8_t msb = _wire->read();
    uint8_t lsb = _wire->read();
    uint8_t crc = _wire->read();

    releaseMutex();

    // Verify CRC
    uint8_t data[2] = {msb, lsb};
    if (!verifyCRC(data, 2, crc)) {
        Serial.printf("SHT20: CRC verification failed (MSB=0x%02X, LSB=0x%02X, CRC=0x%02X)\n",
                      msb, lsb, crc);
        return 0;
    }

    // Combine bytes (clear status bits)
    uint16_t rawValue = (msb << 8) | lsb;
    rawValue &= 0xFFFC; // Clear status bits (last 2 bits)

    result = rawValue;

    return result;
}

float SensorSHT20::calculateTemperature(uint16_t rawValue) {
    // Formula from datasheet: T = -46.85 + 175.72 * (ST / 2^16)
    return -46.85 + 175.72 * ((float)rawValue / 65536.0);
}

float SensorSHT20::calculateHumidity(uint16_t rawValue) {
    // Formula from datasheet: RH = -6 + 125 * (SRH / 2^16)
    float humidity = -6.0 + 125.0 * ((float)rawValue / 65536.0);

    // Clamp to valid range
    if (humidity < 0.0) humidity = 0.0;
    if (humidity > 100.0) humidity = 100.0;

    return humidity;
}

uint8_t SensorSHT20::calculateCRC(uint8_t data[], uint8_t len) {
    const uint16_t POLYNOMIAL = 0x131; // P(x) = x^8 + x^5 + x^4 + 1
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

bool SensorSHT20::verifyCRC(uint8_t data[], uint8_t len, uint8_t checksum) {
    return (calculateCRC(data, len) == checksum);
}

bool SensorSHT20::read() {
    if (_wire == nullptr) {
        _lastError = "Sensor not initialized";
        _data.valid = false;
        return false;
    }

    // Read temperature
    uint16_t rawTemp = readValue(SHT20_TRIGGER_TEMP_MEASURE_NOHOLD);
    if (rawTemp == 0) {
        _lastError = "Failed to read temperature";
        _data.valid = false;
        return false;
    }

    // Read humidity
    uint16_t rawHumidity = readValue(SHT20_TRIGGER_HUMD_MEASURE_NOHOLD);
    if (rawHumidity == 0) {
        _lastError = "Failed to read humidity";
        _data.valid = false;
        return false;
    }

    // Calculate values
    _data.temperature = calculateTemperature(rawTemp);
    _data.humidity = calculateHumidity(rawHumidity);
    _data.valid = true;
    _data.timestamp = millis();

    Serial.printf("SHT20: T=%.2f°C, RH=%.2f%%\n", _data.temperature, _data.humidity);

    return true;
}

const SensorData& SensorSHT20::getData() const {
    return _data;
}
