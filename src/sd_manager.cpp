/**
 * SD Card Manager Implementation
 */

#include "sd_manager.h"

SDManager::SDManager() : cardMounted(false) {
}

bool SDManager::begin() {
  // Initialize SD card in 1-bit mode (to avoid conflicts with servos on GPIO12/13)
  // For 4-bit mode, use: SD_MMC.begin("/sdcard", false)
  if (!SD_MMC.begin("/sdcard", true)) {  // true = 1-bit mode
    Serial.println("SD Card Mount Failed");
    cardMounted = false;
    return false;
  }

  uint8_t cardType = SD_MMC.cardType();

  if (cardType == CARD_NONE) {
    Serial.println("No SD card attached");
    cardMounted = false;
    return false;
  }

  cardMounted = true;
  printCardInfo();

  // Create required directories
  createDirectory("/web");

  Serial.println("SD Card ready");
  return true;
}

void SDManager::printCardInfo() {
  uint8_t cardType = SD_MMC.cardType();

  Serial.print("SD Card Type: ");
  if (cardType == CARD_MMC) {
    Serial.println("MMC");
  } else if (cardType == CARD_SD) {
    Serial.println("SDSC");
  } else if (cardType == CARD_SDHC) {
    Serial.println("SDHC");
  } else {
    Serial.println("UNKNOWN");
  }

  uint64_t cardSize = SD_MMC.cardSize() / (1024 * 1024);
  Serial.printf("SD Card Size: %lluMB\n", cardSize);

  uint64_t usedSpace = SD_MMC.usedBytes() / (1024 * 1024);
  Serial.printf("Used space: %lluMB\n", usedSpace);

  uint64_t totalSpace = SD_MMC.totalBytes() / (1024 * 1024);
  Serial.printf("Total space: %lluMB\n", totalSpace);
}

bool SDManager::fileExists(const char *path) {
  if (!cardMounted) return false;

  File file = SD_MMC.open(path);
  if (!file) return false;

  file.close();
  return true;
}

bool SDManager::createDirectory(const char *path) {
  if (!cardMounted) return false;

  if (SD_MMC.mkdir(path)) {
    Serial.printf("Directory created: %s\n", path);
    return true;
  }

  return false;
}

void SDManager::listDir(const char *dirname, uint8_t levels) {
  if (!cardMounted) return;

  Serial.printf("Listing directory: %s\n", dirname);

  File root = SD_MMC.open(dirname);
  if (!root) {
    Serial.println("Failed to open directory");
    return;
  }

  if (!root.isDirectory()) {
    Serial.println("Not a directory");
    return;
  }

  File file = root.openNextFile();
  while (file) {
    if (file.isDirectory()) {
      Serial.print("  DIR : ");
      Serial.println(file.name());
      if (levels) {
        listDir(file.path(), levels - 1);
      }
    } else {
      Serial.print("  FILE: ");
      Serial.print(file.name());
      Serial.print("  SIZE: ");
      Serial.println(file.size());
    }
    file = root.openNextFile();
  }
}
