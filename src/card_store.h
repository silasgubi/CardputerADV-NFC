#pragma once
#include <Arduino.h>
#include <vector>
#include "nfc_reader.h"

// microSD SPI pins for Cardputer ADV
#define SD_PIN_SCK  40
#define SD_PIN_MISO 39
#define SD_PIN_MOSI 14
#define SD_PIN_CS   12

#define NFC_DIR "/nfc_cards"

class CardStore {
public:
    bool begin();                       // mount SD on the Cardputer SPI bus
    bool ready() const { return _ready; }

    // Save a detected card as <name>.nfc under NFC_DIR. Returns true on success.
    bool save(const CardInfo& card, const String& name);

    // List saved .nfc file base-names (without extension/path)
    std::vector<String> list();

    // Load a card by base-name into 'out'
    bool load(const String& name, CardInfo& out);

    // Delete a saved card by base-name
    bool remove(const String& name);

private:
    bool _ready = false;
};

extern CardStore Store;
