# API REST - ESP32 File Manager

Documentação completa da API REST disponível no ESP32 File Manager.

## Base URL

```
http://<IP-DO-ESP32>
```

Exemplos:
- Modo AP: `http://192.168.4.1`
- Modo Station: `http://192.168.1.100` (varia conforme sua rede)

## Autenticação

Atualmente não há autenticação. **Importante**: Adicione autenticação antes de expor na internet!

## Endpoints Disponíveis

### Health & Status

#### GET /api/health/status

Retorna informações detalhadas sobre o sistema.

**Resposta de Sucesso:**
```json
{
  "uptime": {
    "milliseconds": 3600000,
    "formatted": "0d 1h 0m 0s"
  },
  "memory": {
    "heap": {
      "total": 327680,
      "free": 245000,
      "used": 82680,
      "usage_percent": 25.2
    },
    "psram": {
      "total": 4194304,
      "free": 4100000,
      "used": 94304,
      "usage_percent": 2.2
    }
  },
  "wifi": {
    "connected": true,
    "ssid": "MinhaRede",
    "rssi": -45,
    "signal_strength": "Excellent",
    "ip": "192.168.1.100",
    "mac": "24:0A:C4:XX:XX:XX",
    "channel": 6
  },
  "sd_card": {
    "ready": true,
    "card_size_mb": 15193,
    "total_mb": 15176,
    "used_mb": 1250,
    "free_mb": 13926,
    "usage_percent": 8.2,
    "type": "SDHC"
  },
  "cpu": {
    "frequency_mhz": 240,
    "cores": 2,
    "chip_model": "ESP32-D0WDQ6",
    "chip_revision": 1,
    "sdk_version": "v4.4.3"
  },
  "flash": {
    "size_mb": 4,
    "speed_mhz": 80
  },
  "ota": {
    "upload_in_progress": false
  },
  "status": "healthy",
  "timestamp": 3600000
}
```

**Exemplo com cURL:**
```bash
curl http://192.168.4.1/api/health/status
```

**Exemplo com Python:**
```python
import requests

response = requests.get('http://192.168.4.1/api/health/status')
data = response.json()
print(f"Uptime: {data['uptime']['formatted']}")
print(f"Free memory: {data['memory']['heap']['free']} bytes")
```

---

### File Manager

#### GET /api/files/list

Lista arquivos e diretórios.

**Parâmetros:**
- `dir` (opcional): Caminho do diretório (padrão: `/`)

**Exemplo de requisição:**
```
GET /api/files/list?dir=/web
```

**Resposta de Sucesso:**
```json
{
  "files": [
    {
      "name": "index.html",
      "size": 2034,
      "isDir": false
    },
    {
      "name": "style.css",
      "size": 8188,
      "isDir": false
    },
    {
      "name": "images",
      "size": 0,
      "isDir": true
    }
  ]
}
```

**Exemplos:**

cURL:
```bash
# Lista raiz
curl "http://192.168.4.1/api/files/list"

# Lista diretório específico
curl "http://192.168.4.1/api/files/list?dir=/web"
```

Python:
```python
import requests

# Listar arquivos na raiz
response = requests.get('http://192.168.4.1/api/files/list')
files = response.json()['files']

for file in files:
    type_str = "DIR" if file['isDir'] else "FILE"
    print(f"{type_str}: {file['name']} ({file['size']} bytes)")
```

JavaScript:
```javascript
fetch('/api/files/list?dir=/web')
  .then(response => response.json())
  .then(data => {
    data.files.forEach(file => {
      console.log(`${file.name}: ${file.size} bytes`);
    });
  });
```

---

#### GET /api/files/download

Baixa um arquivo.

**Parâmetros:**
- `file` (obrigatório): Caminho completo do arquivo

**Exemplo:**
```
GET /api/files/download?file=/web/index.html
```

**Resposta:**
- Conteúdo do arquivo (binary)
- Header `Content-Disposition: attachment`

**Exemplos:**

cURL:
```bash
# Download direto
curl "http://192.168.4.1/api/files/download?file=/config.json" -o config.json

# Download com progresso
curl -# "http://192.168.4.1/api/files/download?file=/backup.zip" -o backup.zip
```

Python:
```python
import requests

url = 'http://192.168.4.1/api/files/download?file=/config.json'
response = requests.get(url)

with open('config_backup.json', 'wb') as f:
    f.write(response.content)
```

---

#### GET /api/files/view

Visualiza conteúdo de um arquivo (sem forçar download).

**Parâmetros:**
- `file` (obrigatório): Caminho completo do arquivo

**Exemplo:**
```
GET /api/files/view?file=/config.json
```

**Resposta:**
- Conteúdo do arquivo (text/plain)

**Exemplos:**

cURL:
```bash
curl "http://192.168.4.1/api/files/view?file=/config.json"
```

---

#### GET /api/files/read

