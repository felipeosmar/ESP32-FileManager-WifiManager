/**
 * SPIFFS Manager
 *
 * Handles SPIFFS initialization and file operations
 */

#ifndef SPIFFS_MANAGER_H
#define SPIFFS_MANAGER_H

#include <Arduino.h>
#include <FS.h>
#include <LittleFS.h>

class SPIFFSManager {
private:
  bool mounted;

public:
  SPIFFSManager();

  bool begin();
  bool isReady() const { return mounted; }
  void printInfo();
  bool fileExists(const char *path);
  bool createDirectory(const char *path);
  size_t getTotalBytes();
  size_t getUsedBytes();

private:
  void listDir(const char *dirname, uint8_t levels);
};

#endif // SPIFFS_MANAGER_H
