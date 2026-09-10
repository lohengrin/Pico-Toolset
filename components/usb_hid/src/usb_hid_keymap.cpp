#include "pico_toolset/usb_hid_keymap.h"

namespace pico_toolset {

namespace {

// US-layout symbol table, indexed by (usage_id - 0x1E); entries are
// {unshifted, shifted}. Zero = not a printable key.
struct KeySym {
    char plain;
    char shifted;
};

constexpr KeySym kSymbols[] = {
    {'a', 'A'}, {'b', 'B'}, {'c', 'C'}, {'d', 'D'}, {'e', 'E'}, {'f', 'F'}, {'g', 'G'},
    {'h', 'H'}, {'i', 'I'}, {'j', 'J'}, {'k', 'K'}, {'l', 'L'}, {'m', 'M'}, {'n', 'N'},
    {'o', 'O'}, {'p', 'P'}, {'q', 'Q'}, {'r', 'R'}, {'s', 'S'}, {'t', 'T'}, {'u', 'U'},
    {'v', 'V'}, {'w', 'W'}, {'x', 'X'}, {'y', 'Y'}, {'z', 'Z'}, {'1', '!'}, {'2', '@'},
    {'3', '#'}, {'4', '$'}, {'5', '%'}, {'6', '^'}, {'7', '&'}, {'8', '*'}, {'9', '('},
    {'0', ')'}, {'\n', '\n'}, {0x1B, 0x1B}, {0x08, 0x08}, {0x09, 0x09}, {' ', ' '},
    {'-', '_'}, {'=', '+'}, {'[', '{'}, {']', '}'}, {'\\', '|'}, {'#', '~'},
    {';', ':'}, {'\'', '"'}, {'`', '~'}, {',', '<'}, {'.', '>'}, {'/', '?'},
};

} // namespace

uint8_t hid_usage_to_ascii(uint8_t usage_id, uint8_t modifier_mask) {
    if (usage_id >= 0x1E && usage_id <= 0x4D) {
        const KeySym& s = kSymbols[usage_id - 0x1E];
        if (s.plain == 0) return 0;
        bool shift = (modifier_mask & (0x02 | 0x20)) != 0; // left shift | right shift
        return shift ? static_cast<uint8_t>(s.shifted) : static_cast<uint8_t>(s.plain);
    }
    return 0;
}

} // namespace pico_toolset