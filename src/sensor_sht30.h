/**
 * SHT30/SHT31 Temperature and Humidity Sensor Driver
 * Implements ISensor interface for SHT30/SHT31
 */

#ifndef SENSOR_SHT30_H
#define SENSOR_SHT30_H

#include "sensor_interface.h"

// SHT30 I2C Addresses (ADDR pin configuration)
#define SHT30_I2C_ADDRESS_A 0x44  // ADDR pin LOW (default)
#define SHT30_I2C_ADDRESS_B 0x45  // ADDR pin HIGH

// SHT30 Commands (MSB first, LSB second)
#define SHT30_CMD_MEASURE_HIGH_REP_STRETCH    0x2C06  // High repeatability, clock stretching
#define SHT30_CMD_MEASURE_MEDIUM_REP_STRETCH  0x2C0D  // Medium repeatability, clock stretching
#define SHT30_CMD_MEASURE_LOW_REP_STRETCH     0x2C10  // Low repeatability, clock stretching
#define SHT30_CMD_MEASURE_HIGH_REP_NOSTRETCH  0x2400  // High repeatability, no clock stretching
#define SHT30_CMD_MEASURE_MEDIUM_REP_NOSTRETCH 0x240B // Medium repeatability, no clock stretching
#define SHT30_CMD_MEASURE_LOW_REP_NOSTRETCH   0x2416  // Low repeatability, no clock stretching
#define SHT30_CMD_SOFT_RESET                  0x30A2  // Soft reset
#define SHT30_CMD_HEATER_ENABLE               0x306D  // Enable heater
#define SHT30_CMD_HEATER_DISABLE              0x3066  // Disable heater
#define SHT30_CMD_READ_STATUS                 0xF32D  // Read status register
#define SHT30_CMD_CLEAR_STATUS                0x3041  // Clear status register

class SensorSHT30 : public ISensor {
public:
    SensorSHT30(uint8_t address = SHT30_I2C_ADDRESS_A);
    ~SensorSHT30() override;

    // ISensor interface implementation
    bool begin(TwoWire* wire, SemaphoreHandle_t mutex) override;
    bool isAvailable() override;
    bool read() override;
    const SensorData& getData() const override;
    SensorType getType() const override { return SensorType::SHT30; }
    String getName() const override { return "SHT30"; }
    uint8_t getAddress() const override { return _address; }
    bool reset() override;
    String getLastError() const override { return _lastError; }

private:
    uint8_t _address;

    bool checkSensor();
    bool sendCommand(uint16_t command);
    bool readData(uint8_t* buffer, uint8_t length);
    float calculateTemperature(uint16_t rawValue);
    float calculateHumidity(uint16_t rawValue);
    uint8_t calculateCRC(uint8_t data[], uint8_t len);
    bool verifyCRC(uint8_t data[], uint8_t len, uint8_t checksum);
};

#endif // SENSOR_SHT30_H
