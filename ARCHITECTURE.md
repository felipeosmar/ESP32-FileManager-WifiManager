# ESP32 File Manager & WiFi Manager - Arquitetura do Sistema

## Visão Geral

Este é um projeto de plataforma IoT completa para ESP32 com gerenciamento via web, suporte multi-sensor, comunicação de longo alcance (LoRaWAN), e publicação de dados via MQTT.

**Repositório**: https://github.com/felipeosmar/ESP32-FileManager-WifiManager
**Linguagem**: C++ (Arduino Framework) + HTML/JS/CSS
**Plataforma**: ESP32 (Espressif32)

---

## Fluxograma Geral do Sistema

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                              INICIALIZAÇÃO (setup)                           │
└─────────────────────────────────────────────────────────────────────────────┘
                                       │
    ┌──────────────────────────────────┼──────────────────────────────────┐
    ▼                                  ▼                                  ▼
┌─────────┐                     ┌─────────────┐                    ┌─────────────┐
│ Serial  │                     │  LittleFS   │                    │   Mutexes   │
│ 115200  │                     │   Mount     │                    │ SPIFFS/I2C  │
└─────────┘                     └─────────────┘                    └─────────────┘
                                       │
                                       ▼
                              ┌─────────────────┐
                              │  config.json    │
                              │    Loading      │
                              └─────────────────┘
                                       │
    ┌──────────────────┬───────────────┼───────────────┬──────────────────┐
    ▼                  ▼               ▼               ▼                  ▼
┌────────┐      ┌──────────┐    ┌──────────┐    ┌──────────┐      ┌──────────┐
│  WiFi  │      │   MQTT   │    │  Sensor  │    │   OLED   │      │ LoRaWAN  │
│ Config │      │  Config  │    │  Config  │    │  Config  │      │  Config  │
└────────┘      └──────────┘    └──────────┘    └──────────┘      └──────────┘
    │
    ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                              CONEXÃO WIFI                                    │
│  ┌─────────────┐         ┌─────────────────┐         ┌─────────────────┐    │
│  │ STA Mode    │──Fail──▶│ 30 tentativas   │──Fail──▶│    AP Mode      │    │
│  │(Conectar)   │         │   (timeout)     │         │ (192.168.4.1)   │    │
│  └─────────────┘         └─────────────────┘         └─────────────────┘    │
└─────────────────────────────────────────────────────────────────────────────┘
                                       │
            ┌──────────────────────────┼──────────────────────────┐
            ▼                          ▼                          ▼
     ┌─────────────┐           ┌─────────────┐            ┌─────────────┐
     │    NTP      │           │  WebServer  │            │    I2C      │
     │  Init/Sync  │           │   Start     │            │    Scan     │
     └─────────────┘           └─────────────┘            └─────────────┘
                                                                 │
                                              ┌──────────────────┼──────────────┐
                                              ▼                  ▼              ▼
                                       ┌───────────┐      ┌───────────┐  ┌───────────┐
                                       │   OLED    │      │  Sensor   │  │  LoRaWAN  │
                                       │   Init    │      │ AutoDetect│  │   Init    │
                                       └───────────┘      └───────────┘  └───────────┘

┌─────────────────────────────────────────────────────────────────────────────┐
│                              LOOP PRINCIPAL                                  │
└─────────────────────────────────────────────────────────────────────────────┘
                                       │
    ┌───────────┬───────────┬──────────┼──────────┬───────────┬───────────┐
    ▼           ▼           ▼          ▼          ▼           ▼           ▼
