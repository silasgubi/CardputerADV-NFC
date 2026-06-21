// M5Cardputer.h MUST come first — provides M5Unified and the M5 global object
// required by the wiring helper.
#include <M5Cardputer.h>
#include <wiring/m5_unit_unified_wiring.hpp>
#include "nfc_reader.h"
#include <vector>

NFCReader NFC_Reader;

using namespace m5::nfc::a;

bool NFCReader::begin() {
    // Port.A = G1 (SDA) / G2 (SCL), shared with TCA8418 keyboard (addr 0x34)
    // NFC unit is at 0x50 — no address conflict
    bool ok = m5::unit::wiring::addI2C(Units, unit, 0,
                  m5::unit::wiring::NessoPort::PortA) && Units.begin();
    _ready = ok;
    return ok;
}

void NFCReader::deactivate() {
    nfc_a.deactivate();
    _hasCard = false;
    _cached.clear();
}

static void fillCardInfo(CardInfo& out, PICC& p) {
    out.proto    = NFCProto::A;
    out.atqa     = p.atqa;
    out.sak      = p.sak;
    out.uid_size = p.size;
    out.type_id  = (uint8_t)p.type;
    memcpy(out.uid_bytes, p.uid, p.size);

    out.uid = "";
    for (uint8_t i = 0; i < p.size; ++i) {
        if (i) out.uid += ' ';
        if (p.uid[i] < 0x10) out.uid += '0';
        out.uid += String(p.uid[i], HEX);
    }
    out.uid.toUpperCase();

    out.typeName = String(p.typeAsString().c_str());
    out.valid    = true;
}

bool NFCReader::scan(CardInfo& out) {
    out.clear();
    Units.update();

    // Card previously detected: WUPA -> select cached PICC -> HALT
    // Avoids depending on internal _activePICC which may be cleared after HALT
    if (_hasCard) {
        uint16_t atqa{};
        if (nfc_a.wakeup(atqa) && nfc_a.select(_cachedPicc)) {
            nfc_a.deactivate();  // back to HALT for next cycle
            out = _cached;
            return true;
        }
        _hasCard = false;
        _cached.clear();
    }

    // Full detect for new cards (REQA -> anti-collision -> SELECT -> HALT)
    std::vector<PICC> piccs;
    if (!nfc_a.detect(piccs) || piccs.empty()) return false;

    PICC& p = piccs[0];
    nfc_a.identify(p);
    fillCardInfo(out, p);
    _hasCard    = true;
    _cached     = out;
    _cachedPicc = p;
    return true;
}

// ─── Emulation ───────────────────────────────────────────────────────────────

static uint8_t bcc8(const uint8_t* p, uint8_t len, uint8_t init = 0) {
    uint8_t v = init;
    for (uint8_t i = 0; i < len; ++i) v ^= p[i];
    return v;
}

// Embed a 7-byte UID into the first 9 memory bytes (Ultralight/NTAG layout)
static void embedUid7(uint8_t mem[9], const uint8_t uid[7]) {
    memcpy(mem, uid, 3);
    mem[3] = bcc8(uid, 3, 0x88 /* cascade tag */);
    memcpy(mem + 4, uid + 3, 4);
    mem[8] = bcc8(uid + 3, 4);
}

