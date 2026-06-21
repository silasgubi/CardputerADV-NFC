#include <M5Cardputer.h>
#include <vector>
#include "keyboard.h"
#include "nfc_reader.h"
#include "card_store.h"

static const uint16_t C_BG     = 0x0000;
static const uint16_t C_GREEN  = 0x07E0;
static const uint16_t C_WHITE  = 0xFFFF;
static const uint16_t C_GRAY   = 0x7BEF;
static const uint16_t C_DARK   = 0x39C7;
static const uint16_t C_CYAN   = 0x07FF;
static const uint16_t C_YELLOW = 0xFFE0;
static const uint16_t C_RED    = 0xF800;

enum class AppState : uint8_t { Scan, NameEntry, SaveResult, Browse, Detail, Emulate, ConfirmDelete, NdefEntry, NdefWrite, WriteResult };

static AppState state   = AppState::Scan;
static bool     nfcOk   = false;
static bool     sdOk    = false;

static CardInfo curCard{};       // currently shown card
static String   nameBuf;         // name being typed
static String   resultMsg;       // save result message
static bool     resultOk = false;

// browse / detail
static std::vector<String> savedList;
static int       browseSel = 0;     // selected index in savedList
static int       browseTop = 0;     // first visible row (scroll)
static CardInfo  detailCard{};      // loaded card for detail view
static String    detailName;        // base-name of loaded card
static const int BROWSE_ROWS = 6;   // visible rows in the list

static uint8_t   emuState = 0;      // last emulation state code
static bool      emuStarted = false;

// NDEF write
static String    urlBuf;            // URL being typed / to write
static bool      writeResultOk  = false;
static String    writeResultMsg;

// ─── header ──────────────────────────────────────────────────────────────────

static void drawHeader() {
    auto& d = M5Cardputer.Display;
    d.fillScreen(C_BG);
    d.setTextColor(C_GREEN);
    d.setTextSize(2);
    d.setCursor(8, 6);
    d.print("NFC Tool");

    d.setTextSize(1);
    d.setTextColor(C_DARK);
    d.setCursor(8, 26);
    d.print("Build: " __DATE__ " " __TIME__);
    d.drawFastHLine(0, 40, 240, C_GRAY);
}

// ─── SCAN screen ─────────────────────────────────────────────────────────────

static void drawScan() {
    auto& d = M5Cardputer.Display;
    drawHeader();

    d.setTextSize(1);
    d.setCursor(8, 44);
    d.setTextColor(nfcOk ? C_GRAY : C_RED);
    d.print(nfcOk ? "NFC: OK" : "NFC: ERRO");
    d.setCursor(80, 44);
    d.setTextColor(sdOk ? C_GRAY : C_RED);
    d.print(sdOk ? "SD: OK" : "SD: ERRO");

    if (!curCard.valid) {
        d.setTextColor(C_GRAY);
        d.setCursor(8, 64);
        d.print("Aproxime um cartao NFC...");
        d.setTextColor(C_DARK);
        d.setCursor(8, 120);
        d.print("[B] Salvos  [W] Gravar tag HA");
        return;
    }

    d.setTextColor(C_YELLOW);
    d.setCursor(8, 58);
    d.print("Cartao detectado");

    d.setTextColor(C_WHITE);
    d.setCursor(8, 72);
    d.print("UID: ");
    d.setTextColor(C_CYAN);
    d.print(curCard.uid);

    d.setTextColor(C_WHITE);
    d.setCursor(8, 84);
    d.print("Tipo: ");
    d.setTextColor(C_CYAN);
    String t = curCard.typeName;
    if (t.length() > 24) t = t.substring(0, 24);
    d.print(t);

    if (curCard.proto == NFCProto::A) {
        d.setTextColor(C_WHITE);
        d.setCursor(8, 96);
        d.printf("ATQA:%04X SAK:%02X", curCard.atqa, curCard.sak);
    }

    d.setTextColor(C_GREEN);
    d.setCursor(8, 120);
    d.print("[S] Salvar [B] Salvos [W] Tag HA");
}

// ─── NAME ENTRY screen ───────────────────────────────────────────────────────

