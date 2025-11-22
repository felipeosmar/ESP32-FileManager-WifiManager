# Customização MQTT - Client ID e Hostname

## 📋 Resumo das Alterações

### Objetivo
Modificar a funcionalidade MQTT para:
1. **Client ID**: Sempre usar o MAC Address completo do ESP32
2. **Hostname**: Permitir configuração do hostname do dispositivo via interface web

---

## ✅ Implementação

### 1. **Client ID Baseado em MAC Address**

**Arquivo:** `src/mqtt_manager.cpp`

#### Função `generateClientId()` (linha 86-93)
```cpp
void MQTTManager::generateClientId() {
    // Always use full MAC address as Client ID
    uint8_t mac[6];
    WiFi.macAddress(mac);
    snprintf(config.clientId, sizeof(config.clientId),
             "%02X%02X%02X%02X%02X%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}
```

**Comportamento:**
- Client ID é **sempre** gerado a partir do MAC Address completo
- Formato: `C44F3375EF05` (12 caracteres hexadecimais)
- **Não é editável** pelo usuário
- Regenerado automaticamente ao carregar configuração

**Exemplo:**
- MAC: `C4:4F:33:75:EF:05`
- Client ID: `C44F3375EF05`

---

### 2. **Campo Hostname Configurável**

**Estrutura de Dados:**

#### `src/mqtt_manager.h` (linha 20-29)
```cpp
struct MQTTConfig {
    char server[64];
    uint16_t port;
    char username[32];
    char password[64];
    char clientId[32];
    char hostname[32];      // ← NOVO CAMPO
    char mainTopic[64];
    bool enabled;
};
```

**Arquivo de Configuração (`data/config.json`):**
```json
{
  "mqtt": {
    "enabled": false,
    "server": "",
    "port": 1883,
    "username": "",
    "password": "",
    "hostname": "ESP32-Device",     ← NOVO CAMPO
    "main_topic": "esp32/data",
    "client_id": "C44F3375EF05"
  }
}
```

---

### 3. **Interface Web Atualizada**

**Arquivo:** `data/web/mqtt.html` (linhas 97-129)

#### Campo Hostname (editável)
```html
<div class="form-group">
    <label for="mqttHostname">Hostname do Dispositivo *</label>
    <input type="text"
           id="mqttHostname"
           name="hostname"
           class="form-input"
           placeholder="ESP32-Device"
           required>
    <small>Nome amigável do dispositivo</small>
</div>
```

#### Campo Client ID (somente leitura)
```html
<div class="form-group">
    <label for="mqttClientId">Client ID (MAC Address)</label>
    <input type="text"
           id="mqttClientId"
           name="client_id"
           class="form-input"
           placeholder="Gerado do MAC"
           readonly
           style="background-color: var(--card-bg); cursor: not-allowed;">
    <small>Gerado automaticamente a partir do endereço MAC do ESP32</small>
</div>
```

---

### 4. **API Endpoints**

#### GET `/api/mqtt/config`
**Resposta:**
```json
{
  "enabled": false,
  "server": "",
  "port": 1883,
  "username": "",
  "password": "",
  "hostname": "ESP32-Device",
  "main_topic": "esp32/data",
  "client_id": "C44F3375EF05"
}
```

#### POST `/api/mqtt/config`
**Request Body:**
```json
{
  "enabled": true,
  "server": "broker.hivemq.com",
  "port": 1883,
  "username": "",
  "password": "",
  "hostname": "ESP32-Sensor-01",
  "main_topic": "home/esp32/sensor01"
}
```

**Nota:** O campo `client_id` não precisa ser enviado, pois será sempre gerado automaticamente.

---

## 🔧 Arquivos Modificados

### Backend (C++)
1. **src/mqtt_manager.h**
   - Adicionado campo `hostname` na struct `MQTTConfig`
   - Atualizada assinatura de `updateConfig()`

2. **src/mqtt_manager.cpp**
   - Modificado `generateClientId()` para usar MAC completo
   - Modificado `loadConfig()` para sempre regenerar clientId
   - Adicionado suporte a hostname em `loadConfig()`, `saveConfig()`, `updateConfig()`