┌───────┐  ┌────────┐  ┌────────┐ ┌────────┐ ┌────────┐  ┌────────┐  ┌────────┐
│  NTP  │  │WebSock │  │  MQTT  │ │ Sensor │ │  OLED  │  │LoRaWAN │  │  MQTT  │
│Update │  │Handler │  │  Loop  │ │ Update │ │ Update │  │ Events │  │Publish │
│(hora) │  │(websoc)│  │(msgs)  │ │(2seg)  │ │(2seg)  │  │(async) │  │(5min)  │
└───────┘  └────────┘  └────────┘ └────────┘ └────────┘  └────────┘  └────────┘
```

---

## Fluxograma do Web Server

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                              WEB SERVER (porta 80)                           │
└─────────────────────────────────────────────────────────────────────────────┘
                                       │
                            ┌──────────┴──────────┐
                            ▼                     ▼
                     ┌─────────────┐       ┌─────────────┐
                     │   HTTP      │       │  WebSocket  │
                     │  Requests   │       │  (/ws)      │
                     └─────────────┘       └─────────────┘
                            │                     │
                            ▼                     ▼
                     ┌─────────────┐       ┌─────────────┐
                     │  checkAuth  │       │  Broadcast  │
                     │  (SHA256)   │       │    Logs     │
                     └─────────────┘       └─────────────┘
                            │
         ┌──────────────────┼──────────────────────────────────────┐
         ▼                  ▼                  ▼                   ▼
  ┌─────────────┐    ┌─────────────┐    ┌─────────────┐    ┌─────────────┐
  │  /api/wifi  │    │  /api/mqtt  │    │ /api/sensor │    │/api/lorawan │
  │             │    │             │    │             │    │             │
  │ • scan      │    │ • config    │    │ • status    │    │ • status    │
  │ • connect   │    │ • status    │    │ • config    │    │ • config    │
  │ • status    │    │ • test      │    │ • read      │    │ • join      │
  └─────────────┘    │ • publish   │    └─────────────┘    │ • uplink    │
                     └─────────────┘                       └─────────────┘
         │                  │                  │                   │
         ▼                  ▼                  ▼                   ▼
  ┌─────────────┐    ┌─────────────┐    ┌─────────────┐    ┌─────────────┐
  │  /api/ntp   │    │  /api/files │    │ /api/update │    │  /api/auth  │
  │             │    │             │    │             │    │             │
  │ • config    │    │ • list      │    │ • OTA       │    │ • status    │
  │ • time      │    │ • upload    │    │  (firmware) │    │ • change-   │
  │ • sync      │    │ • download  │    │             │    │   password  │
  └─────────────┘    │ • delete    │    └─────────────┘    └─────────────┘
                     │ • edit      │
                     └─────────────┘
```

---

## Fluxograma de Autenticação

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                           FLUXO DE AUTENTICAÇÃO                              │
└─────────────────────────────────────────────────────────────────────────────┘

   ┌────────────┐
   │  Request   │
   │  HTTP      │
   └────────────┘
         │
         ▼
   ┌────────────┐
   │ Extrai     │
   │ Header     │
   │ Auth Basic │
   └────────────┘
         │
         ▼
   ┌────────────┐        ┌────────────┐
   │ Base64     │───────▶│  Decode    │
   │ Encoded?   │        │ user:pass  │
   └────────────┘        └────────────┘
         │                     │
         │ Não                 ▼
         ▼               ┌────────────┐
   ┌────────────┐        │  SHA256    │
   │   401      │        │  (senha)   │
   │Unauthorized│        └────────────┘
   └────────────┘              │
                               ▼
                         ┌────────────┐
                         │  Compara   │
                         │  com hash  │
                         │config.json │
                         └────────────┘
                               │
              ┌────────────────┼────────────────┐
              ▼                                 ▼
        ┌────────────┐                   ┌────────────┐
        │   Match    │                   │  No Match  │
        │  ✓ Acesso  │                   │  ✗ 401     │
        └────────────┘                   └────────────┘
