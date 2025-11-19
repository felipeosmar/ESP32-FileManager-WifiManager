# ESP32 File Manager com WiFi Manager e OTA (SPIFFS)

Sistema completo de gerenciamento de arquivos via WiFi para ESP32, usando partição SPIFFS interna com suporte a atualizações OTA.

## ⚠️ SEM SD CARD NECESSÁRIO!

Este projeto usa a **partição SPIFFS interna** do ESP32. Não é necessário módulo SD Card!

## Características

- **WiFi Manager**: Modo AP (Access Point) e Station (Cliente)
- **File Manager**: Interface web para gerenciar arquivos na SPIFFS
  - Upload de arquivos
  - Download de arquivos
  - Visualização e edição de arquivos
  - Exclusão de arquivos e pastas
  - Criação de diretórios
- **Firmware Update (OTA)**: Atualização de firmware via WiFi
- **Health Monitor**: Monitoramento em tempo real do sistema
  - Uso de memória (Heap e PSRAM)
  - Status WiFi e intensidade do sinal
  - Informações da partição SPIFFS
  - Uptime do sistema
  - Informações de CPU e Flash

## Hardware Necessário

- ✅ ESP32 (qualquer placa)
- ✅ Cabo USB para programação
- ❌ **NÃO** precisa de SD Card!
- ❌ **NÃO** precisa de módulo SD!

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

### 3. Configurar WiFi (Opcional)

Edite o arquivo `data/config.json`:

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

### 4. Upload do Filesystem (SPIFFS)

**IMPORTANTE**: Faça isso ANTES do primeiro upload do firmware!

```bash
# Upload dos arquivos web para SPIFFS
platformio run --target uploadfs
```

Ou via VSCode:
1. Pressione Ctrl+Shift+P
2. Digite "Upload File System image"
3. Pressione Enter

### 5. Upload do Firmware

```bash
platformio run --target upload

# Ou via VSCode: clique em "Upload" na barra inferior
```

### 6. Monitorar a Serial

```bash
platformio device monitor
```

Você verá o IP do ESP32:
```
=== ESP32 File Manager (SPIFFS) ===
Initializing LittleFS...
LittleFS ready
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
- **/filemanager** - Gerenciador de arquivos SPIFFS
- **/firmware** - Atualização de firmware OTA
- **/health** - Monitor de saúde do sistema

### Atualizar Firmware via OTA

1. Compile o novo firmware: `platformio run`
2. O arquivo `.bin` estará em `.pio/build/esp32dev/firmware.bin`
3. Acesse `http://<IP-DO-ESP32>/firmware`
4. Selecione o arquivo `.bin` e faça upload
5. Aguarde a conclusão e o ESP32 reiniciará automaticamente

## Estrutura do Projeto

```
ESP32-FileManager-WifiManager/
├── src/
│   ├── main.cpp              # Código principal
│   ├── spiffs_manager.h      # Gerenciador SPIFFS
│   ├── spiffs_manager.cpp
│   └── web_server.h          # Definições do servidor web
├── data/                     # Arquivos para SPIFFS
│   ├── config.json           # Configuração WiFi
│   └── web/                  # Interface web (12 arquivos)
├── partitions.csv            # Tabela de partições customizada
├── platformio.ini            # Configuração do projeto
└── README.md
```

## Particionamento da Flash

```
Total: 4MB Flash
├── NVS:     20KB   (0x9000-0xE000)
├── OTA Data: 8KB   (0xE000-0x10000)
├── APP 0:  1.5MB   (0x10000-0x190000)   - Firmware principal
├── APP 1:  1.5MB   (0x190000-0x310000)  - OTA backup
└── SPIFFS: 960KB   (0x310000-0x400000)  - Arquivos web + usuário
```

## API REST

O sistema expõe uma API REST para integração:

### Health Status
```
GET /api/health/status
```

### File Manager
```
GET  /api/files/list?dir=/path
GET  /api/files/download?file=/path/file.txt
POST /api/files/upload (multipart/form-data)
POST /api/files/write (file, content)
POST /api/files/delete (file)
POST /api/files/mkdir (dir)
```

### Firmware Update
```
POST /api/firmware/upload (multipart/form-data)
```

## Desenvolvimento

### Workflow Completo

```bash
# 1. Fazer mudanças no código
# 2. Upload filesystem (se alterou arquivos em data/)
platformio run --target uploadfs

# 3. Compilar e fazer upload
platformio run --target upload

# 4. Monitor serial
platformio device monitor
```

