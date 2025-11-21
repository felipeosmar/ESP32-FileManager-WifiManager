/**
 * SHT30/SHT31 Temperature and Humidity Sensor Driver Implementation
 */

#include "sensor_sht30.h"

SensorSHT30::SensorSHT30(uint8_t address) : _address(address) {
    _data.temperature = 0.0;
    _data.humidity = 0.0;
    _data.valid = false;
    _data.timestamp = 0;
    _data.sensorType = SensorType::SHT30;
    _data.sensorName = "SHT30";  // const char* literal
}

SensorSHT30::~SensorSHT30() {
    // Nothing to clean up
}

bool SensorSHT30::begin(TwoWire* wire, SemaphoreHandle_t mutex) {
    _wire = wire;
    _i2cMutex = mutex;

    if (_wire == nullptr) {
        _lastError = "Wire interface is null";
        Serial.println("SHT30: Wire interface is null");
        return false;
    }

    Serial.printf("SHT30: Initializing at address 0x%02X (mutex: %s)\n",
                  _address, mutex ? "enabled" : "disabled");

    // Check if sensor is present
    if (!checkSensor()) {
        _lastError = "Sensor not found at address 0x" + String(_address, HEX);
        Serial.printf("SHT30: %s\n", _lastError.c_str());
        return false;
    }

    // Soft reset the sensor
    if (!reset()) {
        _lastError = "Soft reset failed";
        Serial.printf("SHT30: %s\n", _lastError.c_str());
        return false;
    }

    delay(50); // Wait for sensor to be ready

    Serial.println("SHT30: Initialized successfully");

    // Do initial reading
    read();

    return true;
}

bool SensorSHT30::checkSensor() {
    if (_wire == nullptr) return false;

    if (!acquireMutex()) {
        Serial.println("SHT30: Failed to acquire mutex for sensor check");
        return false;
    }

    _wire->beginTransmission(_address);
    uint8_t result = _wire->endTransmission();

    releaseMutex();

    return (result == 0);
}

bool SensorSHT30::isAvailable() {
    return checkSensor();
}

bool SensorSHT30::reset() {
    return sendCommand(SHT30_CMD_SOFT_RESET);
}

bool SensorSHT30::sendCommand(uint16_t command) {
    if (_wire == nullptr) return false;

    if (!acquireMutex()) {
        Serial.println("SHT30: Failed to acquire mutex for command");
        return false;
    }

    _wire->beginTransmission(_address);
    _wire->write(command >> 8);   // MSB
    _wire->write(command & 0xFF); // LSB
    uint8_t result = _wire->endTransmission();

    releaseMutex();

    return (result == 0);
}

bool SensorSHT30::readData(uint8_t* buffer, uint8_t length) {
    if (_wire == nullptr || buffer == nullptr) return false;

    if (!acquireMutex()) {
        Serial.println("SHT30: Failed to acquire mutex for read");
        return false;
    }

    size_t bytesReceived = _wire->requestFrom(_address, length);

    if (bytesReceived != length) {
        Serial.printf("SHT30: requestFrom failed, expected %d bytes, got %d\n", length, bytesReceived);
        releaseMutex();
        return false;
    }

    // Wait for data with timeout
    unsigned long timeout = millis() + 100;
    while (_wire->available() < length && millis() < timeout) {
        delay(1);
    }

    if (_wire->available() < length) {
        Serial.printf("SHT30: Data not available, only %d bytes\n", _wire->available());
        releaseMutex();
        return false;
    }

    for (uint8_t i = 0; i < length; i++) {
        buffer[i] = _wire->read();
    }

    releaseMutex();

    return true;
}

float SensorSHT30::calculateTemperature(uint16_t rawValue) {
    // Formula from datasheet: T = -45 + 175 * (ST / 2^16 - 1)
    return -45.0 + 175.0 * ((float)rawValue / 65535.0);
}

float SensorSHT30::calculateHumidity(uint16_t rawValue) {
    // Formula from datasheet: RH = 100 * (SRH / 2^16 - 1)
    float humidity = 100.0 * ((float)rawValue / 65535.0);

    // Clamp to valid range
    if (humidity < 0.0) humidity = 0.0;
    if (humidity > 100.0) humidity = 100.0;

    return humidity;
}

uint8_t SensorSHT30::calculateCRC(uint8_t data[], uint8_t len) {
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

bool SensorSHT30::verifyCRC(uint8_t data[], uint8_t len, uint8_t checksum) {
    return (calculateCRC(data, len) == checksum);
}

bool SensorSHT30::read() {
    if (_wire == nullptr) {
        _lastError = "Sensor not initialized";
        _data.valid = false;
        return false;
    }

    // Send high repeatability measurement command (no clock stretching)
    if (!sendCommand(SHT30_CMD_MEASURE_HIGH_REP_NOSTRETCH)) {
        _lastError = "Failed to send measurement command";
        _data.valid = false;
        return false;
    }

    // Wait for measurement to complete (typical: 15ms for high repeatability)
    delay(20);

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
        Serial.printf("SHT30: Temp CRC failed (MSB=0x%02X, LSB=0x%02X, CRC=0x%02X)\n",
                      buffer[0], buffer[1], buffer[2]);
        _data.valid = false;
        return false;
    }

    // Verify humidity CRC
    if (!verifyCRC(&buffer[3], 2, buffer[5])) {
        _lastError = "Humidity CRC verification failed";
        Serial.printf("SHT30: Humidity CRC failed (MSB=0x%02X, LSB=0x%02X, CRC=0x%02X)\n",
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

    Serial.printf("SHT30: T=%.2f°C, RH=%.2f%%\n", _data.temperature, _data.humidity);

    return true;
}

const SensorData& SensorSHT30::getData() const {
    return _data;
}
