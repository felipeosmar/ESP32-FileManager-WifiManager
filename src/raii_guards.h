/**
 * @file raii_guards.h
 * @brief RAII (Resource Acquisition Is Initialization) guard classes
 *
 * This file contains utility classes that use RAII pattern to automatically
 * manage resources like watchdog timers and mutexes, ensuring proper cleanup
 * even in error paths.
 */

#ifndef RAII_GUARDS_H
#define RAII_GUARDS_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <esp_task_wdt.h>

/**
 * @brief RAII guard for Watchdog Timer (WDT)
 *
 * This class disables the watchdog timer during OTA updates and automatically
 * re-enables it when the object goes out of scope, ensuring the watchdog is
 * always restored even in error paths.
 *
 * Usage:
 *   {
 *     WatchdogGuard guard;  // Watchdog disabled
 *     // ... OTA operations ...
 *   } // Watchdog automatically re-enabled here
 */
class WatchdogGuard {
private:
    bool wasDisabled;
    TaskHandle_t idleTask0;
    TaskHandle_t idleTask1;

public:
    WatchdogGuard() : wasDisabled(false) {
        // Disable watchdog for both CPU cores during OTA
        idleTask0 = xTaskGetIdleTaskHandleForCPU(0);
        idleTask1 = xTaskGetIdleTaskHandleForCPU(1);

        if (idleTask0 != NULL) {
            esp_task_wdt_delete(idleTask0);
            Serial.println("WDT: Disabled for CPU0 (OTA in progress)");
        }

        if (idleTask1 != NULL) {
            esp_task_wdt_delete(idleTask1);
            Serial.println("WDT: Disabled for CPU1 (OTA in progress)");
        }

        wasDisabled = true;
    }

    ~WatchdogGuard() {
        // Re-enable watchdog when object goes out of scope
        if (wasDisabled) {
            if (idleTask0 != NULL) {
                esp_task_wdt_add(idleTask0);
                Serial.println("WDT: Re-enabled for CPU0");
            }

            if (idleTask1 != NULL) {
                esp_task_wdt_add(idleTask1);
                Serial.println("WDT: Re-enabled for CPU1");
            }
        }
    }

    // Prevent copying (RAII should not be copied)
    WatchdogGuard(const WatchdogGuard&) = delete;
    WatchdogGuard& operator=(const WatchdogGuard&) = delete;
};

/**
 * @brief RAII guard for Mutex/Semaphore
 *
 * This class automatically acquires a mutex/semaphore and releases it when
 * the object goes out of scope, preventing deadlocks from forgotten releases.
 *
 * Usage:
 *   {
 *     MutexGuard guard(myMutex, 5000);  // Acquire with 5s timeout
 *     if (!guard.acquired()) {
 *       // Failed to acquire
 *       return;
 *     }
 *     // ... critical section ...
 *   } // Mutex automatically released here
 */
class MutexGuard {
private:
    SemaphoreHandle_t mutex;
    bool wasAcquired;
    const char* name;  // For debugging

public:
    /**
     * @brief Construct and acquire mutex
     * @param mtx Mutex handle to acquire
     * @param timeoutMs Timeout in milliseconds (default 5000ms)
     * @param debugName Optional name for debugging (default nullptr)
     */
    MutexGuard(SemaphoreHandle_t mtx, uint32_t timeoutMs = 5000, const char* debugName = nullptr)
        : mutex(mtx), wasAcquired(false), name(debugName) {

        if (mutex != nullptr) {
            wasAcquired = (xSemaphoreTake(mutex, pdMS_TO_TICKS(timeoutMs)) == pdTRUE);

            if (wasAcquired) {
                if (name) {
                    //Serial.printf("Mutex: Acquired '%s'\n", name);
                }
            } else {
                if (name) {
                    Serial.printf("Mutex: FAILED to acquire '%s' (timeout %ums)\n", name, timeoutMs);
                } else {
                    Serial.printf("Mutex: FAILED to acquire (timeout %ums)\n", timeoutMs);
                }
            }
        }
    }

    ~MutexGuard() {
        if (wasAcquired && mutex != nullptr) {
            xSemaphoreGive(mutex);
            if (name) {
                //Serial.printf("Mutex: Released '%s'\n", name);
            }
        }
    }

    /**
     * @brief Check if mutex was successfully acquired
     * @return true if mutex is held, false otherwise
     */
    bool acquired() const {
        return wasAcquired;
    }

    /**
     * @brief Explicit operator bool for if-check
     */
    explicit operator bool() const {
        return wasAcquired;
    }

    // Prevent copying (RAII should not be copied)
    MutexGuard(const MutexGuard&) = delete;
    MutexGuard& operator=(const MutexGuard&) = delete;
};

#endif // RAII_GUARDS_H
