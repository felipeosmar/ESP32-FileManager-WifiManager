/**
 * SHT20 Temperature and Humidity Sensor Driver
 * Implements ISensor interface for SHT20
 */

#ifndef SENSOR_SHT20_H
#define SENSOR_SHT20_H

#include "sensor_interface.h"

// SHT20 I2C Address
#define SHT20_I2C_ADDRESS 0x40

// SHT20 Commands
#define SHT20_TRIGGER_TEMP_MEASURE_HOLD    0xE3
#define SHT20_TRIGGER_HUMD_MEASURE_HOLD    0xE5
#define SHT20_TRIGGER_TEMP_MEASURE_NOHOLD  0xF3
#define SHT20_TRIGGER_HUMD_MEASURE_NOHOLD  0xF5
#define SHT20_WRITE_USER_REG               0xE6
#define SHT20_READ_USER_REG                0xE7
#define SHT20_SOFT_RESET                   0xFE

class SensorSHT20 : public ISensor {
public:
    SensorSHT20();
    ~SensorSHT20() override;

    // ISensor interface implementation
    bool begin(TwoWire* wire, SemaphoreHandle_t mutex) override;
    bool isAvailable() override;
    bool read() override;
    const SensorData& getData() const override;
    SensorType getType() const override { return SensorType::SHT20; }
    String getName() const override { return "SHT20"; }
    uint8_t getAddress() const override { return SHT20_I2C_ADDRESS; }
    bool reset() override;
    String getLastError() const override { return _lastError; }

private:
    bool checkSensor();
    uint16_t readValue(uint8_t command);
    float calculateTemperature(uint16_t rawValue);
    float calculateHumidity(uint16_t rawValue);
    uint8_t calculateCRC(uint8_t data[], uint8_t len);
    bool verifyCRC(uint8_t data[], uint8_t len, uint8_t checksum);
};

#endif // SENSOR_SHT20_H
