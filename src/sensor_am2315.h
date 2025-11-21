/**
 * AM2315 Temperature and Humidity Sensor Driver
 * Implements ISensor interface for AM2315
 */

#ifndef SENSOR_AM2315_H
#define SENSOR_AM2315_H

#include "sensor_interface.h"

// AM2315 I2C Address
#define AM2315_I2C_ADDRESS 0x5C

// AM2315 Function Codes (Modbus-like protocol)
#define AM2315_CMD_READ_REGISTERS 0x03

// AM2315 Register Addresses
#define AM2315_REG_HUMIDITY_MSB   0x00
#define AM2315_REG_HUMIDITY_LSB   0x01
#define AM2315_REG_TEMP_MSB       0x02
#define AM2315_REG_TEMP_LSB       0x03

class SensorAM2315 : public ISensor {
public:
    SensorAM2315();
    ~SensorAM2315() override;

    // ISensor interface implementation
    bool begin(TwoWire* wire, SemaphoreHandle_t mutex) override;
    bool isAvailable() override;
    bool read() override;
    const SensorData& getData() const override;
    SensorType getType() const override { return SensorType::AM2315; }
    String getName() const override { return "AM2315"; }
    uint8_t getAddress() const override { return AM2315_I2C_ADDRESS; }
    bool reset() override;
    String getLastError() const override { return _lastError; }

private:
    bool checkSensor();
    bool wakeUpSensor();
    bool readRegisters(uint8_t* buffer, uint8_t length);
    uint16_t calculateCRC16(uint8_t* data, uint8_t len);
    bool verifyCRC16(uint8_t* data, uint8_t len, uint16_t crc);
};

#endif // SENSOR_AM2315_H
