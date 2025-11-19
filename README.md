# ESP32 File Manager com WiFi Manager e OTA

Sistema completo de gerenciamento de arquivos via WiFi para ESP32, com suporte a atualizações OTA (Over-The-Air).

## Características

- **WiFi Manager**: Modo AP (Access Point) e Station (Cliente)
- **File Manager**: Interface web para gerenciar arquivos no cartão SD
  - Upload de arquivos
  - Download de arquivos
  - Visualização e edição de arquivos
  - Exclusão de arquivos e pastas
  - Criação de diretórios
- **Firmware Update (OTA)**: Atualização de firmware via WiFi
- **Health Monitor**: Monitoramento em tempo real do sistema
  - Uso de memória (Heap e PSRAM)
  - Status WiFi e intensidade do sinal
  - Informações do cartão SD
  - Uptime do sistema
  - Informações de CPU e Flash

## Hardware Necessário

- ESP32 (qualquer placa)
- Módulo de cartão SD (interface SD_MMC)
- Cartão microSD formatado em FAT32

## Conexões do Hardware

### Cartão SD (modo 1-bit SD_MMC)

| Cartão SD | ESP32 GPIO |
|-----------|------------|
| CLK       | GPIO 14    |
| CMD       | GPIO 15    |
| D0        | GPIO 2     |
| VCC       | 3.3V       |
| GND       | GND        |

**Nota**: O modo 1-bit é utilizado para evitar conflitos com outros pinos. Para modo 4-bit, modifique em `src/sd_manager.cpp`.

## Instalação

### 1. Preparar o Ambiente