```

---

## Fluxograma de Detecção de Sensores

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                         AUTO-DETECÇÃO DE SENSORES                            │
└─────────────────────────────────────────────────────────────────────────────┘

   ┌────────────┐
   │ I2C Scan   │
   │ Bus        │
   └────────────┘
         │
         ▼
   ┌────────────────────────────────────────────────────────────────┐
   │                    Endereços Detectados                        │
   └────────────────────────────────────────────────────────────────┘
         │
    ┌────┴────┬─────────┬─────────┬─────────┐
    ▼         ▼         ▼         ▼         ▼
┌───────┐ ┌───────┐ ┌───────┐ ┌───────┐ ┌───────┐
│ 0x40  │ │ 0x44  │ │ 0x45  │ │ 0x5C  │ │ 0x3C  │
│ SHT20 │ │SHT30/40│ │ SHT30 │ │AM2315 │ │ OLED  │
└───────┘ └───────┘ └───────┘ └───────┘ └───────┘
    │         │         │         │         │
    ▼         ▼         ▼         ▼         ▼
┌───────────────────────────────────────────────────────────────────┐
│                  Prioridade de Seleção                            │
│        SHT20 > SHT30 > SHT40 > AM2315 (sensores)                 │
│        OLED (display independente)                                │
└───────────────────────────────────────────────────────────────────┘
         │
         ▼
   ┌────────────┐       ┌────────────┐
   │  Sensor    │──────▶│   Dados    │
   │  Leitura   │       │ Temp/Humid │
   │ (intervalo)│       │  + CRC     │
   └────────────┘       └────────────┘
```

---

## Fluxograma MQTT

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                            FLUXO MQTT                                        │
└─────────────────────────────────────────────────────────────────────────────┘

┌─────────────┐     ┌─────────────┐     ┌─────────────┐
│WiFi Online? │─Sim─▶│ MQTT       │─────▶│ Conectar   │
└─────────────┘     │ Habilitado? │     │ ao Broker  │
      │             └─────────────┘     └─────────────┘
      │ Não               │                   │
      ▼                   ▼ Não               ▼
┌─────────────┐     ┌─────────────┐     ┌─────────────┐
│   Skip      │     │   Skip      │     │  Reconexão │
│   MQTT      │     │   MQTT      │     │ Automática │
└─────────────┘     └─────────────┘     └─────────────┘
                                              │
                                              ▼
                                   ┌─────────────────┐
                                   │   Publicação    │
                                   │   Periódica     │
                                   │   (300s/5min)   │
                                   └─────────────────┘
                                              │
                    ┌─────────────────────────┼─────────────────────────┐
                    ▼                         ▼                         ▼
             ┌───────────┐             ┌───────────┐             ┌───────────┐
             │ .../status│             │.../sensor │             │ .../uplink│
             │  (JSON)   │             │  (JSON)   │             │(LoRaWAN)  │
             └───────────┘             └───────────┘             └───────────┘

Estrutura do Tópico: {mainTopic}/{hostname}/{subtopic}
Exemplo: dashfer/EDXXX/status
```

---

## Fluxograma LoRaWAN

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                           FLUXO LORAWAN                                      │
└─────────────────────────────────────────────────────────────────────────────┘

   ┌────────────┐
   │  SX1276    │
   │  Init SPI  │
   └────────────┘
         │
         ▼
   ┌────────────────────────────────────────────────┐
   │           Modo de Ativação                     │
   └────────────────────────────────────────────────┘
         │                            │
         ▼                            ▼
   ┌────────────┐              ┌────────────┐
   │    OTAA    │              │    ABP     │
   │ (Over-Air) │              │ (Pre-set)  │
   └────────────┘              └────────────┘
         │                            │
         ▼                            ▼
   ┌────────────┐              ┌────────────┐
   │ DevEUI     │              │ DevAddr    │
   │ AppEUI     │              │ NwkSKey    │
   │ AppKey     │              │ AppSKey    │
   └────────────┘              └────────────┘
         │                            │
         └────────────┬───────────────┘
                      ▼
               ┌────────────┐
               │   JOIN     │
               │  Request   │
               └────────────┘
                      │
         ┌────────────┴────────────┐
         ▼                         ▼
   ┌────────────┐           ┌────────────┐
   │  Sucesso   │           │   Falha    │
   │  Joined!   │           │  Retry     │
   └────────────┘           └────────────┘
         │
         ▼
   ┌────────────────────────────────────────────────┐
   │               Device Classes                   │
   │  A: Baixo consumo (RX após TX)                │
   │  B: Janelas programadas                       │
   │  C: RX contínuo (alto consumo)                │
   └────────────────────────────────────────────────┘
         │
         ▼
   ┌────────────┐     ┌────────────┐
   │  Uplink    │────▶│ Downlink   │
   │  (envio)   │     │ (receber)  │
   └────────────┘     └────────────┘
```

