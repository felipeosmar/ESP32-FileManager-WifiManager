# Guia de Hardware - ESP32 File Manager

## Componentes Necessários

### Obrigatórios
- **ESP32 DevKit** (qualquer variante: ESP32-WROOM, ESP32-WROVER, NodeMCU-32S, etc.)
- **Módulo SD Card** (compatível com interface SD_MMC)
- **Cartão microSD** (formatado em FAT32, recomendado: 4GB a 32GB)
- **Cabos jumper** (macho-fêmea)
- **Cabo USB** (para programação e alimentação)

### Opcionais
- **Regulador de tensão 3.3V** (se usar fonte externa)
- **Capacitor 100µF** (para estabilizar alimentação do SD)
- **LEDs indicadores** (para status visual)

## Esquema de Conexão

### Modo SD_MMC 1-bit (Padrão do Projeto)

```
┌─────────────────────┐          ┌──────────────────┐
│     ESP32 DevKit    │          │  Módulo SD Card  │
│                     │          │                  │
│                     │          │                  │
│  GPIO 14 ──────────┼──────────┤ CLK              │
│  GPIO 15 ──────────┼──────────┤ CMD              │
│  GPIO 2  ──────────┼──────────┤ D0 (DAT0)        │
│                     │          │                  │
│  3.3V    ──────────┼──────────┤ VCC              │
│  GND     ──────────┼──────────┤ GND              │
│                     │          │                  │
└─────────────────────┘          └──────────────────┘
```

### Tabela de Conexões - Modo 1-bit

| Função SD | Pino ESP32 | GPIO | Descrição |
|-----------|------------|------|-----------|
| CLK       | GPIO 14    | 14   | Clock do SD Card |
| CMD       | GPIO 15    | 15   | Comando |
| D0 (DAT0) | GPIO 2     | 2    | Dados linha 0 |
| VCC       | 3.3V       | -    | Alimentação 3.3V |
| GND       | GND        | -    | Terra |

**Vantagens do modo 1-bit:**
- Usa apenas 3 GPIOs
- Compatível com a maioria das placas
- Deixa GPIO 4, 12 e 13 livres para outros usos

### Modo SD_MMC 4-bit (Alta Performance)

Se você precisa de melhor desempenho, pode usar o modo 4-bit:

```
┌─────────────────────┐          ┌──────────────────┐
│     ESP32 DevKit    │          │  Módulo SD Card  │
│                     │          │                  │
│  GPIO 14 ──────────┼──────────┤ CLK              │
│  GPIO 15 ──────────┼──────────┤ CMD              │
│  GPIO 2  ──────────┼──────────┤ D0 (DAT0)        │
│  GPIO 4  ──────────┼──────────┤ D1 (DAT1)        │
│  GPIO 12 ──────────┼──────────┤ D2 (DAT2)        │
│  GPIO 13 ──────────┼──────────┤ D3 (DAT3)        │
│                     │          │                  │
│  3.3V    ──────────┼──────────┤ VCC              │
│  GND     ──────────┼──────────┤ GND              │
│                     │          │                  │
└─────────────────────┘          └──────────────────┘
```

### Tabela de Conexões - Modo 4-bit

| Função SD | Pino ESP32 | GPIO | Descrição |
|-----------|------------|------|-----------|
| CLK       | GPIO 14    | 14   | Clock do SD Card |
| CMD       | GPIO 15    | 15   | Comando |
| D0 (DAT0) | GPIO 2     | 2    | Dados linha 0 |
| D1 (DAT1) | GPIO 4     | 4    | Dados linha 1 |
| D2 (DAT2) | GPIO 12    | 12   | Dados linha 2 |
| D3 (DAT3) | GPIO 13    | 13   | Dados linha 3 |
| VCC       | 3.3V       | -    | Alimentação 3.3V |
| GND       | GND        | -    | Terra |

**Para habilitar modo 4-bit**, edite `src/sd_manager.cpp` linha 13:
```cpp
if (!SD_MMC.begin("/sdcard", false)) {  // false = 4-bit mode
```

## GPIOs Disponíveis

Após conectar o SD Card em modo 1-bit, você ainda tem esses GPIOs livres:

### GPIOs de Entrada/Saída
- GPIO 4, 5, 12, 13, 16, 17, 18, 19, 21, 22, 23, 25, 26, 27, 32, 33

### GPIOs ADC (Leitura Analógica)
- GPIO 32, 33, 34, 35, 36, 39

### GPIOs com Pull-up/Pull-down
- Maioria dos GPIOs (exceto 34, 35, 36, 39 - apenas input)

### GPIOs a Evitar
- **GPIO 0**: Usado para boot (baixo = modo download)
- **GPIO 1/3**: UART0 TX/RX (comunicação serial)
- **GPIO 6-11**: Conectados à flash SPI (não usar!)
- **GPIO 34-39**: Apenas entrada (sem pull-up interno)

## Alimentação

### Via USB
- Conecte o cabo USB ao ESP32
- Fornece 5V que é regulado para 3.3V pela placa
- **Consumo típico**: 80-200mA (varia com WiFi ativo)

### Via Fonte Externa
- **Importante**: Forneça 3.3V regulado
- **Pino VIN**: Aceita 5V (regulador onboard)
- **Pino 3.3V**: Forneça 3.3V regulado (até 500mA)
- **Adicione capacitor 100µF** entre VCC e GND do SD

