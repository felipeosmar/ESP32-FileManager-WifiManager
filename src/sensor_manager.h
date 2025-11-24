/**
 * Unified Sensor Manager
 * Manages multiple temperature/humidity sensors with auto-detection
 * Replaces the old SHT20Manager with multi-sensor support
 */

#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include <Arduino.h>
#include <Wire.h>
#include <ArduinoJson.h>
#include "sensor_interface.h"
#include "sensor_sht20.h"
#include "sensor_sht30.h"
#include "sensor_sht40.h"
#include "sensor_am2315.h"

class SensorManager {
public:
    SensorManager();
    ~SensorManager();

    // Configuration structure
    struct SensorConfig {
        bool enabled;
        SensorType sensorType;       // Type of sensor to use
        uint16_t read_interval;      // Reading interval in seconds (default: 60)
        bool fahrenheit;             // Temperature in Fahrenheit (default: false = Celsius)
        uint8_t customAddress;       // Custom I2C address (for SHT30)
    };

    /**
     * Initialize sensor manager
     * @param wire I2C wire interface
     * @param mutex I2C bus mutex for thread-safe access
     * @return true if initialization successful
     */
    bool begin(TwoWire* wire = &Wire, SemaphoreHandle_t mutex = NULL);

    /**
     * Load configuration from JSON
     * @param doc JSON document containing configuration
     * @return true if configuration loaded successfully
     */
    bool loadConfig(const JsonDocument& doc);

    /**
     * Save configuration to JSON
     * @param doc JSON document to save configuration to
     */
    void saveConfig(JsonDocument& doc);

    /**
     * Update configuration
     * @param enabled Enable/disable sensor
     * @param interval Reading interval in seconds
     * @param fahrenheit Use Fahrenheit instead of Celsius
     * @param sensorType Type of sensor (or AUTO for auto-detection)
     */
    void updateConfig(bool enabled, uint16_t interval, bool fahrenheit, SensorType sensorType = SensorType::AUTO);

    /**
     * Get current configuration
     * @return Reference to configuration structure
     */
    const SensorConfig& getConfig() const { return config; }

    /**
     * Check if a sensor is available
     * @return true if sensor is detected and responsive
     */
    bool isAvailable() const;

    /**
     * Read sensor data
     * @return true if reading successful
     */
    bool readSensor();

    /**
     * Get sensor data
     * @return SensorData structure with temperature, humidity, validity
     */
    const SensorData& getData() const;

    /**
     * Get temperature in configured unit (Celsius or Fahrenheit)
     * @return Temperature value
     */
    float getTemperature() const;

    /**
     * Get temperature in Fahrenheit
     * @return Temperature in Fahrenheit
     */
    float getTemperatureFahrenheit() const;

    /**
     * Get humidity
     * @return Relative humidity in %
     */
    float getHumidity() const;

    /**
     * Auto update (call in loop)
     * Automatically reads sensor at configured intervals
     */
    void update();

    /**
     * Get last error message
     * @return Error description string
     */
    const char* getLastError() const { return lastError; }

    /**
     * Get detected sensor type
     * @return SensorType enum value
     */
    SensorType getDetectedSensorType() const;

    /**
     * Get detected sensor name
     * @param buffer Buffer to store sensor name
     * @param bufferSize Size of buffer
     */
    void getDetectedSensorName(char* buffer, size_t bufferSize) const;

    /**
     * Auto-detect available sensors on I2C bus
     * @return true if at least one sensor detected
     */
    bool autoDetect();

    /**
     * Soft reset the current sensor
     * @return true if reset successful
     */
    bool softReset();

private:
    TwoWire* wire;
    SemaphoreHandle_t i2cMutex;
    SensorConfig config;
    ISensor* currentSensor;          // Currently active sensor
    SensorData dummyData;            // Dummy data for when no sensor is active
    char lastError[128];             // ✅ Buffer estático em vez de String dinâmica
    unsigned long lastReadTime;

    // Helper para definir erro com formatação
    void setError(const char* format, ...) {
        va_list args;
        va_start(args, format);
        vsnprintf(lastError, sizeof(lastError), format, args);
        va_end(args);
    }

    // Helper functions
    bool initializeSensor(SensorType type, uint8_t address = 0);
    void releaseSensor();
    ISensor* createSensor(SensorType type, uint8_t address = 0);
};

#endif // SENSOR_MANAGER_H
