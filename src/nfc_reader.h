#pragma once
#include <M5UnitUnified.h>
#include <M5UnitUnifiedNFC.h>
#include <Arduino.h>
#include <vector>

enum class NFCProto : uint8_t { None, A, B, F };

struct CardInfo {
    NFCProto proto    = NFCProto::None;
    String   uid;         // hex string
    String   typeName;    // e.g. "Mifare Classic 1K"
    uint16_t atqa     = 0;
    uint8_t  sak      = 0;
    uint8_t  uid_size = 0;
    uint8_t  uid_bytes[10]{};
    uint8_t  type_id  = 0;  // raw m5::nfc::a::Type enum value (for emulation)
    bool     valid    = false;

    void clear() { *this = CardInfo{}; }
};

class NFCReader {
public:
    bool begin();
    bool unitReady() const { return _ready; }
    bool scan(CardInfo& out);   // NFC-A scan; true when card found
    void deactivate();

    // ── Emulation (NFC-A) ──
    // Reconfigure the unit to emulation mode and start emulating 'card'.
    bool startEmulation(const CardInfo& card);
    // Call every loop while emulating; returns the current emulation state code
    // (0=None 1=Off 2=Idle 3=Ready 4=Active 5=Halt).
    uint8_t updateEmulation();
    // Stop emulation and return the unit to reader mode.
    void stopEmulation();
    bool emulating() const { return _emulating; }
    const char* lastEmuError() const { return _emuErr; }

    // ── NDEF Write ──
    // Detect a blank NTAG/Ultralight tag and write a URI NDEF record.
    // Returns false without error string if no tag is present (keep polling).
    // Returns false with lastWriteError() set on wrong type or write failure.
    bool writeNdefUrl(const String& url);
    const char* lastWriteError() const { return _writeErr; }

private:
    bool                _ready     = false;
    bool                _hasCard   = false;
    bool                _emulating = false;
    const char*         _emuErr    = "";
    const char*         _writeErr  = "";
    CardInfo            _cached{};
    m5::nfc::a::PICC    _cachedPicc{};
    uint8_t             _emuMem[1024]{};  // covers NTAG216 (924B) and smaller

    m5::unit::UnitUnified   Units;
    m5::unit::UnitNFC       unit;
    m5::nfc::NFCLayerA      nfc_a{unit};
    m5::nfc::EmulationLayerA emu_a{unit};
};

extern NFCReader NFC_Reader;