static void drawNameEntry() {
    auto& d = M5Cardputer.Display;
    drawHeader();

    d.setTextColor(C_YELLOW);
    d.setTextSize(1);
    d.setCursor(8, 48);
    d.print("Nome do cartao:");

    // input box
    d.drawRect(8, 64, 224, 20, C_GRAY);
    d.setTextColor(C_CYAN);
    d.setCursor(14, 70);
    d.print(nameBuf);
    d.print("_");

    d.setTextColor(C_DARK);
    d.setCursor(8, 96);
    d.print("UID: " + curCard.uid);

    d.setTextColor(C_GREEN);
    d.setCursor(8, 120);
    d.print("[Enter] Salvar  [Del] Apagar/Voltar");
}

// ─── SAVE RESULT screen ──────────────────────────────────────────────────────

static void drawSaveResult() {
    auto& d = M5Cardputer.Display;
    drawHeader();

    d.setTextSize(2);
    d.setTextColor(resultOk ? C_GREEN : C_RED);
    d.setCursor(8, 60);
    d.print(resultOk ? "Salvo!" : "Erro!");

    d.setTextSize(1);
    d.setTextColor(C_WHITE);
    d.setCursor(8, 90);
    d.print(resultMsg);

    d.setTextColor(C_DARK);
    d.setCursor(8, 120);
    d.print("Qualquer tecla para voltar");
}

// ─── BROWSE screen ───────────────────────────────────────────────────────────

static void drawBrowse() {
    auto& d = M5Cardputer.Display;
    drawHeader();

    d.setTextSize(1);
    d.setTextColor(C_YELLOW);
    d.setCursor(8, 44);
    d.printf("Cartoes salvos (%u)", (unsigned)savedList.size());

    if (savedList.empty()) {
        d.setTextColor(C_GRAY);
        d.setCursor(8, 64);
        d.print("Nenhum cartao salvo.");
        d.setTextColor(C_DARK);
        d.setCursor(8, 120);
        d.print("[Del] Voltar");
        return;
    }

    int y = 58;
    for (int i = browseTop; i < (int)savedList.size() && i < browseTop + BROWSE_ROWS; ++i) {
        bool sel = (i == browseSel);
        if (sel) {
            d.fillRect(4, y - 1, 232, 11, C_DARK);
        }
        d.setTextColor(sel ? C_WHITE : C_GRAY);
        d.setCursor(10, y);
        d.print(savedList[i]);
        y += 11;
    }

    d.setTextColor(C_DARK);
    d.setCursor(8, 122);
    d.print("Fn+;/.=nav Enter=abrir Del=voltar");
}

// ─── DETAIL screen ───────────────────────────────────────────────────────────

static void drawDetail() {
    auto& d = M5Cardputer.Display;
    drawHeader();

    d.setTextSize(1);
    d.setTextColor(C_YELLOW);
    d.setCursor(8, 44);
    d.print(detailName);

    d.setTextColor(C_WHITE);
    d.setCursor(8, 60);
    d.print("UID: ");
    d.setTextColor(C_CYAN);
    d.print(detailCard.uid);

    d.setTextColor(C_WHITE);
    d.setCursor(8, 72);
    d.print("Tipo: ");
    d.setTextColor(C_CYAN);
    String t = detailCard.typeName;
    if (t.length() > 24) t = t.substring(0, 24);
    d.print(t);

    if (detailCard.proto == NFCProto::A) {
        d.setTextColor(C_WHITE);
        d.setCursor(8, 84);
        d.printf("ATQA:%04X SAK:%02X", detailCard.atqa, detailCard.sak);
    }

    d.setTextColor(C_GREEN);
    d.setCursor(8, 108);
    d.print("[E] Emular");
    d.setTextColor(C_RED);
    d.setCursor(100, 108);
    d.print("[D] Apagar");
    d.setTextColor(C_DARK);
    d.setCursor(8, 122);
    d.print("[Del] Voltar");
}

// ─── EMULATE screen ──────────────────────────────────────────────────────────

static const char* emuStateName(uint8_t s) {
    switch (s) {
        case 1: return "Off";
        case 2: return "Idle (aguardando leitor)";
        case 3: return "Ready";
        case 4: return "Active (lendo!)";
        case 5: return "Halt";
        default: return "Iniciando...";
    }
}

