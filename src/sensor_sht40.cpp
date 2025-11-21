/**
 * SHT40 Temperature and Humidity Sensor Driver Implementation
 */

#include "sensor_sht40.h"

SensorSHT40::SensorSHT40() {
    _data.temperature = 0.0;
    _data.humidity = 0.0;
    _data.valid = false;
    _data.timestamp = 0;
    _data.sensorType = SensorType::SHT40;
    _data.sensorName = "SHT40";  // const char* literal
}

SensorSHT40::~SensorSHT40() {
    // Nothing to clean up
}

bool SensorSHT40::begin(TwoWire* wire, SemaphoreHandle_t mutex) {
    _wire = wire;
    _i2cMutex = mutex;

    if (_wire == nullptr) {
        _lastError = "Wire interface is null";
        Serial.println("SHT40: Wire interface is null");
        return false;
    }

    Serial.printf("SHT40: Initializing at address 0x%02X (mutex: %s)\n",
                  SHT40_I2C_ADDRESS, mutex ? "enabled" : "disabled");

    // Check if sensor is present
    if (!checkSensor()) {
        _lastError = "Sensor not found at address 0x44";
        Serial.printf("SHT40: %s\n", _lastError.c_str());
        return false;
    }

    // Soft reset the sensor
    if (!reset()) {
        _lastError = "Soft reset failed";
        Serial.printf("SHT40: %s\n", _lastError.c_str());
        return false;
    }

    delay(10); // Wait for sensor to be ready (SHT40 is faster than SHT20/30)

    Serial.println("SHT40: Initialized successfully");

    // Do initial reading
    read();

    return true;
}

bool SensorSHT40::checkSensor() {
    if (_wire == nullptr) return false;

    if (!acquireMutex()) {
        Serial.println("SHT40: Failed to acquire mutex for sensor check");
        return false;
    }

    _wire->beginTransmission(SHT40_I2C_ADDRESS);
    uint8_t result = _wire->endTransmission();

    releaseMutex();

    return (result == 0);
}

bool SensorSHT40::isAvailable() {
    return checkSensor();
}

bool SensorSHT40::reset() {
    return sendCommand(SHT40_CMD_SOFT_RESET);
}

bool SensorSHT40::sendCommand(uint8_t command) {
    if (_wire == nullptr) return false;

    if (!acquireMutex()) {
        Serial.println("SHT40: Failed to acquire mutex for command");
        return false;
    }

    _wire->beginTransmission(SHT40_I2C_ADDRESS);
    _wire->write(command);
    uint8_t result = _wire->endTransmission();

    releaseMutex();

    return (result == 0);
}

bool SensorSHT40::readData(uint8_t* buffer, uint8_t length) {
    if (_wire == nullptr || buffer == nullptr) return false;

    if (!acquireMutex()) {
        Serial.println("SHT40: Failed to acquire mutex for read");
        return false;
    }

    size_t bytesReceived = _wire->requestFrom(SHT40_I2C_ADDRESS, length);

    if (bytesReceived != length) {
        Serial.printf("SHT40: requestFrom failed, expected %d bytes, got %d\n", length, bytesReceived);
        releaseMutex();
        return false;
    }

    // Wait for data with timeout
    unsigned long timeout = millis() + 100;
    while (_wire->available() < length && millis() < timeout) {
        delay(1);
    }

    if (_wire->available() < length) {
        Serial.printf("SHT40: Data not available, only %d bytes\n", _wire->available());
        releaseMutex();
        return false;
    }

    for (uint8_t i = 0; i < length; i++) {
        buffer[i] = _wire->read();
    }

    releaseMutex();

    return true;
}

float SensorSHT40::calculateTemperature(uint16_t rawValue) {
    // Formula from datasheet: T = -45 + 175 * (rawValue / 65535)
    return -45.0 + 175.0 * ((float)rawValue / 65535.0);
}

float SensorSHT40::calculateHumidity(uint16_t rawValue) {
    // Formula from datasheet: RH = -6 + 125 * (rawValue / 65535)
    float humidity = -6.0 + 125.0 * ((float)rawValue / 65535.0);

    // Clamp to valid range
    if (humidity < 0.0) humidity = 0.0;
    if (humidity > 100.0) humidity = 100.0;

    return humidity;
}

uint8_t SensorSHT40::calculateCRC(uint8_t data[], uint8_t len) {
    // CRC-8 formula: x^8 + x^5 + x^4 + 1 (Polynomial: 0x31)
    const uint8_t POLYNOMIAL = 0x31;
    uint8_t crc = 0xFF; // Initialization value

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

bool SensorSHT40::verifyCRC(uint8_t data[], uint8_t len, uint8_t checksum) {
    return (calculateCRC(data, len) == checksum);
}

bool SensorSHT40::read() {
    if (_wire == nullptr) {
        _lastError = "Sensor not initialized";
        _data.valid = false;
        return false;
    }

    // Send high precision measurement command
    if (!sendCommand(SHT40_CMD_MEASURE_HIGH_PRECISION)) {
        _lastError = "Failed to send measurement command";
        _data.valid = false;
        return false;
    }

    // Wait for measurement to complete (~8.2ms for high precision)
    delay(10);

    // Read 6 bytes: 2 temp + 1 CRC + 2 humidity + 1 CRC
    uint8_t buffer[6];
    if (!readData(buffer, 6)) {
        _lastError = "Failed to read sensor data";
        _data.valid = false;
        return false;
    }

    // Verify temperature CRC
    if (!verifyCRC(&buffer[0], 2, buffer[2])) {
        _lastError = "Temperature CRC verification failed";
        Serial.printf("SHT40: Temp CRC failed (MSB=0x%02X, LSB=0x%02X, CRC=0x%02X)\n",
                      buffer[0], buffer[1], buffer[2]);
        _data.valid = false;
        return false;
    }

    // Verify humidity CRC
    if (!verifyCRC(&buffer[3], 2, buffer[5])) {
        _lastError = "Humidity CRC verification failed";
        Serial.printf("SHT40: Humidity CRC failed (MSB=0x%02X, LSB=0x%02X, CRC=0x%02X)\n",
                      buffer[3], buffer[4], buffer[5]);
        _data.valid = false;
        return false;
    }

    // Extract raw values
    uint16_t rawTemp = (buffer[0] << 8) | buffer[1];
    uint16_t rawHumidity = (buffer[3] << 8) | buffer[4];

    // Calculate values
    _data.temperature = calculateTemperature(rawTemp);
    _data.humidity = calculateHumidity(rawHumidity);
    _data.valid = true;
    _data.timestamp = millis();

    Serial.printf("SHT40: T=%.2f°C, RH=%.2f%%\n", _data.temperature, _data.humidity);

    return true;
}

const SensorData& SensorSHT40::getData() const {
    return _data;
}
