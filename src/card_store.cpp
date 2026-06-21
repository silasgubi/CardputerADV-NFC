#include "card_store.h"
#include <SD.h>
#include <SPI.h>

CardStore Store;

static SPIClass SD_SPI(HSPI);

static const char* protoName(NFCProto p) {
    switch (p) {
        case NFCProto::A: return "NFC-A";
        case NFCProto::B: return "NFC-B";
        case NFCProto::F: return "NFC-F";
        default:          return "Unknown";
    }
}

static NFCProto protoFromName(const String& s) {
    if (s == "NFC-A") return NFCProto::A;
    if (s == "NFC-B") return NFCProto::B;
    if (s == "NFC-F") return NFCProto::F;
    return NFCProto::None;
}

bool CardStore::begin() {
    SD_SPI.begin(SD_PIN_SCK, SD_PIN_MISO, SD_PIN_MOSI, SD_PIN_CS);
    _ready = SD.begin(SD_PIN_CS, SD_SPI, 25000000);
    if (_ready && !SD.exists(NFC_DIR)) {
        SD.mkdir(NFC_DIR);
    }
    return _ready;
}

bool CardStore::save(const CardInfo& card, const String& name) {
    if (!_ready) return false;

    String path = String(NFC_DIR) + "/" + name + ".nfc";
    File f = SD.open(path, FILE_WRITE);
    if (!f) return false;

    f.println("Filetype: NFC Cardputer");
    f.println("Version: 1");
    f.printf("Protocol: %s\n", protoName(card.proto));
    f.printf("Device type: %s\n", card.typeName.c_str());
    f.printf("UID: %s\n", card.uid.c_str());
    f.printf("UID size: %u\n", card.uid_size);
    if (card.proto == NFCProto::A) {
        f.printf("ATQA: %04X\n", card.atqa);
        f.printf("SAK: %02X\n", card.sak);
        f.printf("Type id: %u\n", card.type_id);
    }
    f.close();
    return true;
}

std::vector<String> CardStore::list() {
    std::vector<String> names;
    if (!_ready) return names;

    File dir = SD.open(NFC_DIR);
    if (!dir) return names;

    for (File e = dir.openNextFile(); e; e = dir.openNextFile()) {
        if (!e.isDirectory()) {
            String fn = String(e.name());
            int slash = fn.lastIndexOf('/');
            if (slash >= 0) fn = fn.substring(slash + 1);
            if (fn.endsWith(".nfc")) {
                names.push_back(fn.substring(0, fn.length() - 4));
            }
        }
        e.close();
    }
    dir.close();
    return names;
}

bool CardStore::load(const String& name, CardInfo& out) {
    if (!_ready) return false;

    String path = String(NFC_DIR) + "/" + name + ".nfc";
    File f = SD.open(path, FILE_READ);
    if (!f) return false;

    out.clear();
    while (f.available()) {
        String line = f.readStringUntil('\n');
        line.trim();
        int colon = line.indexOf(':');
        if (colon < 0) continue;
        String key = line.substring(0, colon);
        String val = line.substring(colon + 1);
        val.trim();

        if (key == "Protocol")         out.proto    = protoFromName(val);
        else if (key == "Device type") out.typeName = val;
        else if (key == "UID")         out.uid      = val;
        else if (key == "UID size")    out.uid_size = (uint8_t)val.toInt();
        else if (key == "ATQA")        out.atqa     = (uint16_t)strtol(val.c_str(), nullptr, 16);
        else if (key == "SAK")         out.sak      = (uint8_t)strtol(val.c_str(), nullptr, 16);
        else if (key == "Type id")     out.type_id  = (uint8_t)val.toInt();
    }
    f.close();

    // Parse UID hex bytes back into array
    out.uid_size = 0;
    char buf[64];
    out.uid.toCharArray(buf, sizeof(buf));
    char* tok = strtok(buf, " ");
    while (tok && out.uid_size < 10) {
        out.uid_bytes[out.uid_size++] = (uint8_t)strtol(tok, nullptr, 16);
        tok = strtok(nullptr, " ");
    }

    out.valid = (out.proto != NFCProto::None && out.uid.length() > 0);
    return out.valid;
}

bool CardStore::remove(const String& name) {
    if (!_ready) return false;
    String path = String(NFC_DIR) + "/" + name + ".nfc";
    return SD.remove(path);
}
