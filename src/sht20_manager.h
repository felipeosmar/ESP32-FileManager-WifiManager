/**
 * SHT20 Temperature and Humidity Sensor Manager
 * Manages SHT20 I2C sensor readings
 */

#ifndef SHT20_MANAGER_H
#define SHT20_MANAGER_H

#include <Arduino.h>
#include <Wire.h>
#include <ArduinoJson.h>

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

class SHT20Manager {
public:
    SHT20Manager();
    ~SHT20Manager();

    // Configuration structure
    struct SHT20Config {
        bool enabled;
        uint16_t read_interval;  // Reading interval in seconds (default: 60)
        bool fahrenheit;         // Temperature in Fahrenheit (default: false = Celsius)
    };

    // Sensor data structure
    struct SHT20Data {
        float temperature;       // Temperature in Celsius or Fahrenheit
        float humidity;          // Relative humidity in %
        bool valid;              // Data validity flag
        unsigned long timestamp; // Last reading timestamp
    };

    // Initialize sensor
    bool begin(TwoWire* wire = &Wire);

    // Load configuration from JSON
    bool loadConfig(const JsonDocument& doc);

    // Save configuration to JSON
    void saveConfig(JsonDocument& doc);

    // Update configuration
    void updateConfig(bool enabled, uint16_t interval, bool fahrenheit);

    // Get configuration
    const SHT20Config& getConfig() const { return config; }

    // Check if sensor is available
    bool isAvailable() const { return sensorAvailable; }

    // Read sensor data
    bool readSensor();

    // Get sensor data
    const SHT20Data& getData() const { return data; }

    // Get temperature in Celsius
    float getTemperature() const;

    // Get temperature in Fahrenheit
    float getTemperatureFahrenheit() const;

    // Get humidity
    float getHumidity() const;

    // Auto update (call in loop)
    void update();

    // Get last error
    String getLastError() const { return lastError; }

    // Soft reset sensor
    bool softReset();

private:
    TwoWire* wire;
    SHT20Config config;
    SHT20Data data;
    bool sensorAvailable;
    String lastError;
    unsigned long lastReadTime;

    // Low-level sensor functions
    bool checkSensor();
    uint16_t readValue(uint8_t command);
    float calculateTemperature(uint16_t rawValue);
    float calculateHumidity(uint16_t rawValue);
    uint8_t calculateCRC(uint8_t data[], uint8_t len);
    bool verifyCRC(uint8_t data[], uint8_t len, uint8_t checksum);
};

#endif // SHT20_MANAGER_H
