/**
 * SHT40 Temperature and Humidity Sensor Driver
 * Implements ISensor interface for SHT40
 */

#ifndef SENSOR_SHT40_H
#define SENSOR_SHT40_H

#include "sensor_interface.h"

// SHT40 I2C Address
#define SHT40_I2C_ADDRESS 0x44

// SHT40 Commands (simplified compared to SHT30)
#define SHT40_CMD_MEASURE_HIGH_PRECISION   0xFD  // High precision (~8.2ms)
#define SHT40_CMD_MEASURE_MEDIUM_PRECISION 0xF6  // Medium precision (~4.5ms)
#define SHT40_CMD_MEASURE_LOW_PRECISION    0xE0  // Low precision (~1.7ms)
#define SHT40_CMD_READ_SERIAL              0x89  // Read serial number
#define SHT40_CMD_SOFT_RESET               0x94  // Soft reset
#define SHT40_CMD_HEATER_200MW_1S          0x39  // Activate heater 200mW for 1s
#define SHT40_CMD_HEATER_200MW_0_1S        0x32  // Activate heater 200mW for 0.1s
#define SHT40_CMD_HEATER_110MW_1S          0x2F  // Activate heater 110mW for 1s
#define SHT40_CMD_HEATER_110MW_0_1S        0x24  // Activate heater 110mW for 0.1s
#define SHT40_CMD_HEATER_20MW_1S           0x1E  // Activate heater 20mW for 1s
#define SHT40_CMD_HEATER_20MW_0_1S         0x15  // Activate heater 20mW for 0.1s

class SensorSHT40 : public ISensor {
public:
    SensorSHT40();
    ~SensorSHT40() override;

    // ISensor interface implementation
    bool begin(TwoWire* wire, SemaphoreHandle_t mutex) override;
    bool isAvailable() override;
    bool read() override;
    const SensorData& getData() const override;
    SensorType getType() const override { return SensorType::SHT40; }
    String getName() const override { return "SHT40"; }
    uint8_t getAddress() const override { return SHT40_I2C_ADDRESS; }
    bool reset() override;
    String getLastError() const override { return _lastError; }

private:
    bool checkSensor();
    bool sendCommand(uint8_t command);
    bool readData(uint8_t* buffer, uint8_t length);
    float calculateTemperature(uint16_t rawValue);
    float calculateHumidity(uint16_t rawValue);
    uint8_t calculateCRC(uint8_t data[], uint8_t len);
    bool verifyCRC(uint8_t data[], uint8_t len, uint8_t checksum);
};

#endif // SENSOR_SHT40_H
