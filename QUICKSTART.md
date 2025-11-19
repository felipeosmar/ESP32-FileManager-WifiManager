# Guia de Início Rápido

Este guia vai te ajudar a ter o ESP32 File Manager funcionando em menos de 10 minutos!

## Checklist Pré-Requisitos

Antes de começar, certifique-se de ter:

- [ ] ESP32 (qualquer modelo)
- [ ] Módulo SD Card
- [ ] Cartão microSD formatado em FAT32
- [ ] 6 cabos jumper
- [ ] Cabo USB
- [ ] PlatformIO instalado (VSCode ou CLI)

## Passo 1: Hardware (5 minutos)

### Conexões Básicas

Conecte o módulo SD Card ao ESP32:

```
SD Card Module → ESP32
━━━━━━━━━━━━━━━━━━━━━━━━━
CLK  → GPIO 14
CMD  → GPIO 15
D0   → GPIO 2
VCC  → 3.3V
GND  → GND
```

**Dica**: Use cabos curtos (menos de 15cm) para evitar problemas!

## Passo 2: Preparar o SD Card (2 minutos)

### Formatar

1. **Windows**: Clique direito > Formatar > FAT32
2. **macOS**: Utilitário de Disco > Apagar > MS-DOS (FAT)
3. **Linux**: `sudo mkfs.vfat -F 32 /dev/sdX1`

### Copiar Arquivos

No diretório do projeto, copie os arquivos para o SD:

```bash
# Copie todo o conteúdo de data/ para a raiz do SD
cp -r data/* /media/SD_CARD/

# Ou manualmente:
# 1. Copie config.json para a raiz
# 2. Copie a pasta web/ inteira
```

Estrutura final no SD:
```
SD:/
├── config.json
└── web/
    ├── index.html
    ├── app.js
    └── ... (outros 10 arquivos)
```

## Passo 3: Configurar WiFi (1 minuto)

Edite o arquivo `config.json` no SD card:

### Opção A: Modo Access Point (Mais fácil)

```json
{
  "wifi": {
    "ssid": "ESP32-FileManager",
    "password": "12345678",
    "ap_mode": true
  }
}
```

### Opção B: Conectar à sua rede WiFi

```json
{
  "wifi": {
    "ssid": "SuaRedeWiFi",
    "password": "SuaSenha",
    "ap_mode": false
  }
}
```

**Salve o arquivo!**

## Passo 4: Upload do Firmware (2 minutos)

### Método A: VSCode + PlatformIO

1. Abra o projeto no VSCode
2. Conecte o ESP32 via USB
3. Clique em "Upload" na barra inferior (ícone →)
4. Aguarde a compilação e upload

### Método B: Terminal

```bash
# No diretório do projeto
platformio run --target upload
```

## Passo 5: Inserir o SD e Ligar (30 segundos)

1. **IMPORTANTE**: Desligue o ESP32
2. Insira o cartão SD no módulo
3. Ligue o ESP32 novamente

## Passo 6: Conectar e Testar (1 minuto)

### Se usou Modo AP:

1. Procure a rede WiFi "ESP32-FileManager" no seu celular/computador
2. Conecte usando a senha "12345678"
3. Abra o navegador em: `http://192.168.4.1`

### Se conectou à sua rede:

1. Abra o Serial Monitor: `platformio device monitor`
2. Veja o IP exibido (exemplo: 192.168.1.100)
3. Abra o navegador em: `http://[IP-EXIBIDO]`

## Passo 7: Verificar Funcionamento

Você deve ver a página inicial com 3 cards:

- 📁 Gerenciador de Arquivos
- 🚀 Atualização de Firmware
- 🏥 Monitor de Saúde

**Clique em cada um para testar!**

## Pronto! 🎉

Seu ESP32 File Manager está funcionando!

---

## Troubleshooting Rápido

### Problema: SD Card não reconhecido

**Soluções rápidas:**
```
1. Verifique as conexões (principalmente GND e VCC)
2. Certifique-se que o SD está em FAT32
3. Teste outro cartão SD
4. Use cabos mais curtos
```

### Problema: WiFi não aparece (Modo AP)

**Soluções rápidas:**
```
1. Verifique o Serial Monitor para erros
2. Aguarde até 30 segundos após ligar
3. Procure por "ESP32" nas redes WiFi próximas
4. Tente reiniciar o ESP32
```

### Problema: Não conecta à rede (Modo Station)

**Soluções rápidas:**
```
1. Verifique SSID e senha no config.json
2. Certifique-se de que não há espaços extras
3. Tente com senha mais simples (para testar)
4. Se falhar, o ESP32 volta automaticamente para modo AP
```

### Problema: Página não carrega

**Soluções rápidas:**
```
1. Verifique se os arquivos estão em SD:/web/
2. Desligue, remova e reinsira o SD
3. Verifique Serial Monitor para ver erros
4. Tente acessar diretamente: http://[IP]/filemanager
```

### Problema: Serial Monitor mostra erros

**Erros comuns e soluções:**

```
"SD Card Mount Failed"
→ Verifique conexões do SD
→ Reformate em FAT32
→ Teste outro cartão

"Failed to load config"
→ Confira que config.json está na raiz do SD
→ Valide o JSON em jsonlint.com

"WiFi failed to connect"
→ Verifique credenciais
→ Sistema vai automaticamente para modo AP
```

## Próximos Passos

Agora que está funcionando, explore:

1. **File Manager** - Faça upload de arquivos
2. **Health Monitor** - Veja o status do sistema
3. **Firmware Update** - Teste atualização OTA (use o .bin gerado)

## Documentação Completa

Para mais detalhes, consulte:

- `README.md` - Documentação completa
- `HARDWARE.md` - Esquemas detalhados de hardware
- `API.md` - Referência da API REST

## Comandos Úteis

```bash
# Compilar sem fazer upload
platformio run

# Upload via serial
platformio run --target upload

# Abrir monitor serial
platformio device monitor

# Compilar e monitorar
platformio run --target upload && platformio device monitor

# Limpar build
platformio run --target clean
```

## Customização Rápida

### Mudar nome da rede WiFi (AP Mode)

Edite `config.json`:
```json
{
  "wifi": {
    "ssid": "MeuESP32",  ← Mude aqui
    "password": "minhasenha123",
    "ap_mode": true
  }
}
```

### Mudar cor da interface

Edite `data/web/style.css` e altere as cores CSS.

### Adicionar sua logo

1. Coloque `logo.png` no SD em `/web/`
2. Edite `data/web/index.html` e adicione:
   ```html
   <img src="/web/logo.png" alt="Logo">
   ```

## Backup de Segurança

**IMPORTANTE**: Sempre faça backup do SD antes de experimentos!

```bash
# Linux/macOS
cp -r /media/SD_CARD ~/backup-esp32/

# Windows
# Copie manualmente todos os arquivos para uma pasta
```

## Suporte

Se precisar de ajuda:

1. ✅ Verifique este guia primeiro
2. ✅ Consulte a documentação completa
3. ✅ Veja o Serial Monitor para logs
4. ✅ Teste cada componente separadamente

---

**Boa sorte com seu ESP32 File Manager!** 🚀

Se funcionou de primeira, parabéns! Se teve problemas, não desista - a maioria é resolvida verificando as conexões e a formatação do SD.