Instale o [PlatformIO](https://platformio.org/):
- Via VSCode: Instale a extensão PlatformIO IDE
- Via CLI: `pip install platformio`

### 2. Clonar o Projeto

```bash
git clone <url-do-repositorio>
cd ESP32-FileManager-WifiManager
```

### 3. Preparar o Cartão SD

1. Formate o cartão microSD em FAT32
2. Copie todo o conteúdo da pasta `data/` para a raiz do cartão SD:
   ```
   SD Card:
   ├── config.json
   └── web/
       ├── index.html
       ├── style.css
       ├── app.js
       ├── filemanager.html
       ├── filemanager.css
       ├── filemanager.js
       ├── firmware.html
       ├── firmware.css
       ├── firmware.js
       ├── health.html
       ├── health.css
       └── health.js
   ```

### 4. Configurar WiFi

Edite o arquivo `config.json` no cartão SD:

**Modo Access Point (padrão)**:
```json
{
  "wifi": {
    "ssid": "ESP32-FileManager",
    "password": "12345678",
    "ap_mode": true
  }
}
```

**Modo Station (conectar a rede existente)**:
```json
{
  "wifi": {
    "ssid": "SuaRedeWiFi",
    "password": "SuaSenha",
    "ap_mode": false
  }
}
```

### 5. Compilar e Fazer Upload

```bash
# Conecte o ESP32 via USB
platformio run --target upload

# Ou, via VSCode PlatformIO:
# Clique em "Upload" na barra inferior
```

### 6. Monitorar a Serial

```bash
platformio device monitor

# Ou via VSCode: clique em "Serial Monitor"
```

Você verá o IP do ESP32 na serial:
```
=== ESP32 File Manager ===
Initializing SD card...
SD Card initialized successfully
Setting up WiFi...
AP Mode - SSID: ESP32-FileManager
IP Address: 192.168.4.1
=== System Ready ===
Web interface: http://192.168.4.1/
```

## Uso

### Acessar a Interface Web

1. **Modo AP**: Conecte-se ao WiFi `ESP32-FileManager` (senha: `12345678`)
2. **Modo Station**: Use o IP exibido na serial
3. Abra o navegador e acesse o IP

### Páginas Disponíveis

- **/** - Página inicial com resumo do sistema
- **/filemanager** - Gerenciador de arquivos
- **/firmware** - Atualização de firmware OTA
- **/health** - Monitor de saúde do sistema

### Atualizar Firmware via OTA

1. Compile o novo firmware: `platformio run`
2. O arquivo `.bin` estará em `.pio/build/esp32dev/firmware.bin`
3. Acesse `http://<IP-DO-ESP32>/firmware`
4. Selecione o arquivo `.bin` e faça upload
5. Aguarde a conclusão e o ESP32 reiniciará automaticamente

**IMPORTANTE**: O sistema valida o arquivo .bin antes de aplicar. Apenas arquivos ESP32 válidos são aceitos.

## Estrutura do Projeto

```
ESP32-FileManager-WifiManager/
├── src/
│   ├── main.cpp           # Código principal
│   ├── sd_manager.h       # Gerenciador do cartão SD
│   ├── sd_manager.cpp
│   └── web_server.h       # Definições do servidor web
├── data/                  # Arquivos para copiar ao SD
│   ├── config.json        # Configuração WiFi
│   └── web/              # Interface web
│       ├── index.html
│       ├── filemanager.*
│       ├── firmware.*
│       └── health.*
├── platformio.ini         # Configuração do projeto
└── README.md
```

## API REST

O sistema expõe uma API REST para integração:

### Health Status
```
GET /api/health/status
```
Retorna informações do sistema em JSON.

### File Manager
```
GET  /api/files/list?dir=/path
GET  /api/files/download?file=/path/file.txt
GET  /api/files/view?file=/path/file.txt
GET  /api/files/read?file=/path/file.txt
POST /api/files/write (file, content)
POST /api/files/delete (file)
POST /api/files/upload (multipart/form-data)
POST /api/files/mkdir (dir)
```

### Firmware Update
```
POST /api/firmware/upload (multipart/form-data)
```

## Configurações Avançadas

### Alterar Partições

O projeto usa `min_spiffs.csv` para suportar OTA. Para alterar:

1. Edite `platformio.ini`:
   ```ini
   board_build.partitions = custom.csv
   ```

2. Crie `custom.csv` na raiz do projeto com o layout desejado.

### Mudar para Modo SD 4-bit

Para melhor desempenho, edite `src/sd_manager.cpp`:

```cpp
// Linha 13: Mude para false (4-bit mode)
if (!SD_MMC.begin("/sdcard", false)) {  // false = 4-bit mode
```

**Conexões adicionais para 4-bit**:
- D1: GPIO 4
- D2: GPIO 12
- D3: GPIO 13

## Solução de Problemas

### SD Card não inicializa
- Verifique as conexões
- Certifique-se de que o cartão está formatado em FAT32
- Tente outro cartão SD

### WiFi não conecta (Station Mode)
- Verifique SSID e senha no `config.json`
- O sistema automaticamente volta para AP mode se falhar

### OTA falha
- Verifique se o arquivo .bin é válido
- Certifique-se de ter espaço suficiente (partição OTA)
- Não interrompa o processo de atualização

### Página web não carrega
- Verifique se os arquivos estão na pasta `/web/` do SD
- Use o Serial Monitor para ver logs de erro

## Desenvolvimento

### Compilar
```bash
platformio run
```

### Upload via Serial
```bash
platformio run --target upload
```

### Limpar build
```bash
platformio run --target clean
```

### Monitor Serial
```bash
platformio device monitor
```

## Segurança

- Altere as credenciais padrão em `config.json`
- Use senhas fortes para o WiFi
- Mantenha o firmware atualizado
- Não exponha o sistema diretamente na internet sem proteção adicional

## Licença

MIT License - veja o arquivo LICENSE para detalhes

## Créditos

Baseado no projeto [ESP32-file_manager_object_tracker](https://github.com/felipeosmar/ESP32-file_manager_object_tracker.git)

Funcionalidades adaptadas:
- File Manager
- Firmware Update (OTA)
- WiFi Manager
- Health Monitor

## Contribuindo

Contribuições são bem-vindas! Sinta-se livre para:
- Reportar bugs
- Sugerir novas funcionalidades
- Enviar pull requests

## Changelog

### v1.0.0 (2024)
- Versão inicial
- File Manager completo
- OTA Updates
- WiFi Manager (AP + Station)
- Health Monitor
- Interface web responsiva
