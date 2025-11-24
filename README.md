# ESP32 File Manager & WiFi Manager

Este projeto é um firmware robusto para ESP32 que fornece um gerenciador de arquivos via interface web, gerenciador de WiFi, suporte a sensores de temperatura/umidade, display OLED e integração MQTT.

## 🚀 Funcionalidades

*   **Gerenciador WiFi**: Interface web para configurar credenciais de WiFi (Modo Station) ou funcionar como Ponto de Acesso (AP).
*   **Gerenciador de Arquivos**: Interface web completa para upload, download, edição e exclusão de arquivos no sistema de arquivos do ESP32 (LittleFS).
*   **Servidor Web Assíncrono**: Rápido e responsivo, baseado na biblioteca ESPAsyncWebServer.
*   **Autenticação**: Proteção da interface web com login e senha.
*   **Suporte a Sensores**: Detecção e leitura automática de sensores de temperatura e umidade:
    *   SHT20
    *   SHT30
    *   SHT40
    *   AM2315
*   **Display OLED**: Suporte a displays SSD1306 (I2C) para exibir status do sistema, IP, dados do sensor e status MQTT.
*   **MQTT**: Publicação periódica de status do sistema e dados dos sensores.
*   **OTA (Over-The-Air)**: Atualização de firmware via interface web.
*   **Dual Environment**: Configurações otimizadas para placas com 4MB e 2MB de Flash.

## 🛠️ Hardware Suportado

*   **Microcontrolador**: ESP32 (qualquer placa baseada em ESP32).
*   **Display (Opcional)**: OLED SSD1306 I2C (128x64 ou 128x32).
*   **Sensores (Opcional)**: SHT20, SHT30, SHT40, AM2315 (I2C).

### Pinagem Padrão (I2C)

| Placa / Ambiente | SDA | SCL |
| :--- | :--- | :--- |
| **Padrão (4MB)** | GPIO 4 | GPIO 15 |
| **JVTech V3 (2MB)** | GPIO 21 | GPIO 22 |

> **Nota**: A pinagem pode ser alterada no arquivo `platformio.ini`.

## 💻 Software e Dependências

O projeto é construído utilizando **PlatformIO**. As principais dependências são baixadas automaticamente:

*   [ESPAsyncWebServer](https://github.com/ESP32Async/ESPAsyncWebServer)
*   [AsyncTCP](https://github.com/dvarrel/AsyncTCP)
*   [ArduinoJson](https://arduinojson.org/) (v7)
*   [PubSubClient](https://github.com/knolleary/pubsubclient)
*   [Adafruit GFX](https://github.com/adafruit/Adafruit-GFX-Library) & [SSD1306](https://github.com/adafruit/Adafruit_SSD1306)
*   [NTPClient](https://github.com/arduino-libraries/NTPClient)

## 📥 Instalação

1.  **Clone o repositório**:
    ```bash
    git clone <url-do-repositorio>
    cd ESP32-FileManager-WifiManager
    ```

2.  **Abra no VS Code** com a extensão PlatformIO instalada.

3.  **Selecione o Ambiente**:
    *   `env:heltec-v2`: Para placas ESP32 padrão com 4MB de Flash. Suporta OTA com rollback.
    *   `env:jvtech-v3-2mb`: Para placas com 2MB de Flash. Suporta OTA simples (sem rollback) e otimizações de tamanho.

4.  **Upload do Filesystem (LittleFS)**:
    *   É necessário fazer o upload dos arquivos da pasta `data/` para o ESP32.
    *   No PlatformIO: `PlatformIO: Upload Filesystem Image`.

5.  **Upload do Firmware**:
    *   No PlatformIO: `PlatformIO: Upload`.

## ⚙️ Configuração e Uso

### Primeiro Acesso (Modo AP)

1.  Se o ESP32 não conseguir conectar a um WiFi conhecido, ele criará um Ponto de Acesso (AP).
2.  Conecte-se à rede WiFi gerada (ex: `ESP32-FileManager`).
3.  Acesse `http://192.168.4.1` no navegador.
4.  Configure as credenciais do seu WiFi na página de configurações.

### Interface Web

A interface web possui as seguintes seções:

*   **Status**: Visão geral do sistema (Uptime, Memória, WiFi, Sensor).
*   **File Manager**: Gerenciamento de arquivos do LittleFS.
*   **WiFi**: Configuração de rede.
*   **MQTT**: Configuração do broker MQTT.
*   **Sensor**: Leituras em tempo real e gráficos.
*   **Firmware**: Atualização OTA.

### Configuração via Arquivo (config.json)

As configurações são salvas no arquivo `/config.json` no LittleFS. Exemplo:

```json
{
  "wifi": {
    "ssid": "MinhaRede",
    "password": "MinhaSenha",
    "ap_mode": false
  },
  "mqtt": {
    "enabled": true,
    "server": "mqtt.exemplo.com",
    "port": 1883,
    "user": "usuario",
    "password": "senha",
    "main_topic": "esp32/device1",
    "publish_interval": 60
  },
  "oled": {
    "enabled": true,
    "flip_screen": false,
    "auto_update": true
  },
  "sensor": {
    "type": "AUTO",
    "fahrenheit": false,
    "offset_temp": 0.0,
    "offset_hum": 0.0
  }
}
```

## 📝 Estrutura do Projeto

*   `src/`: Código fonte C++.
    *   `main.cpp`: Loop principal e setup.
    *   `web_server.cpp`: Lógica do servidor web e rotas.
    *   `*_manager.cpp`: Gerenciadores modulares (WiFi, MQTT, OLED, Sensor, SPIFFS).
*   `data/`: Arquivos web (HTML, CSS, JS) e configurações iniciais.
*   `platformio.ini`: Configurações de build e ambientes.
*   `partitions*.csv`: Tabelas de partição personalizadas.

## 🤝 Contribuição

Sinta-se à vontade para abrir issues ou enviar pull requests com melhorias e correções.

## 📄 Licença

Este projeto é de código aberto. Consulte o arquivo LICENSE para mais detalhes.