## Considerações Importantes

### 1. Nível de Tensão
- **ESP32 opera em 3.3V** - NUNCA use 5V nos pinos!
- Módulos SD Card geralmente são 3.3V
- Verifique o datasheet do seu módulo SD

### 2. Corrente
- ESP32 pode consumir até 500mA com WiFi ativo
- SD Card pode consumir 50-200mA durante escrita
- Use fonte adequada (mínimo 1A recomendado)

### 3. Comprimento dos Cabos
- **Cabos curtos** são essenciais para SD Card
- Máximo recomendado: 10-15cm
- Cabos longos causam problemas de comunicação

### 4. Resistores Pull-up
- A maioria dos módulos SD já tem pull-ups
- Se tiver problemas, adicione 10kΩ em CMD e DAT

### 5. Capacitor de Desacoplamento
- **Recomendado**: 100µF entre VCC e GND do módulo SD
- Ajuda a estabilizar a alimentação
- Reduz erros de leitura/escrita

## Testando o Hardware

### 1. Verificar Conexões
```
- Confira cada pino com multímetro
- VCC deve medir 3.3V
- GND deve ter continuidade
```

### 2. Teste Inicial
```cpp
// No Serial Monitor, você deve ver:
SD Card Type: SDHC
SD Card Size: [tamanho]MB
Used space: [usado]MB
Total space: [total]MB
```

### 3. Diagnóstico de Problemas

**SD Card não detectado:**
- Verifique alimentação (3.3V)
- Teste outro cartão SD
- Reduza comprimento dos cabos
- Formate o SD em FAT32

**Erros intermitentes:**
- Adicione capacitor 100µF
- Use cabos mais curtos
- Verifique qualidade dos jumpers

**Cartão muito lento:**
- Use cartão Class 10 ou superior
- Considere modo 4-bit
- Verifique se o SD está formatado corretamente

## Montagem Recomendada

### Opção 1: Protoboard
```
1. Fixe o ESP32 na protoboard
2. Conecte o módulo SD ao lado
3. Use trilhas para VCC e GND
4. Mantenha cabos organizados e curtos
```

### Opção 2: PCB Customizada
```
1. Crie layout com trilhas curtas
2. Adicione capacitores de desacoplamento
3. Use planos de GND
4. Considere proteção ESD
```

### Opção 3: Shield/HAT
```
1. Use shield ESP32 com slot SD integrado
2. Conexões já estão otimizadas
3. Normalmente inclui regulador e capacitores
```

## Expansões Futuras

### Adicionar Display
- I2C: GPIO 21 (SDA), GPIO 22 (SCL)
- SPI: GPIO 18 (CLK), GPIO 23 (MOSI), GPIO 19 (MISO)

### Adicionar Sensores
- DHT22: Qualquer GPIO disponível
- Sensores I2C: Compartilhar barramento com display

### Adicionar LEDs de Status
- LED WiFi: GPIO 2 (já usado pelo SD - use outro)
- LED Atividade: GPIO 4
- LED Erro: GPIO 5

## Referências de Pinagem

### ESP32 DevKit v1 (30 pinos)
```
                    ESP32 DevKit v1
                    ┌─────────────┐
                    │  [USB Port] │
                    └─────────────┘
EN                  │1          30│  GND
VP (GPIO36)         │2          29│  GPIO23
VN (GPIO39)         │3          28│  GPIO22
GPIO34              │4          27│  GPIO1 (TX)
GPIO35              │5          26│  GPIO3 (RX)
GPIO32              │6          25│  GPIO21
GPIO33              │7          24│  GND
GPIO25              │8          23│  GPIO19
GPIO26              │9          22│  GPIO18
GPIO27             │10          21│  GPIO5
GPIO14 (CLK)       │11          20│  GPIO17
GPIO12             │12          19│  GPIO16
GND                │13          18│  GPIO4
GPIO13             │14          17│  GPIO2 (D0)
D2 (GPIO9)         │15          16│  GPIO15 (CMD)
                    └──────────────┘
```

## Checklist de Montagem

- [ ] Verificar tensão VCC = 3.3V
- [ ] Verificar continuidade GND
- [ ] Cabos com menos de 15cm
- [ ] Cartão SD formatado em FAT32
- [ ] Arquivos copiados para o SD
- [ ] Conexões firmes (sem contato falso)
- [ ] Capacitor de desacoplamento (opcional)
- [ ] Teste de comunicação serial funcionando
- [ ] WiFi configurado corretamente

## Troubleshooting Hardware

### Problema: SD Card não monta
**Soluções:**
1. Verifique alimentação (multímetro)
2. Teste outro cartão SD
3. Reformate em FAT32
4. Reduza comprimento dos cabos

### Problema: WiFi não conecta
**Soluções:**
1. Verifique antena (se externa)
2. Teste outro SSID
3. Verifique senha no config.json
4. Reduza interferência (afaste de metal)

### Problema: Sistema trava
**Soluções:**
1. Adicione capacitor 100µF
2. Use fonte com mais corrente
3. Verifique aquecimento do ESP32
4. Teste com SD menor (menos consumo)

## Suporte

Para problemas de hardware:
1. Verifique todas as conexões
2. Use um multímetro para diagnóstico
3. Teste cada componente separadamente
4. Consulte o datasheet do seu módulo

**Boa montagem!** 🔧
