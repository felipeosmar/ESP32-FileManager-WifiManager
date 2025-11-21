#ifndef CONFIG_H
#define CONFIG_H

// WiFi Configuration
#define WIFI_SSID_DEFAULT "Termo-Higrometro"
#define WIFI_PASS_DEFAULT "12345678"
#define WIFI_AP_MODE_DEFAULT true

// Web Interface Configuration
#define WEB_USERNAME "admin"
#define WEB_PASSWORD "admin"

// Pin Definitions
#ifndef PIN_I2C_SDA
#define PIN_I2C_SDA 21
#endif

#ifndef PIN_I2C_SCL
#define PIN_I2C_SCL 22
#endif

// OLED Reset Pin (optional, set to -1 if not used)
#ifndef PIN_OLED_RST
#define PIN_OLED_RST -1
#endif


// System Configuration
#define SERIAL_BAUD_RATE 115200
#define I2C_CLOCK_SPEED 100000

#endif // CONFIG_H