---

## Fluxograma OTA (Firmware Update)

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                         OTA FIRMWARE UPDATE                                  │
└─────────────────────────────────────────────────────────────────────────────┘

   ┌────────────┐
   │   Upload   │
   │  .bin File │
   │ (Web UI)   │
   └────────────┘
         │
         ▼
   ┌────────────┐
   │  checkAuth │
   │  (SHA256)  │
   └────────────┘
         │
         ▼
   ┌────────────┐
   │ Watchdog   │
   │  Disable   │
   │  (RAII)    │
   └────────────┘
         │
         ▼
   ┌────────────┐
   │  Receive   │
   │   Chunks   │──┬──▶ Progresso via WebSocket
   └────────────┘  │
         │         │
         ▼         │
   ┌────────────┐  │
   │  Write to  │◀─┘
   │ OTA Part.  │
   └────────────┘
         │
         ▼
   ┌────────────┐
   │  Verify    │
   │  Firmware  │
   └────────────┘
         │
    ┌────┴────┐
    ▼         ▼
┌───────┐ ┌───────┐
│ OK ✓  │ │ FAIL  │
│Reboot │ │ Error │
└───────┘ └───────┘
```

---

## Estrutura do Projeto

```
/src/                          # Código fonte C++
  ├── main.cpp               # Entry point, FreeRTOS setup
  ├── web_server.cpp/h       # HTTP/WebSocket server management
  ├── mqtt_manager.cpp/h     # MQTT protocol handler
  ├── sensor_manager.cpp/h   # Multi-sensor abstraction layer
  ├── sensor_*.cpp/h         # Individual sensor drivers (SHT20/30/40, AM2315)
  ├── oled_manager.cpp/h     # OLED display (SSD1306) control
  ├── lorawan_manager.cpp/h  # LoRaWAN communication (SX1276 radio)
  ├── ntp_manager.cpp/h      # Network Time Protocol client
  ├── spiffs_manager.cpp/h   # LittleFS file system management
  ├── auth_manager.h         # SHA256 authentication utilities
  ├── sensor_interface.h     # Abstract sensor interface (ISensor)
  ├── raii_guards.h          # RAII wrappers (WDT, Mutex)
  └── config.h               # Hardware pin definitions

/data/web/                     # Arquivos da interface web
  ├── index.html             # Dashboard homepage
  ├── header.html            # Navigation sidebar
  ├── footer.js              # Footer component
  ├── app.js                 # Main JS app logic
  ├── unified.css            # Unified styling
  ├── auth.html/js           # Authentication login page
  ├── wifi.html/js           # WiFi configuration page
  ├── mqtt.html/js           # MQTT broker configuration
  ├── sensor.html/js         # Temperature/humidity sensor readings
  ├── ntp.html/js            # NTP time sync configuration
  ├── lorawan.html/js        # LoRaWAN network configuration
  ├── firmware.html/js       # OTA firmware update
  ├── filemanager.html/js    # File browser with ACE editor
  ├── display.html/js        # OLED display configuration
  └── status.html/js         # System status monitoring

/data/
  └── config.json            # Configuration file (WiFi, MQTT, sensors, etc.)