Lê conteúdo de arquivo para edição (JSON response, limite 50KB).

**Parâmetros:**
- `file` (obrigatório): Caminho completo do arquivo

**Exemplo:**
```
GET /api/files/read?file=/config.json
```

**Resposta de Sucesso:**
```json
{
  "status": "ok",
  "content": "{\n  \"wifi\": {\n    \"ssid\": \"ESP32\"\n  }\n}",
  "size": 45
}
```

**Erro (arquivo muito grande):**
```json
{
  "error": "File too large (max 50KB)"
}
```

**Exemplos:**

JavaScript:
```javascript
fetch('/api/files/read?file=/config.json')
  .then(response => response.json())
  .then(data => {
    if (data.status === 'ok') {
      console.log('File content:', data.content);
      console.log('File size:', data.size, 'bytes');
    }
  });
```

---

#### POST /api/files/write

Salva conteúdo em um arquivo.

**Parâmetros (form-data):**
- `file` (obrigatório): Caminho do arquivo
- `content` (obrigatório): Conteúdo a ser escrito

**Exemplo:**
```
POST /api/files/write
Content-Type: application/x-www-form-urlencoded

file=/test.txt&content=Hello World
```

**Resposta de Sucesso:**
```json
{
  "status": "ok",
  "written": 11
}
```

**Exemplos:**

cURL:
```bash
curl -X POST "http://192.168.4.1/api/files/write" \
  -d "file=/test.txt" \
  -d "content=Hello World from ESP32"
```

Python:
```python
import requests

data = {
    'file': '/notes.txt',
    'content': 'Minha anotação importante'
}

response = requests.post('http://192.168.4.1/api/files/write', data=data)
print(response.json())
```

---

#### POST /api/files/delete

Deleta um arquivo ou diretório.

**Parâmetros (form-data):**
- `file` (obrigatório): Caminho completo do arquivo/diretório

**Exemplo:**
```
POST /api/files/delete
Content-Type: application/x-www-form-urlencoded

file=/old_file.txt
```

**Resposta de Sucesso:**
```json
{
  "status": "ok"
}
```

**Exemplos:**

cURL:
```bash
curl -X POST "http://192.168.4.1/api/files/delete" \
  -d "file=/temp/old_file.txt"
```

Python:
```python
import requests

data = {'file': '/backup_old.zip'}
response = requests.post('http://192.168.4.1/api/files/delete', data=data)
print(response.json())
```

---

#### POST /api/files/upload

Faz upload de um arquivo.

**Parâmetros (multipart/form-data):**
- `file` (obrigatório): Arquivo binário
- `dir` (opcional): Diretório de destino (query string)

**Exemplo:**
```
POST /api/files/upload?dir=/uploads
Content-Type: multipart/form-data

[binary file data]
```

**Resposta de Sucesso:**
```json
{
  "status": "ok"
}
```

**Exemplos:**

cURL:
```bash
# Upload para raiz
curl -X POST "http://192.168.4.1/api/files/upload" \
  -F "file=@documento.pdf"

# Upload para diretório específico
curl -X POST "http://192.168.4.1/api/files/upload?dir=/documentos" \
  -F "file=@relatorio.pdf"
```

Python:
```python
import requests

files = {'file': open('image.jpg', 'rb')}
params = {'dir': '/images'}

response = requests.post(
    'http://192.168.4.1/api/files/upload',
    files=files,
    params=params
)
print(response.json())
```

JavaScript (HTML Form):
```html
<form id="uploadForm" enctype="multipart/form-data">
  <input type="file" name="file" id="fileInput">
  <button type="submit">Upload</button>
</form>

<script>
document.getElementById('uploadForm').addEventListener('submit', async (e) => {
  e.preventDefault();

  const formData = new FormData();
  formData.append('file', fileInput.files[0]);

  const response = await fetch('/api/files/upload?dir=/uploads', {
    method: 'POST',
    body: formData
  });

  const result = await response.json();
  console.log(result);
});
</script>
```

---

#### POST /api/files/mkdir

Cria um novo diretório.

**Parâmetros (form-data):**
- `dir` (obrigatório): Caminho completo do novo diretório

**Exemplo:**
```
POST /api/files/mkdir
Content-Type: application/x-www-form-urlencoded

dir=/new_folder
```

**Resposta de Sucesso:**
```json
{
  "status": "ok"
}
```

**Exemplos:**

cURL:
```bash
curl -X POST "http://192.168.4.1/api/files/mkdir" \
  -d "dir=/backups"
```

Python:
```python
import requests

data = {'dir': '/meus_arquivos/2024'}
response = requests.post('http://192.168.4.1/api/files/mkdir', data=data)
print(response.json())
```

---

### Firmware Update (OTA)

#### POST /api/firmware/upload

Atualiza o firmware via OTA.

**Parâmetros (multipart/form-data):**
- `file` (obrigatório): Arquivo .bin do firmware

