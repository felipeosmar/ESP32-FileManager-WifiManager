# OTA Environments Guide

Este projeto suporta duas configurações de OTA (Over-The-Air) para diferentes capacidades de flash.

## 📋 Comparação das Environments

| Característica | esp32dev (4MB) | esp32dev-2mb (2MB) |
|---------------|----------------|-------------------|
| **Flash Total** | 4MB | 2MB |
| **Partições OTA** | 2 (ota_0 + ota_1) | 0 (Apenas Factory/App0) |
| **Tamanho App** | 1.5MB cada | 1.2MB única |
| **Tamanho SPIFFS** | 960KB | 704KB |
| **Rollback Automático** | ✅ Sim | ❌ Não (Sem OTA) |
| **Otimização** | `-O2` (performance) | `-Os` (tamanho) |
| **Flash Clock** | 80MHz | 40MHz |

---

## 🔧 Environment: `esp32dev` (4MB Flash - Recomendado)

### Características:
- **Rollback automático**: Se o novo firmware falhar, o ESP32 automaticamente volta para a versão anterior
- **Dual partition**: Mantém duas versões do firmware simultaneamente
- **Maior segurança**: Ideal para dispositivos em produção

### Uso:
```bash
# Build
pio run -e esp32dev

# Upload via serial
pio run -e esp32dev -t upload

# Upload via OTA
pio run -e esp32dev -t upload --upload-port <IP_DO_ESP32>
```

### Tabela de Partições (`partitions.csv`):
```
nvs:      20KB  - Non-Volatile Storage
otadata:  8KB   - OTA data
app0:     1.5MB - OTA partition 0
app1:     1.5MB - OTA partition 1 (backup)
spiffs:   960KB - File system
```

### Processo de OTA com Rollback:
1. Upload do novo firmware → vai para `app1` (ou `app0` se atual é `app1`)
2. ESP32 reinicia e tenta executar o novo firmware
3. Se o firmware responder corretamente (web server ativo), marca como válido
4. Se o firmware falhar, ESP32 automaticamente volta para a partição anterior

---

## 💾 Environment: `esp32dev-2mb` (2MB Flash - Economia)

### Características:
- **SEM suporte a OTA**: Não há espaço suficiente para duas partições de app
- **Single partition**: Apenas uma cópia do firmware
- **Otimizado para tamanho**: Usa `-Os` para gerar binários menores
- **Atualização**: Apenas via cabo serial (USB)

### Uso:
```bash
# Build
pio run -e esp32dev-2mb

# Upload via serial
pio run -e esp32dev-2mb -t upload

# Upload via OTA
# NÃO SUPORTADO EM 2MB
```

### Tabela de Partições (`partitions_2mb.csv`):
```
nvs:      20KB  - Non-Volatile Storage
otadata:  8KB   - OTA data
phy_init: 4KB   - PHY initialization
app0:     1.2MB - Single OTA partition
spiffs:   704KB - File system
```

### Processo de Atualização (2MB):
1. Conecte o ESP32 via USB
2. Execute `pio run -e esp32dev-2mb -t upload`
3. O firmware será gravado diretamente na flash

---

## 🎯 Quando Usar Cada Environment?

### Use `esp32dev` (4MB) quando:
✅ Você tem placas ESP32 com 4MB+ de flash
✅ Dispositivos estão em produção ou locais remotos
✅ Segurança e confiabilidade são prioridades
✅ Você quer proteção contra firmware corrompido

### Use `esp32dev-2mb` (2MB) quando:
✅ Você tem placas ESP32 com apenas 2MB de flash
✅ Está em ambiente de desenvolvimento/teste
✅ Tem acesso físico fácil ao dispositivo
✅ Precisa economizar espaço em flash

---

## 🔍 Verificando o Modo Atual

### Via Serial Monitor:
No boot, você verá uma das mensagens:

**Modo com Rollback (4MB):**
```
First boot after OTA update detected
Web server responding successfully - marking partition valid
OTA update validated successfully - rollback cancelled
```

**Modo sem Rollback (2MB):**
```
OTA rollback protection: DISABLED (2MB flash mode)
```

### Via Web Interface:
Acesse `http://<IP_DO_ESP32>/api/health/status` e verifique:

```json
{
  "ota": {
    "rollback_enabled": true,      // false para 2MB
    "mode": "dual_partition"        // "single_partition" para 2MB
  }
}
```

---

## ⚠️ Avisos Importantes para Modo 2MB

1. **Teste extensivamente antes do OTA**: Sem rollback, um firmware com bugs pode "brickar" o dispositivo
2. **Tenha sempre acesso ao cabo serial**: Para recuperação em caso de falha
3. **Faça backup da versão funcional**: Mantenha o `.bin` da última versão estável
4. **Valide o firmware localmente primeiro**: Teste em uma placa antes de distribuir

---

## 🛠️ Recuperação de Firmware Falho (2MB)

Se um OTA falhar em modo 2MB:

### 1. Via Cabo Serial (esptool):
```bash
# Apagar flash
esptool.py --port /dev/ttyUSB0 erase_flash

# Reflash firmware completo
pio run -e esp32dev-2mb -t upload
```

### 2. Via PlatformIO:
```bash
pio run -e esp32dev-2mb -t erase
pio run -e esp32dev-2mb -t upload
```

---

## 📊 Comparação de Tamanhos Típicos

| Componente | Tamanho Aproximado |
|-----------|-------------------|
| Firmware base | ~800KB |
| Bibliotecas (AsyncWeb, JSON, MQTT) | ~300KB |
| OLED + Adafruit GFX | ~50KB |
| **Total Firmware** | **~1.15MB** |
| Web interface (HTML/CSS/JS) | ~100-200KB |
| Dados de usuário (logs, configs) | Variável |

**Conclusão**:
- **4MB**: Espaço confortável (1.5MB por app + 960KB SPIFFS)
- **2MB**: Apertado mas funcional (1.2MB app + 704KB SPIFFS)

---

## 🚀 Migração entre Environments

### De 4MB para 2MB:
⚠️ Requer reflash completo (partições diferentes)
```bash
pio run -e esp32dev-2mb -t erase
pio run -e esp32dev-2mb -t upload
```

### De 2MB para 4MB:
⚠️ Requer reflash completo (partições diferentes)
```bash
pio run -e esp32dev -t erase
pio run -e esp32dev -t upload
```

---

## 📝 Notas de Desenvolvimento

- **Build Flag**: A flag `-DOTA_NO_ROLLBACK` controla o comportamento do código
- **Código Condicional**: Use `#ifdef OTA_NO_ROLLBACK` para features específicas
- **Testes**: Sempre teste OTA em ambiente seguro primeiro

---

**Desenvolvido para ESP32 File Manager**
*Documentação atualizada: 2025*