platformio.ini                 # Build configuration for multiple environments
```

---

## Módulos e Responsabilidades

| Módulo | Arquivo | Responsabilidade |
|--------|---------|------------------|
| **Main** | `main.cpp` | Orquestração, loop principal, FreeRTOS |
| **Web Server** | `web_server.cpp/h` | HTTP, WebSocket, APIs REST |
| **MQTT** | `mqtt_manager.cpp/h` | Conexão broker, publish/subscribe |
| **Sensor** | `sensor_manager.cpp/h` | Interface unificada ISensor |
| **SHT20/30/40** | `sensor_sht*.cpp/h` | Drivers I2C sensores Sensirion |
| **AM2315** | `sensor_am2315.cpp/h` | Driver I2C sensor Aosong |
| **OLED** | `oled_manager.cpp/h` | Display SSD1306 128x64 |
| **LoRaWAN** | `lorawan_manager.cpp/h` | Radio SX1276, protocolo LoRaWAN |
| **NTP** | `ntp_manager.cpp/h` | Sincronização de tempo |
| **SPIFFS** | `spiffs_manager.cpp/h` | Sistema de arquivos LittleFS |
| **Auth** | `auth_manager.h` | Hashing SHA256, validação |
| **RAII** | `raii_guards.h` | Guards automáticos (WDT, Mutex) |

---

## Sensores Suportados

| Sensor | Modelo | Endereço I2C | Faixa Temperatura | Faixa Umidade |
|--------|--------|--------------|-------------------|---------------|
| **SHT20** | Sensirion SHT20 | 0x40 | -40 a +125°C | 0-100% RH |
| **SHT30** | Sensirion SHT30/31 | 0x44, 0x45 | -40 a +125°C | 0-100% RH |
| **SHT40** | Sensirion SHT40 | 0x44 | -40 a +125°C | 0-100% RH |
| **AM2315** | Aosong AM2315 | 0x5C | -40 a +80°C | 0-100% RH |

---

## Protocolos de Comunicação

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                         PROTOCOLOS SUPORTADOS                                │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  ┌─────────┐    ┌─────────┐    ┌─────────┐    ┌─────────┐    ┌─────────┐  │
│  │  WiFi   │    │  HTTP   │    │  MQTT   │    │LoRaWAN  │    │   NTP   │  │
│  │802.11bgn│    │WebSocket│    │ 3.1.1   │    │  1.0.3  │    │  UDP    │  │
│  └─────────┘    └─────────┘    └─────────┘    └─────────┘    └─────────┘  │
│      │              │              │              │              │         │
│      │    ┌─────────┴──────────────┘              │              │         │
│      │    │                                       │              │         │
│      ▼    ▼                                       ▼              ▼         │
│  ┌──────────────────────────┐           ┌──────────────────────────────┐  │
│  │      Internet/LAN        │           │      Long Range Radio        │  │
│  │   (até 100m indoor)      │           │      (até 15km outdoor)      │  │
│  └──────────────────────────┘           └──────────────────────────────┘  │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘

Barramentos:
  I2C: OLED + Sensores (SDA=4, SCL=15)
  SPI: LoRa SX1276 (MISO=19, MOSI=27, SCK=5)
  UART: Serial Debug (115200 baud)
```

---

## Páginas da Interface Web

| Página | Arquivo | Propósito |
|--------|---------|-----------|
| **Home** | index.html | Dashboard e tela de boas-vindas |
| **WiFi** | wifi.html/js | Gerenciamento de rede WiFi |
| **MQTT** | mqtt.html/js | Configuração do broker MQTT |
| **Sensor** | sensor.html/js | Leituras de temperatura/umidade |
| **NTP** | ntp.html/js | Sincronização de tempo |
| **LoRaWAN** | lorawan.html/js | Configuração de comunicação LoRaWAN |
| **Firmware** | firmware.html/js | Atualização OTA de firmware |
| **File Manager** | filemanager.html/js | Gerenciador de arquivos com editor ACE |
| **Display** | display.html/js | Configuração do display OLED |
| **Status** | status.html/js | Monitoramento de saúde do sistema |
| **Auth** | auth.html/js | Autenticação de usuário |
| **Header** | header.html/js | Barra de navegação lateral |

