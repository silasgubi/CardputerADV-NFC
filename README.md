# NFC Tool — M5Stack Cardputer ADV

Leitor, emulador e gravador de tags NFC para o **M5Stack Cardputer ADV** com o
módulo **Unit NFC U216 (ST25R3916)**. Lê cartões NFC-A, salva no SD card,
emula os cartões salvos e grava registros NDEF em tags em branco (Home Assistant,
etiquetas, cartões de visita digitais).

Roda como `.bin` standalone instalável pelo **Launcher 2.7.2** (bmorcelli).

---

## Hardware

| Componente | Detalhe |
|---|---|
| Placa | M5Stack Cardputer ADV (ESP32-S3FN8, 8 MB flash) |
| Display | ST7789V2 240×135 |
| Teclado | TCA8418 via I2C interno (addr 0x34) — única forma de interação |
| Módulo NFC | Unit NFC U216 (ST25R3916), I2C addr 0x50 |
| SD card | SPI: CS=G12, MOSI=G14, SCK=G40, MISO=G39 |

### Conexão do módulo NFC

Conecte o Unit NFC U216 na **Port.A (Grove)** do Cardputer:

| Unit NFC | Cardputer Port.A |
|---|---|
| SDA | G1 |
| SCL | G2 |
| 5V  | 5V |
| GND | GND |

Sem conflito de endereço I2C: teclado = 0x34, NFC = 0x50.

---

## Dependências

- **PlatformIO** (rodando sob **Python 3.10–3.13** — o 3.14 ainda não é suportado pelo PlatformIO)
- Plataforma `espressif32 @ ^6.7.0` (ESP-IDF 5.x via Arduino framework)
- Bibliotecas (resolvidas automaticamente pelo PlatformIO):
  - `m5stack/M5Cardputer`
  - `m5stack/M5Unit-NFC`

O board `m5stack-cardputer` é definido localmente em
[boards/m5stack-cardputer.json](boards/m5stack-cardputer.json) (variant
`m5stack_stamp_s3`, mesmo ESP32-S3FN8).

---

## Build

```bash
pio run
```

> ⚠️ Se o `pio` do sistema estiver sob Python 3.14, instale o PlatformIO num
> Python 3.13 e use o caminho explícito, ex:
> `C:\Python313-x64\Scripts\pio.exe run`

O binário é gerado em:

```
.pio/build/m5stack-cardputer/firmware.bin
```

---

## Instalação via Launcher 2.7.2

1. Copie `firmware.bin` para o SD card (ex.: `/downloads/nfc-tool.bin`)
2. Insira o SD no Cardputer
3. No Launcher, navegue até o arquivo e selecione **instalar/rodar**
4. Ao **resetar**, o Cardputer volta sozinho ao Launcher

A versão do build (data/hora) aparece no topo da tela para você confirmar que
instalou o binário correto.

---

## Uso

### Telas e navegação

| Tela | Ação |
|---|---|
| **Scan** | Aproxime um cartão → mostra UID, tipo, ATQA/SAK |
| | `S` = salvar &nbsp;·&nbsp; `B` = ver cartões salvos &nbsp;·&nbsp; `W` = gravar tag HA |
| **Nome** | Digite um nome &nbsp;·&nbsp; `Enter` = salvar &nbsp;·&nbsp; `Del` = apagar char / voltar |
| **Salvos** | `Fn+;` / `Fn+.` = navegar &nbsp;·&nbsp; `Enter` = abrir &nbsp;·&nbsp; `Del` = voltar |
| **Detalhe** | `E` = emular &nbsp;·&nbsp; `D` = apagar &nbsp;·&nbsp; `Del` = voltar |
| **Emular** | `Del` = parar e voltar |
| **Apagar?** | `Enter` = confirma &nbsp;·&nbsp; `Del` = cancela |
| **URL (NDEF)** | Digite URL &nbsp;·&nbsp; `Enter` = gravar na tag &nbsp;·&nbsp; `Del` = apagar char / voltar |
| **Gravando** | Aproxime tag em branco → grava automático &nbsp;·&nbsp; `Del` = cancelar |

### Teclas de navegação

| Tecla | Função |
|---|---|
| `Fn` + `;` | Cima |
| `Fn` + `.` | Baixo |
| `Fn` + `,` | Esquerda |
| `Fn` + `/` | Direita |
| `Enter` | Confirmar |
| `Del` (`⌫`, canto sup. direito) | Voltar / Cancelar / apagar caractere |

> O botão físico **G0** não é usado para ações (fica atrás e é difícil de
> pressionar). Tudo é feito pelo teclado.

---

## Formato dos arquivos `.nfc`

Salvos em `/nfc_cards/<nome>.nfc` no SD card (texto simples):