static void drawEmulate() {
    auto& d = M5Cardputer.Display;
    drawHeader();

    d.setTextSize(1);
    d.setTextColor(C_YELLOW);
    d.setCursor(8, 44);
    d.print("Emulando: " + detailName);

    d.setTextColor(C_WHITE);
    d.setCursor(8, 60);
    d.print("UID: ");
    d.setTextColor(C_CYAN);
    d.print(detailCard.uid);

    d.setTextColor(C_WHITE);
    d.setCursor(8, 72);
    d.print("Tipo: ");
    d.setTextColor(C_CYAN);
    String t = detailCard.typeName;
    if (t.length() > 24) t = t.substring(0, 24);
    d.print(t);

    // state indicator
    uint16_t sc = (emuState == 4) ? C_GREEN : (emuState == 2 ? C_YELLOW : C_GRAY);
    d.setTextColor(C_WHITE);
    d.setCursor(8, 90);
    d.print("Estado: ");
    d.setTextColor(sc);
    d.print(emuStateName(emuState));

    if (!emuStarted) {
        d.setTextColor(C_RED);
        d.setCursor(8, 104);
        d.print("Falha: ");
        d.print(NFC_Reader.lastEmuError());
    }

    d.setTextColor(C_DARK);
    d.setCursor(8, 122);
    d.print("[Del] Parar e voltar");
}

// ─── CONFIRM DELETE screen ───────────────────────────────────────────────────

static void drawConfirmDelete() {
    auto& d = M5Cardputer.Display;
    drawHeader();

    d.setTextSize(1);
    d.setTextColor(C_RED);
    d.setCursor(8, 50);
    d.print("Apagar este cartao?");

    d.setTextColor(C_WHITE);
    d.setCursor(8, 68);
    d.print(detailName);

    d.setTextColor(C_GREEN);
    d.setCursor(8, 100);
    d.print("[Enter] Sim");
    d.setTextColor(C_GRAY);
    d.setCursor(120, 100);
    d.print("[Del] Nao");
}

// ─── NDEF ENTRY screen ───────────────────────────────────────────────────────

static void drawNdefEntry() {
    auto& d = M5Cardputer.Display;
    drawHeader();

    d.setTextSize(1);
    d.setTextColor(C_YELLOW);
    d.setCursor(8, 44);
    d.print("URL para gravar na tag NDEF:");

    // Show last 74 chars of URL split across 2 lines (cursor at end)
    String shown = urlBuf;
    if ((int)shown.length() > 74) shown = shown.substring(shown.length() - 74);
    String ln1 = shown.substring(0, min(37, (int)shown.length()));
    String ln2 = shown.length() > 37 ? shown.substring(37) : "";

    d.drawRect(6, 56, 228, 11, C_GRAY);
    d.setTextColor(C_CYAN);
    d.setCursor(8, 58);
    d.print(ln1);

    d.drawRect(6, 70, 228, 11, C_GRAY);
    d.setCursor(8, 72);
    d.print(ln2);
    d.print("_");

    d.setTextColor(C_DARK);
    d.setCursor(8, 92);
    d.printf("(%u chars)", urlBuf.length());

    d.setTextColor(C_GREEN);
    d.setCursor(8, 120);
    d.print("[Enter] Gravar  [Del] Apagar/Voltar");
}

// ─── NDEF WRITE screen ───────────────────────────────────────────────────────

static void drawNdefWrite() {
    auto& d = M5Cardputer.Display;
    drawHeader();

    d.setTextSize(1);
    d.setTextColor(C_YELLOW);
    d.setCursor(8, 44);
    d.print("Aguardando tag em branco...");

    d.setTextColor(C_GRAY);
    d.setCursor(8, 62);
    d.print("Aproxime um NTAG213/215/216");
    d.setCursor(8, 74);
    d.print("ou Mifare Ultralight");

    d.setTextColor(C_DARK);
    d.setCursor(8, 92);
    // Show truncated URL preview
    String preview = urlBuf;
    if (preview.length() > 36) preview = preview.substring(0, 33) + "...";
    d.print(preview);

    d.setTextColor(C_DARK);
    d.setCursor(8, 120);
    d.print("[Del] Cancelar");
}

// ─── WRITE RESULT screen ─────────────────────────────────────────────────────

static void drawWriteResult() {
    auto& d = M5Cardputer.Display;
    drawHeader();

    d.setTextSize(2);
    d.setTextColor(writeResultOk ? C_GREEN : C_RED);
    d.setCursor(8, 52);
    d.print(writeResultOk ? "Gravado!" : "Erro!");

    d.setTextSize(1);
    d.setTextColor(C_WHITE);
    d.setCursor(8, 86);
    d.print(writeResultMsg);

    d.setTextColor(C_DARK);
    d.setCursor(8, 120);
    d.print("Qualquer tecla para voltar");
}