**Exemplo:**
```
POST /api/firmware/upload
Content-Type: multipart/form-data

[binary firmware data]
```

**Resposta de Sucesso:**
```json
{
  "status": "ok",
  "message": "Firmware updated successfully. Device will reboot now."
}
```

**Resposta de Erro:**
```json
{
  "error": "Invalid ESP32 firmware file (magic byte check failed)"
}
```

**Exemplos:**

cURL:
```bash
curl -X POST "http://192.168.4.1/api/firmware/upload" \
  -F "file=@.pio/build/esp32dev/firmware.bin"
```

Python:
```python
import requests
import time

# Upload do firmware
files = {'file': open('firmware.bin', 'rb')}
response = requests.post('http://192.168.4.1/api/firmware/upload', files=files)

print(response.json())

if response.status_code == 200:
    print("Firmware enviado com sucesso!")
    print("ESP32 vai reiniciar em 2 segundos...")
    time.sleep(10)  # Aguarda reboot

    # Tenta conectar novamente
    for i in range(10):
        try:
            health = requests.get('http://192.168.4.1/api/health/status', timeout=2)
            print("ESP32 voltou online!")
            break
        except:
            time.sleep(2)
            print(f"Aguardando boot... ({i+1}/10)")
```

---

## Códigos de Status HTTP

| Código | Significado | Descrição |
|--------|-------------|-----------|
| 200    | OK          | Requisição bem-sucedida |
| 400    | Bad Request | Parâmetros inválidos ou ausentes |
| 404    | Not Found   | Arquivo ou endpoint não encontrado |
| 413    | Payload Too Large | Arquivo muito grande (>50KB para edição) |
| 500    | Internal Server Error | Erro interno do servidor |
| 503    | Service Unavailable | SD card não disponível ou OTA em progresso |

## Errors

Todas as respostas de erro seguem o formato:

```json
{
  "error": "Descrição do erro"
}
```

Exemplos:
```json
{
  "error": "SD card not ready"
}

{
  "error": "Missing file parameter"
}

{
  "error": "System busy - firmware update in progress"
}
```

## Rate Limiting

Não há rate limiting implementado. Use com moderação para não sobrecarregar o ESP32.

## Exemplos de Integração

### Script de Backup Automático (Python)

```python
import requests
import json
import os
from datetime import datetime

ESP32_IP = "192.168.4.1"
BACKUP_DIR = "./esp32_backup"

def backup_esp32():
    os.makedirs(BACKUP_DIR, exist_ok=True)

    # Lista todos os arquivos
    response = requests.get(f'http://{ESP32_IP}/api/files/list')
    files = response.json()['files']

    # Baixa cada arquivo
    for file in files:
        if not file['isDir']:
            print(f"Baixando {file['name']}...")

            file_url = f"http://{ESP32_IP}/api/files/download?file=/{file['name']}"
            file_data = requests.get(file_url).content

            backup_path = os.path.join(BACKUP_DIR, file['name'])
            with open(backup_path, 'wb') as f:
                f.write(file_data)

    print(f"Backup concluído! Arquivos salvos em {BACKUP_DIR}")

if __name__ == '__main__':
    backup_esp32()
```

### Monitor de Saúde (Node.js)

```javascript
const axios = require('axios');

const ESP32_IP = '192.168.4.1';

async function monitorHealth() {
  try {
    const response = await axios.get(`http://${ESP32_IP}/api/health/status`);
    const health = response.data;

    console.log(`\n=== ESP32 Health Monitor ===`);
    console.log(`Uptime: ${health.uptime.formatted}`);
    console.log(`Free Memory: ${health.memory.heap.free} bytes`);
    console.log(`WiFi RSSI: ${health.wifi.rssi} dBm (${health.wifi.signal_strength})`);
    console.log(`SD Card: ${health.sd_card.free_mb}MB free`);
    console.log(`Status: ${health.status}`);

    if (health.memory.heap.usage_percent > 80) {
      console.warn('⚠️  Warning: Memory usage is high!');
    }

  } catch (error) {
    console.error('❌ Failed to connect to ESP32:', error.message);
  }
}

// Monitor a cada 5 segundos
setInterval(monitorHealth, 5000);
monitorHealth();
```

## Segurança

**IMPORTANTE**: Esta API não possui autenticação por padrão!

### Recomendações de Segurança:

1. **Não exponha diretamente na internet**
2. **Use VPN** para acesso remoto
3. **Implemente autenticação** (Basic Auth, Bearer Token, etc.)
4. **Use HTTPS** em produção
5. **Valide todos os inputs**
6. **Limite o tamanho dos uploads**

## Suporte

Para questões sobre a API:
1. Verifique os logs no Serial Monitor
2. Teste com cURL primeiro
3. Valide o formato JSON das respostas
4. Consulte o código em `src/main.cpp`

---

**Documentação gerada para ESP32 File Manager v1.0**