---

## Endpoints da API REST

### WiFi
- `GET /api/wifi/scan` - Escanear redes WiFi
- `POST /api/wifi/connect` - Conectar a uma rede
- `GET /api/wifi/status` - Status da conexão

### MQTT
- `GET /api/mqtt/config` - Obter configuração MQTT
- `POST /api/mqtt/config` - Atualizar configuração
- `GET /api/mqtt/status` - Status da conexão
- `POST /api/mqtt/test` - Testar conexão
- `POST /api/mqtt/publish` - Publicar mensagem

### Sensor
- `GET /api/sensor/status` - Dados do sensor
- `GET /api/sensor/config` - Obter configuração
- `POST /api/sensor/config` - Atualizar configuração

### NTP
- `GET /api/ntp/config` - Obter configuração NTP
- `POST /api/ntp/config` - Atualizar configuração
- `GET /api/ntp/time` - Obter hora atual

### LoRaWAN
- `GET /api/lorawan/status` - Status LoRaWAN
- `GET /api/lorawan/config` - Obter configuração
- `POST /api/lorawan/config` - Atualizar configuração
- `POST /api/lorawan/join` - Entrar na rede LoRaWAN
- `POST /api/lorawan/uplink` - Enviar dados uplink

### Arquivos
- `GET /api/files` - Listar arquivos
- `POST /api/files/upload` - Upload de arquivo
- `GET /api/files/download` - Download de arquivo
- `POST /api/files/delete` - Deletar arquivo
- `POST /api/files/edit` - Editar conteúdo

### Autenticação
- `GET /api/auth/status` - Status de autenticação
- `POST /api/auth/change-pwd` - Alterar senha

### Sistema
- `GET /api/status` - Status do sistema
- `POST /api/update` - Atualização OTA

---

## Configuração (config.json)

```json
{
  "web": {
    "username": "admin",
    "password_hash": "8c6976e5b5410415bde908bd4dee15dfb167a9c873fc4bb8a81f6f2ab448a918",
    "first_login": true
  },
  "wifi": {
    "ssid": "NomeDaRede",
    "password": "senha123",
    "ap_mode": false
  },
  "oled": {
    "enabled": false,
    "address": 60,
    "sda_pin": 4,
    "scl_pin": 15,
    "auto_update": true,
    "brightness": 128,
    "flip_display": false
  },
  "sensor": {
    "enabled": false,
    "type": "AUTO",
    "read_interval": 180,
    "fahrenheit": false,
    "custom_address": 0
  },
  "mqtt": {
    "server": "broker.mqtt.com",
    "port": 1883,
    "username": "user",
    "password": "pass",
    "hostname": "ESP32-Device",
    "main_topic": "iot/sensors",
    "enabled": false,
    "publish_interval": 300
  },
  "ntp": {
    "server": "pool.ntp.org",
    "offset": -10800,
    "interval": 3600000,
    "enabled": false
  },
  "lorawan": {
    "enabled": false,
    "activation_mode": "OTAA",
    "region": "US915",
    "device_class": "A",
    "dev_eui": "0000000000000000",
    "app_eui": "0000000000000000",
    "app_key": "00000000000000000000000000000000",
    "pins": {
      "miso": 19, "mosi": 27, "sck": 5,
      "nss": 18, "rst": 14, "dio0": 26,
      "dio1": 33, "dio2": 32
    }
  }
}
```

---

## Pinagem de Hardware

### I2C (Compartilhado)
| Pino | GPIO | Dispositivos |
|------|------|--------------|
| SDA | 4 | OLED, Sensores |
| SCL | 15 | OLED, Sensores |

### SPI (LoRaWAN - SX1276)
| Pino | GPIO |
|------|------|
| MISO | 19 |
| MOSI | 27 |
| SCK | 5 |
| NSS | 18 |
| RST | 14 |
| DIO0 | 26 |
| DIO1 | 33 |
| DIO2 | 32 |

