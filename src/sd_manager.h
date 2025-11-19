/**
 * SD Card Manager
 *
 * Handles SD card initialization and file operations
 */

#ifndef SD_MANAGER_H
#define SD_MANAGER_H

#include <Arduino.h>
#include <SD_MMC.h>

class SDManager {
private:
  bool cardMounted;

public:
  SDManager();

  bool begin();
  bool isReady() const { return cardMounted; }
  void printCardInfo();
  bool fileExists(const char *path);
  bool createDirectory(const char *path);

private:
  void listDir(const char *dirname, uint8_t levels);
};

#endif // SD_MANAGER_H