// ─── redraw dispatcher ───────────────────────────────────────────────────────

static void redraw() {
    switch (state) {
        case AppState::Scan:          drawScan();          break;
        case AppState::NameEntry:     drawNameEntry();     break;
        case AppState::SaveResult:    drawSaveResult();    break;
        case AppState::Browse:        drawBrowse();        break;
        case AppState::Detail:        drawDetail();        break;
        case AppState::Emulate:       drawEmulate();       break;
        case AppState::ConfirmDelete: drawConfirmDelete(); break;
        case AppState::NdefEntry:     drawNdefEntry();     break;
        case AppState::NdefWrite:     drawNdefWrite();     break;
        case AppState::WriteResult:   drawWriteResult();   break;
    }
}

// ─── browse helpers ──────────────────────────────────────────────────────────

static void openBrowse() {
    savedList = Store.list();
    browseSel = 0;
    browseTop = 0;
    state     = AppState::Browse;
    redraw();
}

// ─── setup ───────────────────────────────────────────────────────────────────

void setup() {
    auto cfg = M5.config();
    M5Cardputer.begin(cfg, true);
    KBD.begin();
    M5Cardputer.Display.setRotation(1);

    nfcOk = NFC_Reader.begin();
    sdOk  = Store.begin();

    redraw();
}

// ─── input handling per state ────────────────────────────────────────────────

static void handleScanKey(Key k) {
    if (k == Key::Char) {
        char c = KBD.lastChar();
        if (c == 'b' || c == 'B') {
            openBrowse();
            return;
        }
        if (curCard.valid && (c == 's' || c == 'S')) {
            nameBuf = "";
            state = AppState::NameEntry;
            redraw();
            return;
        }
        if (c == 'w' || c == 'W') {
            urlBuf = "https://www.home-assistant.io/tag/";
            state  = AppState::NdefEntry;
            redraw();
        }
    }
}

static void handleBrowseKey(Key k) {
    if (savedList.empty()) {
        if (k == Key::Back) { state = AppState::Scan; redraw(); }
        return;
    }
    switch (k) {
        case Key::Up:
            if (browseSel > 0) {
                browseSel--;
                if (browseSel < browseTop) browseTop = browseSel;
                redraw();
            }
            break;
        case Key::Down:
            if (browseSel < (int)savedList.size() - 1) {
                browseSel++;
                if (browseSel >= browseTop + BROWSE_ROWS) browseTop = browseSel - BROWSE_ROWS + 1;
                redraw();
            }
            break;
        case Key::Enter:
            if (Store.load(savedList[browseSel], detailCard)) {
                detailName = savedList[browseSel];
                state = AppState::Detail;
                redraw();
            }
            break;
        case Key::Back:
            state = AppState::Scan;
            redraw();
            break;
        default: break;
    }
}

static void handleDetailKey(Key k) {
    if (k == Key::Back) {
        state = AppState::Browse;
        redraw();
        return;
    }
    if (k == Key::Char) {
        char c = KBD.lastChar();
        if (c == 'd' || c == 'D') {           // delete → confirm
            state = AppState::ConfirmDelete;
            redraw();
        } else if (c == 'e' || c == 'E') {    // emulate
            emuStarted = NFC_Reader.startEmulation(detailCard);
            emuState   = 0;
            state      = AppState::Emulate;
            redraw();
        }
    }
}

static void handleNameEntryKey(Key k) {
    switch (k) {
        case Key::Char:
            if (nameBuf.length() < 24) {
                char c = KBD.lastChar();
                // allow alnum, dash, underscore
                if (isalnum(c) || c == '-' || c == '_') {
                    nameBuf += c;
                    redraw();
                }
            }
            break;
        case Key::Back:
            // Backspace while typing; cancel back to Scan when empty
            if (nameBuf.length() > 0) {
                nameBuf.remove(nameBuf.length() - 1);
            } else {
                state = AppState::Scan;
            }
            redraw();
            break;
        case Key::Enter:
            if (nameBuf.length() > 0) {
                resultOk  = Store.save(curCard, nameBuf);
                resultMsg = resultOk ? (nameBuf + ".nfc") : "Falha ao gravar no SD";
                state     = AppState::SaveResult;
                redraw();
            }
            break;
        default: break;
    }
}

