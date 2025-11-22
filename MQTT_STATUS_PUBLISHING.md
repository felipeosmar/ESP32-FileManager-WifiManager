# MQTT - Publicação Automática de Status

## 📋 Resumo

O ESP32 agora publica automaticamente informações completas de status do sistema via MQTT a cada 60 segundos.

---

## 🎯 Estrutura do Tópico

### Formato
```
{mainTopic}/{hostname}/status
```

### Exemplos
```
esp32/data/ESP32-Device/status
home/sensors/ESP32-Sensor-01/status
iot/devices/ESP32-Garage/status
```

**Componentes:**
- `mainTopic`: Tópico principal configurado (ex: "esp32/data")
- `hostname`: Nome do dispositivo configurado (ex: "ESP32-Device")
- `status`: Subtópico fixo para dados de status

---

## 📊 Payload JSON

### Estrutura Completa

```json
{
  "uptime": {
    "milliseconds": 1234567890,
    "formatted": "14d 6h 56m 7s"
  },
  "memory": {
    "heap": {
      "total": 327680,
      "free": 280732,
      "used": 46948,
      "usage_percent": 14.3
    },
    "psram": {
      "total": 0,
      "free": 0,
      "used": 0,
      "usage_percent": 0
    },
    "sketch": {
      "total": 1572864,
      "used": 1019277,
      "free": 553587,
      "usage_percent": 64.8
    }
  },
  "wifi": {
    "connected": true,
    "ssid": "MinhaRede",
    "rssi": -64,
    "ip": "192.168.68.106",
    "mac": "C4:4F:33:75:EF:05"
  },
  "spiffs": {
    "ready": true,
    "total_bytes": 983040,
    "used_bytes": 212992,
    "free_bytes": 770048,
    "usage_percent": 21.7
  },
  "cpu": {
    "frequency_mhz": 240,
    "chip_model": "ESP32-D0WDQ6"
  },
  "sensor": {
    "type": "AM2315",
    "temperature": 25.5,
    "humidity": 60.2,
    "timestamp": 1234567890
  }
}
```

### Campos Detalhados

#### **uptime**
- `milliseconds`: Tempo desde o boot em milissegundos
- `formatted`: Tempo formatado (dias, horas, minutos, segundos)

#### **memory.heap**
- `total`: Total de heap RAM disponível (bytes)
- `free`: Heap RAM livre (bytes)
- `used`: Heap RAM em uso (bytes)
- `usage_percent`: Percentual de uso

#### **memory.psram** (se disponível)
- Mesma estrutura do heap
- `total = 0` indica que PSRAM não está disponível

#### **memory.sketch**
- `total`: Espaço total para firmware (bytes)
- `used`: Espaço ocupado pelo firmware atual (bytes)
- `free`: Espaço livre para OTA updates (bytes)
- `usage_percent`: Percentual ocupado

#### **wifi**
- `connected`: Status da conexão WiFi
- `ssid`: Nome da rede conectada
- `rssi`: Força do sinal (dBm)
- `ip`: Endereço IP local
- `mac`: Endereço MAC do ESP32

#### **spiffs**
- `ready`: Sistema de arquivos está montado
- `total_bytes`: Capacidade total (bytes)
- `used_bytes`: Espaço ocupado (bytes)
- `free_bytes`: Espaço livre (bytes)
- `usage_percent`: Percentual ocupado

#### **cpu**
- `frequency_mhz`: Frequência do CPU (MHz)
- `chip_model`: Modelo do chip ESP32

#### **sensor** (opcional - apenas se sensor estiver disponível)
- `type`: Tipo de sensor (SHT20, SHT30, SHT40, AM2315)
- `temperature`: Temperatura em Celsius
- `humidity`: Umidade relativa (%)
- `timestamp`: Timestamp da última leitura (millis)

---

## ⚙️ Configuração

### Intervalo de Publicação

**Arquivo:** `src/main.cpp` (linha 49)
```cpp
const unsigned long STATUS_PUBLISH_INTERVAL = 60000; // 60 seconds
```

**Para alterar:**
```cpp
// Publicar a cada 5 minutos
const unsigned long STATUS_PUBLISH_INTERVAL = 300000;

// Publicar a cada 30 segundos
const unsigned long STATUS_PUBLISH_INTERVAL = 30000;
```

