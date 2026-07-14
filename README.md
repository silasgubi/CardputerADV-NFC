# NFC Tool — M5Stack Cardputer ADV

![Platform](https://img.shields.io/badge/platform-ESP32--S3-blue)
![Framework](https://img.shields.io/badge/framework-Arduino%20%2F%20PlatformIO-orange)
![Board](https://img.shields.io/badge/board-M5Stack%20Cardputer%20ADV-red)
![NFC Chip](https://img.shields.io/badge/NFC-ST25R3916-brightgreen)
![License](https://img.shields.io/badge/license-MIT-lightgrey)

NFC tag reader, emulator, and writer for the **M5Stack Cardputer ADV** with the
**Unit NFC U216 (ST25R3916)** module. Reads NFC-A cards, saves them to the SD card,
emulates saved cards, and writes NDEF records to blank tags (Home Assistant,
labels, digital business cards).

Runs as a standalone `.bin` installable via **Launcher 2.7.2** (bmorcelli).

---

## Hardware

| Component | Detail |
|---|---|
| Board | M5Stack Cardputer ADV (ESP32-S3FN8, 8 MB flash) |
| Display | ST7789V2 240×135 |
| Keyboard | TCA8418 via internal I2C (addr 0x34) — the only means of interaction |
| NFC module | Unit NFC U216 (ST25R3916), I2C addr 0x50 |
| SD card | SPI: CS=G12, MOSI=G14, SCK=G40, MISO=G39 |

### NFC module wiring

Connect the Unit NFC U216 to the Cardputer's **Port.A (Grove)**:

| Unit NFC | Cardputer Port.A |
|---|---|
| SDA | G1 |
| SCL | G2 |
| 5V  | 5V |
| GND | GND |

No I2C address conflict: keyboard = 0x34, NFC = 0x50.

---

## Dependencies

- **PlatformIO** (running under **Python 3.10–3.13** — 3.14 isn't supported by PlatformIO yet)
- Platform `espressif32 @ ^6.7.0` (ESP-IDF 5.x via Arduino framework)
- Libraries (resolved automatically by PlatformIO):
  - `m5stack/M5Cardputer`
  - `m5stack/M5Unit-NFC`

The `m5stack-cardputer` board is defined locally in
[boards/m5stack-cardputer.json](boards/m5stack-cardputer.json) (variant
`m5stack_stamp_s3`, same ESP32-S3FN8).

---

## Build

```bash
pio run
```

> ⚠️ If your system's `pio` is running under Python 3.14, install PlatformIO on
> Python 3.13 and use the explicit path, e.g.:
> `C:\Python313-x64\Scripts\pio.exe run`

The binary is generated at:

```
.pio/build/m5stack-cardputer/firmware.bin
```

---

## Installation via Launcher 2.7.2

1. Copy `firmware.bin` to the SD card (e.g. `/downloads/nfc-tool.bin`)
2. Insert the SD card into the Cardputer
3. In the Launcher, navigate to the file and select **install/run**
4. On **reset**, the Cardputer returns to the Launcher on its own

The build version (date/time) is shown at the top of the screen so you can
confirm you installed the right binary.

---

## Usage

### Screens and navigation

| Screen | Action |
|---|---|
| **Scan** | Bring a card close → shows UID, type, ATQA/SAK |
| | `S` = save &nbsp;·&nbsp; `B` = view saved cards &nbsp;·&nbsp; `W` = write HA tag |
| **Name** | Type a name &nbsp;·&nbsp; `Enter` = save &nbsp;·&nbsp; `Del` = delete char / back |
| **Saved** | `Fn+;` / `Fn+.` = navigate &nbsp;·&nbsp; `Enter` = open &nbsp;·&nbsp; `Del` = back |
| **Detail** | `E` = emulate &nbsp;·&nbsp; `D` = delete &nbsp;·&nbsp; `Del` = back |
| **Emulate** | `Del` = stop and go back |
| **Delete?** | `Enter` = confirm &nbsp;·&nbsp; `Del` = cancel |
| **URL (NDEF)** | Type URL &nbsp;·&nbsp; `Enter` = write to tag &nbsp;·&nbsp; `Del` = delete char / back |
| **Writing** | Bring a blank tag close → writes automatically &nbsp;·&nbsp; `Del` = cancel |

### Navigation keys

| Key | Function |
|---|---|
| `Fn` + `;` | Up |
| `Fn` + `.` | Down |
| `Fn` + `,` | Left |
| `Fn` + `/` | Right |
| `Enter` | Confirm |
| `Del` (`⌫`, top-right corner) | Back / Cancel / delete character |

> The physical **G0** button is not used for actions (it's on the back and
> hard to press). Everything is done via the keyboard.

---

## `.nfc` file format

Saved to `/nfc_cards/<name>.nfc` on the SD card (plain text):

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

## Supported card types

| Type | Read | Emulate | NDEF write |
|---|---|---|---|
| Mifare Classic 1K/4K (transit cards, building access) | ✅ UID/ATQA/SAK | ❌ | ❌ |
| Mifare Ultralight / NTAG2xx (badges, tickets, blank tags) | ✅ | ✅ | ✅ |
| Generic NFC-A (ISO 14443-A) | ✅ | — | — |
| 125 kHz RFID (EM4100, door key fobs) | ❌ wrong frequency | ❌ | ❌ |

## Writing NDEF tags (Home Assistant and others)

The **Write HA tag** function (`W` on the Scan screen) lets you program blank NFC
tags with an NDEF URI record — ideal for **Home Assistant** automations, product
labels, digital business cards, etc.

### Home Assistant flow

1. In the HA app: create an NFC automation → HA displays a UUID (e.g. `550e8400-e29b-41d4-a716-446655440000`)
2. On the Cardputer: press `W` on the Scan screen
3. The URL is pre-filled: `https://www.home-assistant.io/tag/`
4. Type the UUID provided by HA (alnum + dashes)
5. Press `Enter` → the Cardputer enters write mode
6. Bring a blank tag close — writing happens automatically
7. Tap the tag with your phone → HA triggers the automation

### Compatible tags for writing

| Tag | Memory | Average price |
|---|---|---|
| **NTAG213** | 144 bytes | ~US$ 0.30–0.60 |
| **NTAG215** | 504 bytes | ~US$ 0.40–0.80 |
| **NTAG216** | 888 bytes | ~US$ 0.60–1.00 |
| Mifare Ultralight | 64 bytes | ~US$ 0.20–0.40 |

Available in bulk from online marketplaces. For HA, the NTAG213 is enough.

> The URL can be any text — just clear the pre-filled value with `Del` and type
> the new one. Allowed characters: letters, numbers, `-_./:?=&@+#%~`

---

### Known limitations

- **Emulation only works for Mifare Ultralight and NTAG2xx.** This is a
  limitation of the ST25R3916 in the M5Unit-NFC library: the emulation
  function rejects any other type. Therefore:
  - ❌ **Mifare Classic 1K/4K CANNOT be emulated** — this includes most
    **transit cards** and **building access** cards.
  - ✅ Ultralight / NTAG (badges, tickets) can be emulated (UID + memory).
- **Reading** works for all NFC-A cards (including Classic): you can read and
  save the UID/ATQA/SAK of a transit card, you just can't emulate it.
- Emulation replicates **UID + type (ATQA/SAK)** + the UID's onboard memory.
  There's no full sector/page content dump.
- Only **NFC-A** is supported (NFC-B/F not implemented in this version).
- Mifare Plus SL3 has issues with the module's I2C version — avoid it.

---

## Project structure

```
├── platformio.ini
├── boards/
│   └── m5stack-cardputer.json   # custom board (ESP32-S3FN8)
├── src/
│   ├── main.cpp                 # state machine + UI
│   ├── keyboard.h / .cpp        # TCA8418 keyboard reading
│   ├── nfc_reader.h / .cpp      # NFC-A reading and emulation (ST25R3916)
│   └── card_store.h / .cpp      # .nfc save/load on SD
└── README.md
```

---

## Technical notes

- **Continuous detection:** after `detect()` the card enters HALT; the loop
  uses `wakeup()` (WUPA) + `select()` with the cached PICC to keep the card
  visible while held in place, instead of repeating `detect()` (which uses
  REQA and doesn't wake cards in HALT).
- **Emulation mode:** requires reconfiguring the unit (`cfg.emulation = true`)
  and calling `unit.begin()` again; on exit, it reconfigures back to reader
  mode.
- **NDEF writing:** flow is `detect → identify → reactivate → ndefWrite → deactivate`.
  For plain Mifare Ultralight (not NTAG), `mifareUltralightChangeFormatToNDEF()`
  is called before writing. NTAG213/215/216 come pre-formatted from the
  factory. The protocol prefix (`https://`, `http://`, etc.) is stored as a
  compact code in the NDEF URI record (RFC 5.2), not as literal text.
