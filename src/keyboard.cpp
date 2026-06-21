#include "keyboard.h"

Keyboard KBD;

void Keyboard::begin() {
    _last = 0;
}

Key Keyboard::poll() {
    if (!M5Cardputer.Keyboard.isChange() || !M5Cardputer.Keyboard.isPressed()) {
        return Key::None;
    }

    auto st = M5Cardputer.Keyboard.keysState();

    // Enter
    if (st.enter) return Key::Enter;

    // Del/Backspace key (top-right, "<-") acts as Back / Cancel everywhere.
    // In text entry the handler treats it as backspace until the field is empty.
    if (st.del) return Key::Back;

    // Navigation via Fn + letter or direct arrow-like keys
    // Cardputer keyboard: arrow up = Fn+W, down = Fn+S, left = Fn+A, right = Fn+D
    if (st.fn) {
        if (!st.word.empty()) {
            switch (st.word[0]) {
                case ';': case ':': return Key::Up;
                case '.': case '>': return Key::Down;
                case ',': case '<': return Key::Left;
                case '/': case '?': return Key::Right;
            }
        }
        return Key::None;
    }

    // Printable character
    if (!st.word.empty()) {
        _last = st.word[0];
        return Key::Char;
    }

    return Key::None;
}

char Keyboard::lastChar() {
    return _last;
}