### Configurar Hostname e Tópico

**Via Interface Web:**
1. Acesse `http://192.168.68.106/mqtt`
2. Configure:
   - **Hostname do Dispositivo**: Nome amigável (ex: "ESP32-Sensor-Sala")
   - **Tópico Principal**: Base do tópico (ex: "home/sensors")
3. Salve a configuração

**Resultado:**
- Tópico completo: `home/sensors/ESP32-Sensor-Sala/status`
- Client ID (MAC): `C44F3375EF05`

---

## 🔄 Comportamento

### Quando Publica
1. ✅ MQTT está habilitado
2. ✅ ESP32 está conectado ao broker MQTT
3. ✅ Passou o intervalo configurado (60 segundos)

### Quando NÃO Publica
- ❌ MQTT está desabilitado
- ❌ ESP32 não está conectado ao broker
- ❌ Intervalo ainda não foi atingido

### Logs no Serial Monitor

**Publicação bem-sucedida:**
```
MQTT: Published to home/sensors/ESP32-Device/status: {"uptime":...}
MQTT: System status published
```

**Falha na publicação:**
```
MQTT: Failed to publish to home/sensors/ESP32-Device/status
MQTT: Failed to publish system status
```

---

## 🧪 Teste e Validação

### 1. Testar com MQTT Explorer

**Instalação:**
```bash
# Windows/Mac: Download de http://mqtt-explorer.com/
# Linux:
snap install mqtt-explorer
```

**Configuração:**
1. Conectar ao mesmo broker que o ESP32
2. Inscrever no tópico: `#` (todos os tópicos) ou `esp32/data/#`
3. Aguardar 60 segundos
4. Verificar mensagem em: `esp32/data/ESP32-Device/status`

### 2. Testar com Mosquitto Client

**Subscrever ao tópico:**
```bash
mosquitto_sub -h broker.hivemq.com -t "esp32/data/ESP32-Device/status" -v
```

**Resultado esperado (a cada 60s):**
```
esp32/data/ESP32-Device/status {"uptime":{"milliseconds":1234567890,...
```

### 3. Testar com Node-RED

**Flow de exemplo:**
```json
[
  {
    "id": "mqtt-in",
    "type": "mqtt in",
    "topic": "esp32/data/+/status",
    "broker": "broker-config",
    "name": "ESP32 Status"
  },
  {
    "id": "json-parse",
    "type": "json",
    "name": "Parse JSON"
  },
  {
    "id": "debug",
    "type": "debug",
    "name": "Show Status"
  }
]
```

---

## 📈 Casos de Uso

### 1. Monitoramento de Saúde do Sistema

**Dashboard:**
```javascript
// Verificar uso de memória
if (msg.payload.memory.heap.usage_percent > 80) {
  node.warn("Heap memory high: " + msg.payload.memory.heap.usage_percent + "%");
}

// Verificar WiFi
if (!msg.payload.wifi.connected) {
  node.error("WiFi disconnected!");
}
```

### 2. Alertas de Temperatura

```javascript
if (msg.payload.sensor) {
  if (msg.payload.sensor.temperature > 30) {
    // Enviar alerta
    node.send({topic: "alerts/temperature", payload: "High temperature!"});
  }
}
```

### 3. Estatísticas de Uptime

```javascript
// Registrar uptime em banco de dados
const uptime = msg.payload.uptime.milliseconds;
const device = msg.topic.split('/')[2]; // hostname
// Salvar no InfluxDB, MongoDB, etc.
```

### 4. Monitoramento de Armazenamento

```javascript
if (msg.payload.spiffs.usage_percent > 90) {
  node.warn("SPIFFS almost full: " + msg.payload.spiffs.usage_percent + "%");
}
```

---

## 🔧 Implementação Técnica

### Arquivos Modificados

**src/mqtt_manager.h:**
- Adicionado método `publishToSubtopic()`

**src/mqtt_manager.cpp:**
- Implementado `publishToSubtopic()` que constrói tópico: `mainTopic/hostname/subtopic`

**src/main.cpp:**
- Adicionada função `publishSystemStatus()`
- Adicionado timer de 60 segundos no `loop()`
- Geração de payload JSON completo

### Fluxo de Execução

