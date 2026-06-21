#pragma once
#include <M5Cardputer.h>

// Logical key events produced by poll()
enum class Key : uint8_t {
    None,
    Up,
    Down,
    Left,
    Right,
    Enter,
    Back,   // Del/Backspace key — Back / Cancel (or backspace while typing)
    Char,   // printable character — read with lastChar()
};

class Keyboard {
public:
    void begin();
    Key  poll();       // call once per loop(); returns event or Key::None
    char lastChar();   // valid when poll() returned Key::Char

private:
    char _last = 0;
};

extern Keyboard KBD;
