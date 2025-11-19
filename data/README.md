# Arquivos para o Cartão SD

Este diretório contém todos os arquivos que devem ser copiados para o cartão SD do ESP32.

## Estrutura no Cartão SD

Copie todo o conteúdo desta pasta para a **raiz** do cartão SD:

```
SD Card (raiz):
├── config.json          # Configuração WiFi
└── web/                 # Interface web
    ├── index.html       # Página inicial
    ├── style.css        # Estilos principais
    ├── app.js           # JavaScript principal
    ├── filemanager.html # Gerenciador de arquivos
    ├── filemanager.css
    ├── filemanager.js
    ├── firmware.html    # Atualização OTA
    ├── firmware.css
    ├── firmware.js
    ├── health.html      # Monitor de saúde
    ├── health.css
    └── health.js
```

## Preparação do Cartão SD

1. **Formatar o cartão SD em FAT32**
   - Windows: Botão direito no drive > Formatar > FAT32
   - macOS: Utilitário de Disco > Apagar > MS-DOS (FAT)
   - Linux: `sudo mkfs.vfat -F 32 /dev/sdX1`

2. **Copiar os arquivos**
   ```bash
   # No diretório do projeto
   cp -r data/* /media/SD_CARD/
   ```

   Ou manualmente:
   - Copie `config.json` para a raiz do SD
   - Crie a pasta `web/` na raiz do SD
   - Copie todos os arquivos de `data/web/` para `SD:/web/`

3. **Verificar a estrutura**
   - Certifique-se de que `config.json` está na raiz
   - Verifique se a pasta `web/` contém todos os 12 arquivos
   - Os arquivos devem estar diretamente em `SD:/web/`, não em subpastas

## Configuração WiFi (config.json)

### Modo Access Point (AP)
O ESP32 cria sua própria rede WiFi:

```json
{
  "wifi": {
    "ssid": "ESP32-FileManager",
    "password": "12345678",
    "ap_mode": true
  }
}
```

**Como conectar:**
1. Procure a rede WiFi "ESP32-FileManager"
2. Conecte usando a senha "12345678"
3. Acesse http://192.168.4.1/

### Modo Station (STA)
O ESP32 conecta-se à sua rede WiFi existente:

```json
{
  "wifi": {
    "ssid": "MinhaRedeWiFi",
    "password": "MinhaSenha",
    "ap_mode": false
  }
}
```

**Como conectar:**
1. O ESP32 conecta automaticamente à rede configurada
2. Veja o IP na serial monitor
3. Acesse http://[IP-EXIBIDO]/

## Personalização

### Alterar a Página Inicial
Edite `web/index.html` para personalizar a página inicial.

### Adicionar CSS Customizado
Edite `web/style.css` para alterar cores e estilos.

### Criar Páginas Adicionais
1. Crie novos arquivos HTML em `web/`
2. Adicione rotas no código (`src/main.cpp`)
3. Recompile e faça upload do firmware

## Backup

**IMPORTANTE**: Sempre faça backup do conteúdo do cartão SD antes de atualizar o firmware!

```bash
# Linux/macOS
cp -r /media/SD_CARD/ ~/backup-esp32-sd/

# Windows
# Copie a unidade inteira para uma pasta de backup
```

## Solução de Problemas

### Arquivos não carregam
- Verifique se os arquivos estão na estrutura correta
- Certifique-se de que o SD está formatado em FAT32
- Insira o SD antes de ligar o ESP32

### config.json não funciona
- Valide o JSON em https://jsonlint.com/
- Certifique-se de que não há espaços extras
- Use aspas duplas, não simples

### Interface web não aparece
- Verifique o Serial Monitor para erros
- Confirme que todos os 12 arquivos estão em `web/`
- Tente acessar diretamente: http://[IP]/filemanager

## Arquivos Opcionais

Você pode adicionar outros arquivos ao SD:
- Logs personalizados
- Configurações adicionais
- Dados de aplicação
- Backups de firmware (.bin)

O File Manager permite gerenciar todos esses arquivos pela interface web!