```
loop() (a cada 10ms)
  ↓
Verifica se MQTT conectado?
  ↓ (sim)
Passou 60 segundos?
  ↓ (sim)
publishSystemStatus()
  ↓
Coleta dados do sistema
  ↓
Gera JSON payload
  ↓
publishToSubtopic("status", payload)
  ↓
publish(mainTopic/hostname/status, payload)
  ↓
mqttClient.publish()
  ↓
Broker MQTT recebe a mensagem
```

---

## 📝 Exemplos de Integração

### Home Assistant

**configuration.yaml:**
```yaml
mqtt:
  sensor:
    - name: "ESP32 Temperature"
      state_topic: "esp32/data/ESP32-Device/status"
      value_template: "{{ value_json.sensor.temperature }}"
      unit_of_measurement: "°C"

    - name: "ESP32 Humidity"
      state_topic: "esp32/data/ESP32-Device/status"
      value_template: "{{ value_json.sensor.humidity }}"
      unit_of_measurement: "%"

    - name: "ESP32 Heap Free"
      state_topic: "esp32/data/ESP32-Device/status"
      value_template: "{{ value_json.memory.heap.free }}"
      unit_of_measurement: "bytes"
```

### Node-RED Dashboard

```javascript
// Gauge para temperatura
msg.payload = msg.payload.sensor.temperature;
return msg;

// Graph para memória
msg.payload = {
  series: ["Heap", "Sketch"],
  data: [
    [msg.payload.memory.heap.usage_percent],
    [msg.payload.memory.sketch.usage_percent]
  ]
};
return msg;
```

### Grafana com InfluxDB

```javascript
// Telegraf MQTT Consumer
[[inputs.mqtt_consumer]]
  servers = ["tcp://broker.hivemq.com:1883"]
  topics = ["esp32/data/+/status"]
  data_format = "json"
  json_string_fields = ["wifi_ssid", "sensor_type"]
```

---

## 🛡️ Segurança

### Retained Messages

Por padrão, as mensagens de status **não** são retidas (`retained = false`).

**Para habilitar retenção:**

**Arquivo:** `src/main.cpp` (linha 400)
```cpp
// Antes
bool success = mqttManager.publishToSubtopic("status", payload.c_str(), false);

// Depois (com retenção)
bool success = mqttManager.publishToSubtopic("status", payload.c_str(), true);
```

**Vantagens da retenção:**
- Novos assinantes recebem o último status imediatamente
- Útil para dashboards que precisam de estado inicial

**Desvantagens:**
- Broker armazena a mensagem permanentemente
- Pode expor dados sensíveis

### QoS (Quality of Service)

Atualmente usa QoS 0 (padrão do PubSubClient).

**Para alterar QoS:**
```cpp
// Em mqtt_manager.cpp, função publish()
mqttClient.publish(topic, payload, retained);

// Para QoS 1 (garantia de entrega)
// Requer modificação no PubSubClient
```

---

## ✅ Checklist de Implementação

- [x] Método `publishToSubtopic()` implementado
- [x] Função `publishSystemStatus()` criada
- [x] Timer de 60 segundos adicionado ao loop
- [x] Payload JSON com dados completos
- [x] Inclusão de dados do sensor (quando disponível)
- [x] Logs informativos no Serial
- [x] Tópico baseado em hostname configurável
- [x] Documentação completa
- [x] Firmware compilado e testado

---

## 🔍 Troubleshooting

### Status não está sendo publicado

**Verificar:**
1. MQTT está habilitado? → `http://192.168.68.106/mqtt`
2. ESP32 está conectado ao broker? → Verificar status na interface
3. Intervalo de 60s passou? → Aguardar ou reiniciar ESP32
4. Broker está acessível? → Testar com mosquitto_pub

**Logs esperados:**
```
MQTT: Connected successfully
MQTT: Published to esp32/data/ESP32-Device/status: {...}
MQTT: System status published
```

### Mensagens com dados incompletos

**Verificar:**
1. Sensor configurado? → Dados do sensor só aparecem se sensor está disponível
2. PSRAM detectado? → Se não houver PSRAM, valores serão 0
3. SPIFFS montado? → Se falhar, campos SPIFFS podem estar ausentes

---

**Última Atualização:** 2025-11-21
**Versão:** 1.0
**Intervalo de Publicação:** 60 segundos
**Formato de Tópico:** `{mainTopic}/{hostname}/status`