static void handleSaveResultKey(Key k) {
    if (k != Key::None) {
        state = AppState::Scan;
        redraw();
    }
}

static void handleEmulateKey(Key k) {
    if (k == Key::Back) {
        NFC_Reader.stopEmulation();
        state = AppState::Detail;
        redraw();
    }
}

static void handleNdefEntryKey(Key k) {
    switch (k) {
        case Key::Char: {
            if (urlBuf.length() < 100) {
                char c = KBD.lastChar();
                // allow alnum + common URL characters (typed without Fn key)
                if (isalnum(c) || strchr("-_./:?=&@+#%~", c)) {
                    urlBuf += c;
                    redraw();
                }
            }
            break;
        }
        case Key::Back:
            if (urlBuf.length() > 0) {
                urlBuf.remove(urlBuf.length() - 1);
                redraw();
            } else {
                state = AppState::Scan;
                redraw();
            }
            break;
        case Key::Enter:
            if (urlBuf.length() > 0) {
                state = AppState::NdefWrite;
                redraw();
            }
            break;
        default: break;
    }
}

static void handleNdefWriteKey(Key k) {
    if (k == Key::Back) {
        state = AppState::NdefEntry;
        redraw();
    }
}

static void handleWriteResultKey(Key k) {
    if (k != Key::None) {
        state = AppState::Scan;
        redraw();
    }
}

static void handleConfirmDeleteKey(Key k) {
    if (k == Key::Enter) {
        Store.remove(detailName);
        openBrowse();
    } else if (k == Key::Back) {
        state = AppState::Detail;
        redraw();
    }
}

// ─── loop ────────────────────────────────────────────────────────────────────

void loop() {
    M5Cardputer.update();

    Key k = KBD.poll();
    if (k != Key::None) {
        switch (state) {
            case AppState::Scan:          handleScanKey(k);          break;
            case AppState::NameEntry:     handleNameEntryKey(k);     break;
            case AppState::SaveResult:    handleSaveResultKey(k);    break;
            case AppState::Browse:        handleBrowseKey(k);        break;
            case AppState::Detail:        handleDetailKey(k);        break;
            case AppState::Emulate:       handleEmulateKey(k);       break;
            case AppState::ConfirmDelete: handleConfirmDeleteKey(k); break;
            case AppState::NdefEntry:     handleNdefEntryKey(k);     break;
            case AppState::NdefWrite:     handleNdefWriteKey(k);     break;
            case AppState::WriteResult:   handleWriteResultKey(k);   break;
        }
    }

    // NDEF write: poll for blank tag every 300 ms
    if (state == AppState::NdefWrite) {
        static uint32_t lastWritePoll = 0;
        uint32_t now = millis();
        if (now - lastWritePoll >= 300) {
            lastWritePoll = now;
            if (NFC_Reader.writeNdefUrl(urlBuf)) {
                writeResultOk  = true;
                writeResultMsg = "Tag gravada com sucesso!";
                state = AppState::WriteResult;
                redraw();
            } else if (strlen(NFC_Reader.lastWriteError()) > 0) {
                writeResultOk  = false;
                writeResultMsg = String(NFC_Reader.lastWriteError());
                state = AppState::WriteResult;
                redraw();
            }
        }
        return;
    }

    // Emulation runs its own update loop
    if (state == AppState::Emulate) {
        if (emuStarted) {
            uint8_t s = NFC_Reader.updateEmulation();
            if (s != emuState) {
                emuState = s;
                redraw();
            }
        }
        return;
    }

    // NFC scanning only runs on the Scan screen
    if (state != AppState::Scan) return;

    static uint32_t lastScan   = 0;
    static uint32_t lastSeenAt = 0;
    const  uint32_t SCAN_MS    = 250;
    const  uint32_t TIMEOUT_MS = 1500;

    uint32_t now = millis();
    if (now - lastScan < SCAN_MS) return;
    lastScan = now;

    CardInfo found{};
    if (NFC_Reader.scan(found)) {
        lastSeenAt = now;
        if (found.uid != curCard.uid) {
            curCard = found;
            redraw();
        }
    } else if (curCard.valid && (now - lastSeenAt > TIMEOUT_MS)) {
        curCard.clear();
        redraw();
    }
}
