/**
 * Temperature and Humidity Sensor Interface
 * Abstract base class for multi-sensor support
 */

#ifndef SENSOR_INTERFACE_H
#define SENSOR_INTERFACE_H

#include <Arduino.h>
#include <Wire.h>

// Sensor types enumeration
enum class SensorType {
    NONE = 0,
    SHT20 = 1,
    SHT30 = 2,
    SHT40 = 3,
    AM2315 = 4,
    AUTO = 255  // Auto-detection
};

// Sensor data structure (common for all sensors)
struct SensorData {
    float temperature;       // Temperature in Celsius
    float humidity;          // Relative humidity in %
    bool valid;              // Data validity flag
    unsigned long timestamp; // Last reading timestamp
    SensorType sensorType;   // Type of sensor
    const char* sensorName;  // Human-readable sensor name (pointer, not String object)
};

/**
 * Abstract sensor interface
 * All sensor drivers must implement this interface
 */
class ISensor {
public:
    virtual ~ISensor() {}

    /**
     * Initialize the sensor
     * @param wire I2C wire interface
     * @param mutex I2C bus mutex for thread-safe access
     * @return true if initialization successful
     */
    virtual bool begin(TwoWire* wire, SemaphoreHandle_t mutex) = 0;

    /**
     * Check if sensor is available/responsive
     * @return true if sensor is detected on I2C bus
     */
    virtual bool isAvailable() = 0;

    /**
     * Read sensor data (temperature and humidity)
     * @return true if reading successful
     */
    virtual bool read() = 0;

    /**
     * Get the last sensor reading
     * @return Reference to SensorData structure with temperature, humidity, validity
     */
    virtual const SensorData& getData() const = 0;

    /**
     * Get sensor type
     * @return SensorType enum value
     */
    virtual SensorType getType() const = 0;

    /**
     * Get sensor name
     * @return Human-readable sensor name
     */
    virtual String getName() const = 0;

    /**
     * Get I2C address of the sensor
     * @return I2C address (7-bit)
     */
    virtual uint8_t getAddress() const = 0;

    /**
     * Soft reset the sensor (if supported)
     * @return true if reset successful
     */
    virtual bool reset() = 0;

    /**
     * Get last error message
     * @return Error description string
     */
    virtual String getLastError() const = 0;

protected:
    TwoWire* _wire = nullptr;
    SemaphoreHandle_t _i2cMutex = nullptr;
    SensorData _data;
    String _lastError;

    /**
     * Acquire I2C mutex with timeout
     * @param timeoutMs Timeout in milliseconds
     * @return true if mutex acquired
     */
    bool acquireMutex(uint32_t timeoutMs = 1000) {
        if (_i2cMutex == nullptr) return true;
        return (xSemaphoreTake(_i2cMutex, pdMS_TO_TICKS(timeoutMs)) == pdTRUE);
    }

    /**
     * Release I2C mutex
     */
    void releaseMutex() {
        if (_i2cMutex != nullptr) {
            xSemaphoreGive(_i2cMutex);
        }
    }
};

/**
 * Helper function to convert SensorType to string
 */
inline String sensorTypeToString(SensorType type) {
    switch (type) {
        case SensorType::SHT20:  return "SHT20";
        case SensorType::SHT30:  return "SHT30";
        case SensorType::SHT40:  return "SHT40";
        case SensorType::AM2315: return "AM2315";
        case SensorType::AUTO:   return "Auto-Detect";
        default:                 return "None";
    }
}

/**
 * Helper function to convert string to SensorType
 */
inline SensorType stringToSensorType(const String& str) {
    if (str == "SHT20")  return SensorType::SHT20;
    if (str == "SHT30")  return SensorType::SHT30;
    if (str == "SHT40")  return SensorType::SHT40;
    if (str == "AM2315") return SensorType::AM2315;
    if (str == "AUTO")   return SensorType::AUTO;
    return SensorType::NONE;
}

#endif // SENSOR_INTERFACE_H
