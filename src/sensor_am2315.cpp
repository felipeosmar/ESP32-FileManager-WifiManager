/**
 * AM2315 Temperature and Humidity Sensor Driver Implementation
 */

#include "sensor_am2315.h"

SensorAM2315::SensorAM2315() {
    _data.temperature = 0.0;
    _data.humidity = 0.0;
    _data.valid = false;
    _data.timestamp = 0;
    _data.sensorType = SensorType::AM2315;
    _data.sensorName = "AM2315";  // const char* literal
}

SensorAM2315::~SensorAM2315() {
    // Nothing to clean up
}

bool SensorAM2315::begin(TwoWire* wire, SemaphoreHandle_t mutex) {
    _wire = wire;
    _i2cMutex = mutex;

    if (_wire == nullptr) {
        _lastError = "Wire interface is null";
        Serial.println("AM2315: Wire interface is null");
        return false;
    }

    Serial.printf("AM2315: Initializing at address 0x%02X (mutex: %s)\n",
                  AM2315_I2C_ADDRESS, mutex ? "enabled" : "disabled");

    // Check if sensor is present
    if (!checkSensor()) {
        _lastError = "Sensor not found at address 0x5C";
        Serial.printf("AM2315: %s\n", _lastError.c_str());
        return false;
    }

    Serial.println("AM2315: Initialized successfully");

    // Do initial reading
    read();

    return true;
}

bool SensorAM2315::checkSensor() {
    if (_wire == nullptr) return false;

    // AM2315 requires wake-up before communication
    return wakeUpSensor();
}

bool SensorAM2315::isAvailable() {
    return checkSensor();
}

bool SensorAM2315::reset() {
    // AM2315 doesn't have a software reset command
    // Wake up sensor is the closest equivalent
    return wakeUpSensor();
}

bool SensorAM2315::wakeUpSensor() {
    if (_wire == nullptr) return false;

    if (!acquireMutex()) {
        Serial.println("AM2315: Failed to acquire mutex for wake-up");
        return false;
    }

    // Wake up sensor by sending a dummy command
    // AM2315 goes to sleep and needs to be woken up before each read
    _wire->beginTransmission(AM2315_I2C_ADDRESS);
    _wire->endTransmission();

    releaseMutex();

    // Wait for sensor to wake up
    delay(10);

    return true;
}

bool SensorAM2315::readRegisters(uint8_t* buffer, uint8_t length) {
    if (_wire == nullptr || buffer == nullptr) return false;

    // Wake up sensor first
    wakeUpSensor();

    if (!acquireMutex()) {
        Serial.println("AM2315: Failed to acquire mutex for read");
        return false;
    }

    // Send read command: Function Code (0x03) + Start Address (0x00) + Length (0x04)
    _wire->beginTransmission(AM2315_I2C_ADDRESS);
    _wire->write(AM2315_CMD_READ_REGISTERS);  // Function code: Read registers
    _wire->write(0x00);                       // Start address: 0x00 (Humidity MSB)
    _wire->write(0x04);                       // Number of registers to read: 4 (2 humidity + 2 temp)
    uint8_t result = _wire->endTransmission();

    if (result != 0) {
        Serial.printf("AM2315: Failed to send read command, error: %d\n", result);
        releaseMutex();
        return false;
    }

    // Wait for sensor to process request
    delay(10);

    // Read response: Function Code (1) + Byte Count (1) + Data (4) + CRC (2) = 8 bytes
    size_t bytesReceived = _wire->requestFrom(AM2315_I2C_ADDRESS, (uint8_t)8);

    if (bytesReceived != 8) {
        Serial.printf("AM2315: requestFrom failed, expected 8 bytes, got %d\n", bytesReceived);
        releaseMutex();
        return false;
    }

    // Wait for data with timeout
    unsigned long timeout = millis() + 100;
    while (_wire->available() < 8 && millis() < timeout) {
        delay(1);
    }

    if (_wire->available() < 8) {
        Serial.printf("AM2315: Data not available, only %d bytes\n", _wire->available());
        releaseMutex();
        return false;
    }

    // Read all 8 bytes
    uint8_t response[8];
    for (uint8_t i = 0; i < 8; i++) {
        response[i] = _wire->read();
    }

    releaseMutex();

    // Validate response structure
    if (response[0] != AM2315_CMD_READ_REGISTERS) {
        Serial.printf("AM2315: Invalid function code: 0x%02X (expected 0x03)\n", response[0]);
        return false;
    }

    if (response[1] != 0x04) {
        Serial.printf("AM2315: Invalid byte count: %d (expected 4)\n", response[1]);
        return false;
    }

    // Extract CRC from response (last 2 bytes, LSB first)
    uint16_t receivedCRC = (response[7] << 8) | response[6];

    // Verify CRC (first 6 bytes)
    if (!verifyCRC16(response, 6, receivedCRC)) {
        Serial.printf("AM2315: CRC verification failed\n");
        return false;
    }

    // Copy data bytes (indices 2-5)
    for (uint8_t i = 0; i < 4; i++) {
        buffer[i] = response[i + 2];
    }

    return true;
}

uint16_t SensorAM2315::calculateCRC16(uint8_t* data, uint8_t len) {
    // CRC-16/MODBUS calculation
    uint16_t crc = 0xFFFF;

    for (uint8_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i];
        for (uint8_t bit = 0; bit < 8; bit++) {
            if (crc & 0x0001) {
                crc >>= 1;
                crc ^= 0xA001;  // Polynomial for CRC-16/MODBUS
            } else {
                crc >>= 1;
            }
        }
    }

    return crc;
}

bool SensorAM2315::verifyCRC16(uint8_t* data, uint8_t len, uint16_t crc) {
    return (calculateCRC16(data, len) == crc);
}

bool SensorAM2315::read() {
    if (_wire == nullptr) {
        _lastError = "Sensor not initialized";
        _data.valid = false;
        return false;
    }

    // Read 4 bytes: 2 humidity + 2 temperature
    uint8_t buffer[4];
    if (!readRegisters(buffer, 4)) {
        _lastError = "Failed to read sensor data";
        _data.valid = false;
        return false;
    }

    // Extract raw values (MSB first)
    uint16_t rawHumidity = (buffer[0] << 8) | buffer[1];
    uint16_t rawTemp = (buffer[2] << 8) | buffer[3];

    // Convert to actual values
    // Humidity: raw value / 10 (e.g., 550 = 55.0%)
    _data.humidity = rawHumidity / 10.0;

    // Temperature: Check if negative (bit 15 set), then raw value / 10
    if (rawTemp & 0x8000) {
        // Negative temperature
        rawTemp &= 0x7FFF;  // Clear sign bit
        _data.temperature = -(rawTemp / 10.0);
    } else {
        _data.temperature = rawTemp / 10.0;
    }

    // Validate ranges
    if (_data.humidity < 0.0 || _data.humidity > 100.0) {
        _lastError = "Humidity out of range";
        _data.valid = false;
        return false;
    }

    if (_data.temperature < -40.0 || _data.temperature > 125.0) {
        _lastError = "Temperature out of range";
        _data.valid = false;
        return false;
    }

    _data.valid = true;
    _data.timestamp = millis();

    Serial.printf("AM2315: T=%.2f°C, RH=%.2f%%\n", _data.temperature, _data.humidity);

    return true;
}

const SensorData& SensorAM2315::getData() const {
    return _data;
}