### UART (Serial Debug)
| Configuração | Valor |
|--------------|-------|
| Baud Rate | 115200 |

---

## Ambientes de Build (platformio.ini)

| Ambiente | Board | Flash | Partições | Características |
|----------|-------|-------|-----------|-----------------|
| heltec-v2 | esp32dev | 4MB | OTA + SPIFFS | Configuração padrão |
| jvtech-v3-2mb | esp32dev | 2MB | Single OTA | Otimizado para tamanho |

---

## Bibliotecas Utilizadas

- **ESPAsyncWebServer** - Servidor HTTP assíncrono
- **AsyncTCP** - TCP assíncrono para ESP32
- **ArduinoJson** (v7.0.4+) - Parsing JSON
- **PubSubClient** (v2.8+) - Cliente MQTT
- **Adafruit GFX Library** - Gráficos para displays
- **Adafruit SSD1306** - Driver OLED
- **NTPClient** (v3.2.1+) - Cliente NTP
- **RadioLib** (v6.6.0+) - Comunicação LoRa/LoRaWAN

---

## Segurança

### Autenticação
- **Algoritmo**: SHA256 (mbedtls)
- **Requisitos de Senha**:
  - Mínimo 8 caracteres
  - Pelo menos 1 letra maiúscula
  - Pelo menos 1 letra minúscula
  - Pelo menos 1 dígito
- **HTTP Auth**: Basic Authentication (Base64)
- **Credencial Padrão**: admin/admin

### Proteções
- Validação de path para prevenir directory traversal
- Mutex para acesso thread-safe a recursos compartilhados
- Buffers estáticos para prevenir stack overflow
- RAII patterns para cleanup automático

---

## Diagrama de Dependências entre Módulos

```
┌─────────────────────────────────────────────────────────┐
│                    MAIN.CPP                             │
│                  (FreeRTOS Hub)                         │
└─────────────────────────────────────────────────────────┘
           ↓           ↓           ↓           ↓
    ┌──────┴───────────┼───────────┼──────────┴──────┐
    ↓                  ↓           ↓                  ↓
WEB_SERVER         MQTT_MANAGER  SENSOR_MANAGER  OLED_MANAGER
(HTTP/WS)          (Broker)      (I2C)           (I2C/SSD1306)
    │                  │           │                  │
    │                  │      ┌────┴────────┐        │
    │                  │      ↓    ↓    ↓   ↓        │
    │                  │   SHT20 SHT30 SHT40 AM2315  │
    │                  │   (I2C sensors)             │
    │                  │                              │
    ├─────────────┬────┴──────────┬──────────────────┤
    ↓             ↓               ↓                  ↓
SPIFFS_MGR   NTP_MANAGER  LORAWAN_MANAGER    AUTH_MANAGER
(File IO)    (UDP/NTP)    (SPI/SX1276)       (SHA256 hash)
             (System Time)
```

---

## Características Técnicas

- **Linhas de Código C++**: ~7.000
- **Linhas de Código Web**: ~5.000
- **Arquitetura**: Multi-core ESP32 com FreeRTOS
- **Servidor**: Async HTTP/WebSocket (não-bloqueante)
- **Sistema de Arquivos**: LittleFS
- **Compressão**: Suporte a Gzip para arquivos web
- **Comunicação em Tempo Real**: WebSocket para logs e status

---

## Casos de Uso Típicos

1. **Monitoramento Ambiental** - Coleta de temperatura e umidade
2. **Comunicação IoT de Longo Alcance** - Via LoRaWAN
3. **Gerenciamento Remoto** - Interface web completa
4. **Atualização Over-the-Air** - Firmware updates via web
5. **Publicação de Dados** - Integração com brokers MQTT
6. **Operações Sincronizadas** - NTP para timestamp preciso
7. **Gerenciamento de Arquivos** - Logs e configurações locais