3. **src/web_server.cpp**
   - Atualizado `handleMQTTConfigGet()` para incluir hostname
   - Atualizado `handleMQTTConfigPost()` para processar hostname

### Frontend (Web)
1. **data/web/mqtt.html**
   - Adicionado campo hostname (editável)
   - Client ID agora é readonly com estilo visual diferenciado

2. **data/web/mqtt.js**
   - Atualizado `loadMQTTConfig()` para carregar hostname
   - Atualizado `saveMQTTConfig()` para enviar hostname

---

## 📊 Comportamento

### Ao Carregar Configuração
```
1. Carrega dados do config.json
2. Se hostname não existir → usa "ESP32-Device"
3. SEMPRE regenera Client ID a partir do MAC
4. Ignora qualquer client_id salvo no arquivo
```

### Ao Salvar Configuração
```
1. Valida campos obrigatórios
2. Salva hostname fornecido pelo usuário
3. Regenera Client ID automaticamente
4. Salva client_id apenas para referência
```

### Ao Conectar ao Broker MQTT
```
1. Usa Client ID gerado (MAC Address)
2. Hostname é armazenado mas não enviado ao broker
3. Hostname pode ser usado em tópicos ou payloads
```

---

## 🧪 Validação

### Teste 1: Verificar Client ID
```bash
curl -s -u admin:admin http://192.168.68.106/api/mqtt/config | jq '.client_id'
```
**Resultado esperado:** `"C44F3375EF05"`

### Teste 2: Verificar MAC Address
```bash
curl -s -u admin:admin http://192.168.68.106/api/status | jq '.wifi.mac'
```
**Resultado esperado:** `"C4:4F:33:75:EF:05"`

### Teste 3: Verificar Match
```bash
# O Client ID deve ser o MAC sem os dois pontos
MAC=$(curl -s -u admin:admin http://192.168.68.106/api/status | jq -r '.wifi.mac' | tr -d ':')
CLIENT=$(curl -s -u admin:admin http://192.168.68.106/api/mqtt/config | jq -r '.client_id')
[ "$MAC" = "$CLIENT" ] && echo "✅ Match" || echo "❌ Mismatch"
```
**Resultado esperado:** `✅ Match`

---

## 💡 Casos de Uso do Hostname

### 1. Identificação Amigável
```
Hostname: "ESP32-Sensor-Sala"
Client ID: C44F3375EF05
```

### 2. Tópicos Personalizados
```javascript
String topic = String(config.hostname) + "/temperature";
// Resultado: "ESP32-Sensor-Sala/temperature"
```

### 3. Payloads MQTT
```json
{
  "device": "ESP32-Sensor-Sala",
  "client_id": "C44F3375EF05",
  "temperature": 25.5
}
```

---

## 🔐 Segurança

- **Client ID**: Único por dispositivo (baseado em MAC)
- **Hostname**: Configurável, mas não afeta autenticação
- Broker MQTT identifica cliente pelo Client ID
- Hostname é apenas um nome amigável para organização

---

## 📝 Logs do Serial

Ao carregar configuração:
```
MQTT: Configuration loaded
  Server: broker.hivemq.com:1883
  Username: (none)
  Hostname: ESP32-Sensor-01
  Main Topic: home/esp32
  Client ID: C44F3375EF05 (MAC-based)
  Enabled: Yes
```

Ao atualizar configuração:
```
MQTT: Configuration updated
  Hostname: ESP32-Sensor-01
  Client ID: C44F3375EF05 (MAC-based)
```

---

## ✅ Status

- [x] Client ID sempre baseado em MAC Address completo
- [x] Hostname configurável via interface web
- [x] API atualizada para suportar hostname
- [x] Interface web exibe Client ID como readonly
- [x] Documentação criada
- [x] Testado e validado

---

**Última Atualização:** 2025-11-21
**Autor:** Claude Code
**Versão:** 1.0
