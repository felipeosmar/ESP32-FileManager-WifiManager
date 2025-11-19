/**
 * SPIFFS Manager Implementation
 */

#include "spiffs_manager.h"

SPIFFSManager::SPIFFSManager() : mounted(false) {
}

bool SPIFFSManager::begin() {
  // Initialize LittleFS (SPIFFS replacement, more reliable)
  if (!LittleFS.begin(true)) {  // true = format on fail
    Serial.println("LittleFS Mount Failed");
    mounted = false;
    return false;
  }

  mounted = true;
  printInfo();

  Serial.println("LittleFS ready");
  return true;
}

void SPIFFSManager::printInfo() {
  Serial.println("\n=== LittleFS Info ===");

  size_t totalBytes = LittleFS.totalBytes();
  size_t usedBytes = LittleFS.usedBytes();
  size_t freeBytes = totalBytes - usedBytes;

  Serial.printf("Total space: %u bytes (%.2f KB)\n", totalBytes, totalBytes / 1024.0);
  Serial.printf("Used space: %u bytes (%.2f KB)\n", usedBytes, usedBytes / 1024.0);
  Serial.printf("Free space: %u bytes (%.2f KB)\n", freeBytes, freeBytes / 1024.0);
  Serial.printf("Usage: %.1f%%\n", (float)usedBytes / totalBytes * 100);
  Serial.println("=====================\n");
}

bool SPIFFSManager::fileExists(const char *path) {
  if (!mounted) return false;
  return LittleFS.exists(path);
}

bool SPIFFSManager::createDirectory(const char *path) {
  if (!mounted) return false;

  if (LittleFS.mkdir(path)) {
    Serial.printf("Directory created: %s\n", path);
    return true;
  }

  return false;
}

size_t SPIFFSManager::getTotalBytes() {
  if (!mounted) return 0;
  return LittleFS.totalBytes();
}

size_t SPIFFSManager::getUsedBytes() {
  if (!mounted) return 0;
  return LittleFS.usedBytes();
}

void SPIFFSManager::listDir(const char *dirname, uint8_t levels) {
  if (!mounted) return;

  Serial.printf("Listing directory: %s\n", dirname);

  File root = LittleFS.open(dirname);
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