bool NFCReader::startEmulation(const CardInfo& card) {
    _emuErr = "";
    if (!_ready) { _emuErr = "unit nao pronto"; return false; }
    if (card.proto != NFCProto::A) { _emuErr = "so NFC-A emula"; return false; }

    // Re-init the unit in emulation mode
    auto cfg      = unit.config();
    cfg.emulation = true;
    cfg.mode      = m5::nfc::NFC::A;
    unit.config(cfg);
    if (!unit.begin()) { _emuErr = "unit.begin falhou"; return false; }

    // Build emulated PICC from saved type + UID.
    // Older saves (or cards identify() couldn't classify) may have type Unknown —
    // fall back to deriving the type from SAK.
    PICC epicc{};
    auto type = (m5::nfc::a::Type)card.type_id;
    if (type == m5::nfc::a::Type::Unknown) {
        type = m5::nfc::a::sak_to_type(card.sak);
    }

    // The ST25R3916 library only emulates MIFARE Ultralight and NTAG2xx.
    // Mifare Classic (Bilhete Unico, acesso predial) cannot be emulated.
    using namespace m5::nfc::a;
    if (!(is_ntag2(type) || type == Type::MIFARE_Ultralight)) {
        _emuErr = "Classic nao emula (so Ultralight/NTAG)";
        stopEmulation();  // restore reader mode
        return false;
    }

    if (!epicc.emulate(type, card.uid_bytes, card.uid_size)) {
        _emuErr = "tipo/UID nao emulavel";
        stopEmulation();
        return false;
    }

    // Prepare memory: embed UID for 7-byte tags (Ultralight/NTAG)
    memset(_emuMem, 0, sizeof(_emuMem));
    if (card.uid_size == 7) {
        embedUid7(_emuMem, card.uid_bytes);
    } else if (card.uid_size == 4) {
        memcpy(_emuMem, card.uid_bytes, 4);
        _emuMem[4] = bcc8(card.uid_bytes, 4);
    }

    if (!emu_a.begin(epicc, _emuMem, sizeof(_emuMem))) {
        _emuErr = "emu_a.begin falhou";
        stopEmulation();
        return false;
    }

    _emulating = true;
    _hasCard   = false;
    return true;
}

uint8_t NFCReader::updateEmulation() {
    if (!_emulating) return 0;
    Units.update();
    emu_a.update();
    return (uint8_t)emu_a.state();
}

bool NFCReader::writeNdefUrl(const String& url) {
    using namespace m5::nfc::a;
    using namespace m5::nfc::ndef;

    _writeErr = "";
    if (!_ready)      { _writeErr = "unit nao pronto"; return false; }
    if (url.isEmpty()) { _writeErr = "URL vazia";       return false; }

    // Try to detect a tag; return false (no _writeErr) when none is present.
    PICC picc{};
    Units.update();
    if (!nfc_a.detect(picc)) return false;

    nfc_a.identify(picc);
    if (!nfc_a.reactivate(picc)) {
        _writeErr = "falha ao ativar tag";
        nfc_a.deactivate();
        return false;
    }

    if (!picc.supportsNDEF()) {
        _writeErr = "tag nao suporta NDEF (use NTAG/UL)";
        nfc_a.deactivate();
        return false;
    }

    // Ensure NDEF format for plain Ultralight (no-op for NTAG, which comes pre-formatted)
    if (picc.isMifareUltralight()) {
        nfc_a.mifareUltralightChangeFormatToNDEF();
    }

    // Strip protocol prefix so NDEF stores the compact identifier code
    URIProtocol proto = URIProtocol::NA;
    String payload    = url;
    if      (url.startsWith("https://www.")) { proto = URIProtocol::HTTPS_WWW; payload = url.substring(12); }
    else if (url.startsWith("https://"))     { proto = URIProtocol::HTTPS;     payload = url.substring(8);  }
    else if (url.startsWith("http://www."))  { proto = URIProtocol::HTTP_WWW;  payload = url.substring(11); }
    else if (url.startsWith("http://"))      { proto = URIProtocol::HTTP;       payload = url.substring(7);  }

    Record rec;
    rec.setURIPayload(payload.c_str(), proto);

    TLV msg(Tag::Message);
    msg.push_back(rec);

    bool ok = nfc_a.ndefWrite(msg);
    nfc_a.deactivate();

    if (!ok) { _writeErr = "falha ao gravar NDEF na tag"; return false; }
    return true;
}

void NFCReader::stopEmulation() {
    if (_emulating) emu_a.end();

    // Return the unit to reader mode (also used to recover from a failed start)
    auto cfg      = unit.config();
    cfg.emulation = false;
    cfg.mode      = m5::nfc::NFC::A;
    unit.config(cfg);
    unit.begin();

    _emulating = false;
}