```
Filetype: NFC Cardputer
Version: 1
Protocol: NFC-A
Device type: MIFARE Classic 1K
UID: 04 AB CD EF
UID size: 4
ATQA: 0004
SAK: 08
Type id: 2
```

---

## Tipos de cartão suportados

| Tipo | Leitura | Emulação | Gravação NDEF |
|---|---|---|---|
| Mifare Classic 1K/4K (Bilhete Único, acesso predial) | ✅ UID/ATQA/SAK | ❌ | ❌ |
| Mifare Ultralight / NTAG2xx (crachás, tickets, tags em branco) | ✅ | ✅ | ✅ |
| NFC-A genérico (ISO 14443-A) | ✅ | — | — |
| 125 kHz RFID (EM4100, chaveirinhos de porta) | ❌ freq. errada | ❌ | ❌ |

## Gravar tags NDEF (Home Assistant e outros)

A função **Gravar tag HA** (`W` na tela Scan) permite programar tags NFC em branco com um
registro NDEF URI — ideal para automações do **Home Assistant**, etiquetas de produto,
cartões de visita digitais, etc.

### Fluxo para Home Assistant

1. No app HA: crie uma automação NFC → HA exibe um UUID (ex: `550e8400-e29b-41d4-a716-446655440000`)
2. No Cardputer: pressione `W` na tela Scan
3. A URL já vem pré-preenchida: `https://www.home-assistant.io/tag/`
4. Digite o UUID fornecido pelo HA (alnum + traços)
5. Pressione `Enter` → Cardputer entra em modo de gravação
6. Aproxime uma tag em branco — a gravação acontece automaticamente
7. Toque a tag com o celular → HA dispara a automação

### Tags compatíveis para gravação

| Tag | Memória | Preço médio |
|---|---|---|
| **NTAG213** | 144 bytes | ~R$ 1–3 |
| **NTAG215** | 504 bytes | ~R$ 2–4 |
| **NTAG216** | 888 bytes | ~R$ 3–5 |
| Mifare Ultralight | 64 bytes | ~R$ 1–2 |

Disponíveis no Mercado Livre em lotes. Para HA, o NTAG213 é suficiente.

> A URL pode ser qualquer texto — basta apagar o pré-preenchimento com `Del` e digitar a nova.
> Caracteres permitidos: letras, números, `-_./:?=&@+#%~`

---

### Limitações conhecidas

- **Emulação só funciona para Mifare Ultralight e NTAG2xx.** É uma limitação do
  ST25R3916 na biblioteca M5Unit-NFC: a função de emulação rejeita qualquer
  outro tipo. Portanto:
  - ❌ **Mifare Classic 1K/4K NÃO podem ser emulados** — isso inclui o
    **Bilhete Único** e a maioria dos cartões de **acesso predial**.
  - ✅ Ultralight / NTAG (crachás, tickets) podem ser emulados (UID + memória).
- **Leitura** funciona para todos os NFC-A (incluindo Classic): você consegue
  ler e salvar o UID/ATQA/SAK do Bilhete Único, só não consegue emulá-lo.
- A emulação replica **UID + tipo (ATQA/SAK)** + memória embarcada do UID. Não há
  dump completo de setores/páginas com conteúdo.
- Apenas **NFC-A** é suportado (NFC-B/F não implementados nesta versão).
- Mifare Plus SL3 tem problemas na versão I2C do módulo — evite.

---

## Estrutura do projeto

```
├── platformio.ini
├── boards/
│   └── m5stack-cardputer.json   # board custom (ESP32-S3FN8)
├── src/
│   ├── main.cpp                 # máquina de estados + UI
│   ├── keyboard.h / .cpp        # leitura do teclado TCA8418
│   ├── nfc_reader.h / .cpp      # leitura e emulação NFC-A (ST25R3916)
│   └── card_store.h / .cpp      # save/load .nfc no SD
└── README.md
```

---

## Notas técnicas

- **Detecção contínua:** após `detect()` o cartão entra em HALT; o loop usa
  `wakeup()` (WUPA) + `select()` com o PICC cacheado para manter o cartão
  visível enquanto encostado, em vez de repetir `detect()` (que usa REQA e não
  acorda cartões em HALT).
- **Modo emulação:** exige reconfigurar o unit (`cfg.emulation = true`) e chamar
  `unit.begin()` de novo; ao sair, reconfigura de volta para modo leitor.
- **Gravação NDEF:** fluxo `detect → identify → reactivate → ndefWrite → deactivate`.
  Para Mifare Ultralight puro (não NTAG), chama-se `mifareUltralightChangeFormatToNDEF()`
  antes de escrever. NTAG213/215/216 já vêm pré-formatados de fábrica.
  O prefixo de protocolo (`https://`, `http://`, etc.) é armazenado como código
  compacto no registro NDEF URI (RFC 5.2), não como texto literal.
