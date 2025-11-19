# Guia de Início Rápido - SPIFFS Edition

Este guia vai te ajudar a ter o ESP32 File Manager funcionando em menos de 5 minutos!

## ⚡ Novidade: SEM SD CARD!

Este projeto agora usa **SPIFFS/LittleFS interno** - não precisa mais de SD Card!

## Checklist Pré-Requisitos

Antes de começar, certifique-se de ter:

- [ ] ESP32 (qualquer modelo)
- [ ] Cabo USB
- [ ] PlatformIO instalado (VSCode ou CLI)
- [ ] ❌ ~~SD Card~~ **NÃO PRECISA MAIS!**
- [ ] ❌ ~~Módulo SD~~ **NÃO PRECISA MAIS!**

## Passo 1: Clonar e Abrir o Projeto (30 segundos)

```bash
git clone <url-do-repositorio>
cd ESP32-FileManager-WifiManager
code .  # Abre no VSCode
```

## Passo 2: Upload do Filesystem (1 minuto)

**IMPORTANTE**: Faça isso PRIMEIRO!

### Via VSCode (Recomendado)
1. Pressione `Ctrl+Shift+P`
2. Digite `Upload File System image`
3. Pressione Enter
4. Aguarde finalizar

### Via Terminal
```bash
platformio run --target uploadfs
```

Você verá:
```
Building FS image...
Uploading...
Success!
```

## Passo 3: Upload do Firmware (1 minuto)

### Via VSCode
Clique no ícone `→` (Upload) na barra inferior do PlatformIO

### Via Terminal
```bash
platformio run --target upload
```

## Passo 4: Conectar e Testar (1 minuto)

### Opção A: Modo AP (Mais Fácil)

1. **Procure WiFi** "ESP32-FileManager" no seu celular/PC
2. **Conecte** usando senha: `12345678`
3. **Abra navegador**: `http://192.168.4.1`

### Opção B: Modo Station (Sua Rede)

Se quiser conectar à sua rede WiFi:

1. Acesse pelo modo AP primeiro (passos acima)
2. Vá em **File Manager**
3. Edite `/config.json`:
   ```json
   {
     "wifi": {
       "ssid": "SuaRede",
       "password": "SuaSenha",
       "ap_mode": false
     }
   }
   ```
4. Reinicie o ESP32
5. Veja o IP no Serial Monitor
6. Acesse `http://[IP]`

## Pronto! 🎉

Você deve ver a página inicial com 3 cards:

- 📁 **Gerenciador de Arquivos** - Gerencie arquivos na SPIFFS
- 🚀 **Atualização de Firmware** - OTA via web
- 🏥 **Monitor de Saúde** - Status do sistema

---

## Troubleshooting Ultra-Rápido

### ❌ "SPIFFS Mount Failed"

```bash
# Solução: Fazer upload do filesystem
platformio run --target uploadfs
```

### ❌ Interface web não aparece

```bash
# 1. Confirme que fez uploadfs
platformio run --target uploadfs

# 2. Verifique no Serial Monitor
platformio device monitor
```

### ❌ WiFi não conecta (modo Station)

```
1. Volte para modo AP editando config.json
2. Verifique SSID e senha
3. ESP32 volta automaticamente para AP se falhar
```

### ❌ Página em branco

```
Provável: Esqueceu de fazer uploadfs
Solução: platformio run --target uploadfs
```

## Comandos Essenciais

```bash
# Upload filesystem (apenas quando mudar arquivos em data/)
platformio run --target uploadfs

# Upload firmware (código C++)
platformio run --target upload

# Upload tudo (filesystem + firmware)
platformio run --target uploadfs && platformio run --target upload

# Monitor serial
platformio device monitor

# Compilar sem upload
platformio run
```

## FAQ Relâmpago

**Q: Preciso fazer uploadfs toda vez?**
A: Não! Apenas quando alterar arquivos em `data/`

**Q: Quanto espaço tenho?**
A: 960 KB na SPIFFS (veja em /health)

**Q: E se deletar `/web/` sem querer?**
A: Execute `platformio run --target uploadfs` para restaurar

**Q: Posso usar ESP32-S2/S3/C3?**
A: Sim! Mude `board` no `platformio.ini`

**Q: Posso fazer OTA com este firmware?**
A: Sim! Acesse `/firmware` e faça upload do `.bin`

## Diferenças da Versão SD Card

| Aspecto | Versão Antiga (SD) | Nova (SPIFFS) |
|---------|-------------------|---------------|
| Hardware | ESP32 + SD Card | Apenas ESP32 |
| Espaço | ~GB | 960 KB |
| Confiabilidade | ⭐⭐⭐ | ⭐⭐⭐⭐⭐ |
| Velocidade | ⭐⭐⭐ | ⭐⭐⭐⭐ |
| Setup | 10 min | 5 min |

## Próximos Passos

1. ✅ Explore o **File Manager** - Faça upload de arquivos
2. ✅ Veja o **Health Monitor** - Status do sistema
3. ✅ Teste o **OTA** - Atualize o firmware via web
4. ✅ Customize `config.json` - Configure seu WiFi

## Workflow de Desenvolvimento

```bash
# Dia-a-dia (apenas código)
platformio run --target upload

# Mudou arquivos web (data/)
platformio run --target uploadfs
platformio run --target upload

# Atualização completa
platformio run --target uploadfs && platformio run --target upload
```

## Espaço na SPIFFS

```
Total: 960 KB
├── Interface Web: ~100 KB
└── Seus Arquivos: ~860 KB disponível
```

Monitore em: `/health` > SPIFFS section

## Dicas Pro

1. **Edite config.json via web** - Não precisa recompilar!
2. **Use File Manager** - Upload arquivos sem USB
3. **Monitore /health** - Veja uso de SPIFFS em tempo real
4. **OTA Updates** - Atualize firmware via WiFi

## Backup de Arquivos

### Download via interface web
1. Acesse `/filemanager`
2. Selecione arquivo
3. Clique em Download

### Backup completo da SPIFFS
```bash
esptool.py --port /dev/ttyUSB0 read_flash 0x310000 0xF0000 backup.bin
```

## Restaurar Arquivos

Se deletou algo importante:

```bash
# Restaura TODOS os arquivos de data/
platformio run --target uploadfs
```

## Performance Esperada

- **Upload de 10KB**: ~1 segundo
- **Download de 50KB**: <1 segundo
- **Edição inline**: Instantâneo
- **OTA 1.5MB**: ~30 segundos

## Suporte

Se algo não funcionar:

1. ✅ Veja este guia de novo
2. ✅ Execute `uploadfs` e `upload` novamente
3. ✅ Verifique Serial Monitor: `platformio device monitor`
4. ✅ Confira a documentação completa: `README.md`

---

**Boa sorte!** 🚀

Se funcionou em 5 minutos, você está pronto para usar! Se não, 99% dos problemas são resolvidos com `platformio run --target uploadfs`.

**Dica Final**: Salve `config.json` customizado antes de executar `uploadfs`, pois ele sobrescreve tudo em `data/`!