### Apenas Código (sem filesystem)

```bash
platformio run --target upload
```

### Apenas Filesystem (sem código)

```bash
platformio run --target uploadfs
```

## Configurações Avançadas

### Alterar Tamanho da Partição SPIFFS

Edite `partitions.csv`:

```csv
# Para aumentar SPIFFS para 1.5MB
# Reduza APP0 e APP1 para 1.2MB cada:
app0,   app,  ota_0,   0x10000, 0x130000,
app1,   app,  ota_1,   0x140000,0x130000,
spiffs, data, spiffs,  0x270000,0x180000,  # 1.5MB
```

**⚠️ Atenção**: Certifique-se de que o firmware cabe nas partições APP!

## Solução de Problemas

### SPIFFS não inicializa
- Execute `uploadfs` primeiro: `platformio run --target uploadfs`
- Verifique a tabela de partições em `partitions.csv`
- Tente formatar via código (adicione `LittleFS.format()`)

### Interface web não aparece
- Confirme que executou `uploadfs`
- Verifique Serial Monitor para erros
- Todos os arquivos devem estar em `data/web/`

### OTA falha
- Verifique se o arquivo .bin é válido
- Certifique-se de ter espaço suficiente (partição OTA)
- Firmware deve caber em 1.5MB

### Sem espaço na SPIFFS
- Delete arquivos via File Manager
- Verifique uso em `/health`
- Considere aumentar partição SPIFFS

## Vantagens vs SD Card

| Característica | SPIFFS | SD Card |
|----------------|--------|---------|
| Confiabilidade | ⭐⭐⭐⭐⭐ | ⭐⭐⭐ |
| Velocidade | ⭐⭐⭐⭐ | ⭐⭐⭐ |
| Custo | ⭐⭐⭐⭐⭐ | ⭐⭐⭐ |
| Espaço | ⭐⭐ (960KB) | ⭐⭐⭐⭐⭐ (GB) |
| Complexidade | ⭐⭐⭐⭐⭐ | ⭐⭐⭐ |

**Escolha SPIFFS se**:
- Precisa de alta confiabilidade
- Arquivos pequenos (<1MB)
- Menos componentes = melhor
- Ambiente com vibrações

**Escolha SD Card se**:
- Precisa de muito espaço (>10MB)
- Arquivos grandes (logs, imagens, vídeos)
- Fácil remoção física de dados

## Segurança

- Altere as credenciais padrão em `config.json`
- Use senhas fortes para o WiFi
- Mantenha o firmware atualizado
- Não exponha o sistema diretamente na internet

## Performance

- **SPIFFS Leitura**: ~200 KB/s
- **SPIFFS Escrita**: ~50 KB/s
- **Upload OTA**: ~100 KB/s
- **Interface Web**: Rápida e responsiva

## Licença

MIT License - veja o arquivo LICENSE para detalhes

## Créditos

Baseado no projeto [ESP32-file_manager_object_tracker](https://github.com/felipeosmar/ESP32-file_manager_object_tracker.git)

**Mudanças principais**:
- ✅ Substituído SD Card por SPIFFS/LittleFS
- ✅ Removida dependência de hardware externo
- ✅ Partição customizada com OTA
- ✅ Interface web otimizada
- ✅ Documentação completa

## FAQ

### 1. Posso usar com ESP32-S2/S3/C3?
Sim! Basta mudar `board = esp32dev` para seu modelo no `platformio.ini`.

### 2. Como resetar para configuração padrão?
Delete `/config.json` via File Manager ou execute `uploadfs` novamente.

### 3. Posso adicionar mais arquivos na SPIFFS?
Sim! Até o limite de 960KB. Monitore via `/health`.

### 4. E se eu deletar os arquivos do `/web/`?
Execute `platformio run --target uploadfs` para restaurar.

### 5. Preciso fazer uploadfs toda vez?
Não! Apenas quando alterar arquivos em `data/`.

## Contribuindo

Contribuições são bem-vindas! Sinta-se livre para:
- Reportar bugs
- Sugerir novas funcionalidades
- Enviar pull requests

## Changelog

### v2.0.0 (2024) - SPIFFS Edition
- ✅ Migrado de SD Card para SPIFFS/LittleFS
- ✅ Partição customizada (960KB SPIFFS)
- ✅ Sem hardware externo necessário
- ✅ Performance melhorada
- ✅ Maior confiabilidade

### v1.0.0 (2024)
- Versão inicial com SD Card
