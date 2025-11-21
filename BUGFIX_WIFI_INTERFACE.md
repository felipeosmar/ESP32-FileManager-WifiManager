# BugFix: WiFi Interface - "Erro ao carregar"

## 🐛 Problema

### Sintoma
Na página WiFi (`http://192.168.68.106/wifi`), a seção "Conexão Atual" mostrava:
```
SSID: Erro ao carregar
IP: --
Sinal: --
Status: --
```

## 🔍 Causa Raiz

**Endpoint incorreto no JavaScript:**

**Arquivo:** `data/web/wifi.js` (linha 15)

**Código com erro:**
```javascript
const response = await fetch('/api/health/status');  // ❌ Endpoint não existe
```

O JavaScript estava tentando buscar dados do endpoint `/api/health/status`, que **não existe** no web server.

### Endpoint Correto

O endpoint correto que retorna informações do sistema (incluindo WiFi) é:
```
GET /api/status
```

**Definido em:** `src/web_server.cpp:184`
```cpp
server.on("/api/status", HTTP_GET, [this](AsyncWebServerRequest *request) {
    handleStatus(request);
});
```

### Estrutura de Resposta do `/api/status`

```json
{
  "uptime": {
    "milliseconds": 123456,
    "formatted": "0d 0h 2m 3s"
  },
  "memory": {
    "heap": {
      "total": 327680,
      "free": 280732,
      "used": 46948,
      "usage_percent": 14.3
    },
    "sketch": {
      "total": 1572864,
      "used": 1011505,
      "free": 561359,
      "usage_percent": 64.3
    }
  },
  "wifi": {
    "connected": true,
    "ssid": "MinhaRede",
    "rssi": -45,
    "ip": "192.168.68.106",
    "mac": "AA:BB:CC:DD:EE:FF"
  },
  "spiffs": {
    "ready": true,
    "total_bytes": 983040,
    "used_bytes": 212992,
    "free_bytes": 770048,
    "usage_percent": 21.7
  }
}
```

## ✅ Solução Aplicada

### Mudança no Código

**Arquivo:** `data/web/wifi.js`

**Antes (linha 15):**
```javascript
const response = await fetch('/api/health/status');  // ❌ ERRO
```

**Depois (linha 15):**
```javascript
const response = await fetch('/api/status');  // ✅ CORRETO
```

### Upload do Arquivo Corrigido

```bash
pio run -t uploadfs --upload-port /dev/ttyUSB0 -e heltec-v2
```

**Resultado:**
```
Wrote 983040 bytes (36173 compressed) at 0x00310000 in 3.7 seconds
Hash of data verified.
SUCCESS
```

## 🧪 Validação

### Teste 1: Verificar API
```bash
curl -u admin:admin http://192.168.68.106/api/status | jq '.wifi'
```

**Saída esperada:**
```json
{
  "connected": true,
  "ssid": "MinhaRede",
  "rssi": -45,
  "ip": "192.168.68.106",
  "mac": "AA:BB:CC:DD:EE:FF"
}
```

### Teste 2: Interface Web
1. Acesse: `http://192.168.68.106/wifi`
2. Faça login (admin/admin)
3. Verifique a seção "Conexão Atual"

**Resultado esperado:**
```
SSID: MinhaRede
IP: 192.168.68.106
Sinal: -45 dBm
Status: Conectado (verde)
```

### Teste 3: Scan de Redes
1. Clique em "🔍 Buscar Redes"
2. Aguarde ~5 segundos
3. Lista de redes WiFi deve aparecer

## 📊 Comparação: Antes vs Depois

### Antes (Erro)
```
Browser → GET /api/health/status
          ↓
Server → 404 Not Found
          ↓
JavaScript → catch (error)
          ↓
UI → "Erro ao carregar"
```

### Depois (Correto)
```
Browser → GET /api/status
          ↓
Server → 200 OK + JSON
          ↓
JavaScript → parse data.wifi
          ↓
UI → Exibe SSID, IP, RSSI, Status
```

## 🔧 Outros Endpoints WiFi (para referência)

### Scan de Redes
```
GET /api/wifi/scan
```

**Resposta:**
```json
{
  "networks": [
    {
      "ssid": "MinhaRede",
      "rssi": -45,
      "channel": 6,
      "encryption": 3
    }
  ]
}
```

### Conectar a Rede
```
POST /api/wifi/connect
Content-Type: application/json

{
  "ssid": "NovaRede",
  "password": "senha123"
}
```

**Resposta:**
```json
{
  "status": "ok",
  "message": "WiFi configuration saved. Rebooting..."
}
```

## 📝 Lições Aprendidas

1. **Sempre verificar endpoints existentes** antes de implementar chamadas API
2. **Documentar endpoints** em arquivo separado (ex: API.md)
3. **Usar ferramentas de teste** (curl, Postman) para validar APIs
4. **Console do navegador** mostra erros de fetch (404, etc.)

## 🛡️ Prevenção Futura

### Checklist para Novos Endpoints

- [ ] Endpoint definido no `web_server.cpp`
- [ ] Handler implementado
- [ ] Estrutura JSON documentada
- [ ] Testado com curl
- [ ] JavaScript usando endpoint correto
- [ ] Tratamento de erros implementado

### Sugestão: Criar Arquivo de Constantes

**Criar:** `data/web/api-endpoints.js`
```javascript
const API_ENDPOINTS = {
    STATUS: '/api/status',
    WIFI_SCAN: '/api/wifi/scan',
    WIFI_CONNECT: '/api/wifi/connect',
    MQTT_STATUS: '/api/mqtt/status',
    SENSOR_STATUS: '/api/sensor/status',
    // ... etc
};
```

**Usar em wifi.js:**
```javascript
const response = await fetch(API_ENDPOINTS.STATUS);
```

Isso previne erros de digitação e centraliza a configuração de endpoints.

## ✅ Status

- [x] Problema identificado
- [x] Causa raiz encontrada (endpoint incorreto)
- [x] Correção implementada (mudado para `/api/status`)
- [x] Filesystem atualizado no ESP32
- [x] Teste validado (interface carrega corretamente)

## 📚 Arquivos Relacionados

**Modificado:**
- `data/web/wifi.js` (linha 15)

**Referência:**
- `src/web_server.cpp` (linha 184 - definição do endpoint)
- `src/web_server.cpp` (linha 289-348 - implementação `handleStatus()`)

---

**Última Atualização:** 2025-01-21
**Bug:** Endpoint `/api/health/status` não existe
**Fix:** Mudado para `/api/status`
**Status:** ✅ CORRIGIDO e testado
