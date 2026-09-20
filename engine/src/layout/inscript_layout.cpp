#include "inscript_layout.h"

namespace bangla {

InscriptLayout::InscriptLayout() {
    // Normal key mappings (decoded from Google Input Tools layouts/bn_inscript.js)
    normal_map_u8_ = {
        {'`', "়"}, {'1', "১"}, {'2', "২"}, {'3', "৩"}, {'4', "৪"}, {'5', "৫"},
        {'6', "৬"}, {'7', "৭"}, {'8', "৮"}, {'9', "৯"}, {'0', "০"}, {'-', "-"}, {'=', "ৃ"},
        {'q', "ৌ"}, {'w', "ৈ"}, {'e', "া"}, {'r', "ী"}, {'t', "ূ"}, {'y', "ব"},
        {'u', "হ"}, {'i', "গ"}, {'o', "দ"}, {'p', "জ"}, {'[', "ড"}, {']', "়"}, {'\\', "ে"},
        {'a', "ো"}, {'s', "ে"}, {'d', "্"}, {'f', "ি"}, {'g', "ু"}, {'h', "প"},
        {'j', "র"}, {'k', "ক"}, {'l', "ত"}, {';', "চ"}, {'\'', "ট"},
        {'z', "ে"}, {'x', "ং"}, {'c', "ম"}, {'v', "ন"}, {'b', "ভ"}, {'n', "ল"},
        {'m', "স"}, {',', ","}, {'.', "."}, {'/', "য়"}
    };

    // Shift key mappings
    shift_map_u8_ = {
        {'~', "ৠ"}, {'!', "১"}, {'@', "২"}, {'#', "্র"}, {'$', "র্"}, {'%', "জ্ঞ"},
        {'^', "ত্র"}, {'&', "ক্ষ"}, {'*', "শ্র"}, {'(', "("}, {')', ")"}, {'_', "ঃ"}, {'+', "ঋ"},
        {'Q', "ঔ"}, {'W', "ঐ"}, {'E', "আ"}, {'R', "ঈ"}, {'T', "ঊ"}, {'Y', "ভ"},
        {'U', "ঙ"}, {'I', "ঘ"}, {'O', "ধ"}, {'P', "ঝ"}, {'{', "ঢ"}, {'}', "ঞ"}, {'|', "ঐ"},
        {'A', "ও"}, {'S', "এ"}, {'D', "অ"}, {'F', "ই"}, {'G', "উ"}, {'H', "ফ"},
        {'J', "ঢ়"}, {'K', "খ"}, {'L', "থ"}, {':', "ছ"}, {'\"', "ঠ"},
        {'Z', "ঋ"}, {'X', "ঁ"}, {'C', "ণ"}, {'V', "ন"}, {'B', "ষ"}, {'N', "শ"},
        {'M', "স"}, {'<', "ষ"}, {'>', "।"}, {'?', "য়"}
    };

    // UTF-16 wchar_t equivalents
    normal_map_w_ = {
        {L'`', L'\u09bc'}, {L'1', L'\u09e7'}, {L'2', L'\u09e8'}, {L'3', L'\u09e9'}, {L'4', L'\u09ea'}, {L'5', L'\u09eb'},
        {L'6', L'\u09ec'}, {L'7', L'\u09ed'}, {L'8', L'\u09ee'}, {L'9', L'\u09ef'}, {L'0', L'\u09e6'}, {L'-', L'-'}, {L'=', L'\u09c3'},
        {L'q', L'\u09cc'}, {L'w', L'\u09c8'}, {L'e', L'\u09be'}, {L'r', L'\u09c0'}, {L't', L'\u09c2'}, {L'y', L'\u09ac'},
        {L'u', L'\u09b9'}, {L'i', L'\u0997'}, {L'o', L'\u09a6'}, {L'p', L'\u099c'}, {L'[', L'\u09a1'}, {L']', L'\u09bc'}, {L'\\', L'\u09c7'},
        {L'a', L'\u09cb'}, {L's', L'\u09c7'}, {L'd', L'\u09cd'}, {L'f', L'\u09bf'}, {L'g', L'\u09c1'}, {L'h', L'\u09aa'},
        {L'j', L'\u09b0'}, {L'k', L'\u0995'}, {L'l', L'\u09a4'}, {L';', L'\u099a'}, {L'\'', L'\u099f'},
        {L'z', L'\u09c7'}, {L'x', L'\u0982'}, {L'c', L'\u09ae'}, {L'v', L'\u09a8'}, {L'b', L'\u09ad'}, {L'n', L'\u09b2'},
        {L'm', L'\u09b8'}, {L',', L','}, {L'.', L'.'}, {L'/', L'\u09df'}
    };

    shift_map_w_ = {
        {L'~', L'\u09e0'}, {L'!', L'\u09e7'}, {L'@', L'\u09e8'}, {L'#', L'\u09cd'}, {L'$', L'\u09b0'}, {L'%', L'\u099c'},
        {L'^', L'\u09a4'}, {L'&', L'\u0995'}, {L'*', L'\u09b6'}, {L'(', L'('}, {L')', L')'}, {L'_', L'\u0983'}, {L'+', L'\u098b'},
        {L'Q', L'\u0994'}, {L'W', L'\u0990'}, {L'E', L'\u0986'}, {L'R', L'\u0988'}, {L'T', L'\u098a'}, {L'Y', L'\u09ad'},
        {L'U', L'\u0999'}, {L'I', L'\u0998'}, {L'O', L'\u09a7'}, {L'P', L'\u099d'}, {L'{', L'\u09a2'}, {L'}', L'\u099e'}, {L'|', L'\u0990'},
        {L'A', L'\u0993'}, {L'S', L'\u098f'}, {L'D', L'\u0985'}, {L'F', L'\u0987'}, {L'G', L'\u0989'}, {L'H', L'\u09ab'},
        {L'J', L'\u09a2'}, {L'K', L'\u0996'}, {L'L', L'\u09a5'}, {L':', L'\u099b'}, {L'\"', L'\u09a0'},
        {L'Z', L'\u098b'}, {L'X', L'\u0981'}, {L'C', L'\u09a3'}, {L'V', L'\u09a8'}, {L'B', L'\u09b7'}, {L'N', L'\u09b6'},
        {L'M', L'\u09b8'}, {L'<', L'\u09b7'}, {L'>', L'\u0964'}, {L'?', L'\u09df'}
    };
}

std::string InscriptLayout::GetChar(char key, bool is_shift) const {
    if (is_shift) {
        auto it = shift_map_u8_.find(key);
        if (it != shift_map_u8_.end()) return it->second;
    } else {
        auto it = normal_map_u8_.find(key);
        if (it != normal_map_u8_.end()) return it->second;
    }
    return std::string(1, key);
}

wchar_t InscriptLayout::GetCharW(wchar_t key, bool is_shift) const {
    if (is_shift) {
        auto it = shift_map_w_.find(key);
        if (it != shift_map_w_.end()) return it->second;
    } else {
        auto it = normal_map_w_.find(key);
        if (it != normal_map_w_.end()) return it->second;
    }
    return key;
}

bool InscriptLayout::HasMapping(char key, bool is_shift) const {
    if (is_shift) {
        return shift_map_u8_.find(key) != shift_map_u8_.end();
    }
    return normal_map_u8_.find(key) != normal_map_u8_.end();
}

} // namespace bangla
