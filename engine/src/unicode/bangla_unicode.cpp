#include "bangla_unicode.h"
#include <algorithm>

namespace bangla {

using namespace codepoint;

std::string UnicodeUtils::CodepointToUtf8(char32_t cp) {
    std::string out;
    if (cp <= 0x7F) {
        out.push_back(static_cast<char>(cp));
    } else if (cp <= 0x7FF) {
        out.push_back(static_cast<char>(0xC0 | ((cp >> 6) & 0x1F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp <= 0xFFFF) {
        out.push_back(static_cast<char>(0xE0 | ((cp >> 12) & 0x0F)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp <= 0x10FFFF) {
        out.push_back(static_cast<char>(0xF0 | ((cp >> 18) & 0x07)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
    return out;
}

std::string UnicodeUtils::Utf32ToUtf8(const std::u32string& u32str) {
    std::string out;
    out.reserve(u32str.size() * 3);
    for (char32_t cp : u32str) {
        out.append(CodepointToUtf8(cp));
    }
    return out;
}

std::u32string UnicodeUtils::Utf8ToUtf32(const std::string& utf8str) {
    std::u32string out;
    size_t i = 0;
    size_t len = utf8str.size();

    while (i < len) {
        unsigned char c = static_cast<unsigned char>(utf8str[i]);
        char32_t cp = 0;
        size_t bytes = 0;

        if (c <= 0x7F) {
            cp = c;
            bytes = 1;
        } else if ((c & 0xE0) == 0xC0) {
            cp = c & 0x1F;
            bytes = 2;
        } else if ((c & 0xF0) == 0xE0) {
            cp = c & 0x0F;
            bytes = 3;
        } else if ((c & 0xF8) == 0xF0) {
            cp = c & 0x07;
            bytes = 4;
        } else {
            i++;
            continue;
        }

        if (i + bytes > len) break;

        for (size_t b = 1; b < bytes; b++) {
            unsigned char next = static_cast<unsigned char>(utf8str[i + b]);
            if ((next & 0xC0) != 0x80) {
                bytes = 0;
                break;
            }
            cp = (cp << 6) | (next & 0x3F);
        }

        if (bytes > 0) {
            out.push_back(cp);
            i += bytes;
        } else {
            i++;
        }
    }
    return out;
}

bool UnicodeUtils::IsBengali(char32_t cp) {
    return (cp >= 0x0980 && cp <= 0x09FF) || cp == codepoint::DARI || cp == codepoint::DOUBLE_DARI;
}

bool UnicodeUtils::IsVowel(char32_t cp) {
    return (cp >= codepoint::VOWEL_A && cp <= codepoint::VOWEL_OU);
}

bool UnicodeUtils::IsConsonant(char32_t cp) {
    return ((cp >= codepoint::CONSONANT_KA && cp <= codepoint::CONSONANT_HA) ||
            cp == codepoint::CONSONANT_RRA ||
            cp == codepoint::CONSONANT_RHA ||
            cp == codepoint::CONSONANT_YYA);
}

bool UnicodeUtils::IsKar(char32_t cp) {
    return (cp >= codepoint::KAR_AA && cp <= codepoint::KAR_OU);
}

bool UnicodeUtils::IsModifier(char32_t cp) {
    return (cp == codepoint::HASANT ||
            cp == codepoint::KHANDA_TA ||
            cp == codepoint::ANUSVARA ||
            cp == codepoint::VISARGA ||
            cp == codepoint::CHANDRABINDU);
}

char32_t UnicodeUtils::VowelToKar(char32_t vowel) {
    switch (vowel) {
        case codepoint::VOWEL_AA: return codepoint::KAR_AA;
        case codepoint::VOWEL_I:  return codepoint::KAR_I;
        case codepoint::VOWEL_II: return codepoint::KAR_II;
        case codepoint::VOWEL_U:  return codepoint::KAR_U;
        case codepoint::VOWEL_UU: return codepoint::KAR_UU;
        case codepoint::VOWEL_RI: return codepoint::KAR_RI;
        case codepoint::VOWEL_E:  return codepoint::KAR_E;
        case codepoint::VOWEL_OI: return codepoint::KAR_OI;
        case codepoint::VOWEL_O:  return codepoint::KAR_O;
        case codepoint::VOWEL_OU: return codepoint::KAR_OU;
        default: return 0;
    }
}

char32_t UnicodeUtils::KarToVowel(char32_t kar) {
    switch (kar) {
        case codepoint::KAR_AA: return codepoint::VOWEL_AA;
        case codepoint::KAR_I:  return codepoint::VOWEL_I;
        case codepoint::KAR_II: return codepoint::VOWEL_II;
        case codepoint::KAR_U:  return codepoint::VOWEL_U;
        case codepoint::KAR_UU: return codepoint::VOWEL_UU;
        case codepoint::KAR_RI: return codepoint::VOWEL_RI;
        case codepoint::KAR_E:  return codepoint::VOWEL_E;
        case codepoint::KAR_OI: return codepoint::VOWEL_OI;
        case codepoint::KAR_O:  return codepoint::VOWEL_O;
        case codepoint::KAR_OU: return codepoint::VOWEL_OU;
        default: return 0;
    }
}

std::vector<GraphemeCluster> UnicodeUtils::SegmentGraphemes(const std::string& utf8_text) {
    std::vector<GraphemeCluster> clusters;
    std::u32string u32 = Utf8ToUtf32(utf8_text);
    if (u32.empty()) return clusters;

    GraphemeCluster current;
    for (size_t i = 0; i < u32.size(); i++) {
        char32_t cp = u32[i];

        if (IsConsonant(cp) || IsVowel(cp) || cp == KHANDA_TA || !IsBengali(cp)) {
            // Check if this consonant is bound to preceding via Hasant
            if (!current.codepoints.empty() && current.codepoints.back() == HASANT) {
                current.codepoints.push_back(cp);
                current.has_hasant = true;
            } else {
                if (!current.codepoints.empty()) {
                    clusters.push_back(current);
                    current = GraphemeCluster{};
                }
                current.codepoints.push_back(cp);
            }
        } else if (cp == HASANT) {
            current.codepoints.push_back(cp);
            current.has_hasant = true;
        } else if (IsKar(cp)) {
            current.codepoints.push_back(cp);
            current.has_kar = true;
        } else if (IsModifier(cp) || cp == ZWNJ || cp == ZWJ) {
            current.codepoints.push_back(cp);
            current.has_modifier = true;
        } else {
            if (!current.codepoints.empty()) {
                clusters.push_back(current);
                current = GraphemeCluster{};
            }
            current.codepoints.push_back(cp);
        }
    }

    if (!current.codepoints.empty()) {
        clusters.push_back(current);
    }
    return clusters;
}

std::string UnicodeUtils::BackspaceGrapheme(const std::string& utf8_text) {
    std::u32string u32 = Utf8ToUtf32(utf8_text);
    if (u32.empty()) return "";

    char32_t last = u32.back();

    // 1. If trailing character is a Kar or modifier, delete it
    if (IsKar(last) || last == ANUSVARA || last == VISARGA || last == CHANDRABINDU) {
        u32.pop_back();
        return Utf32ToUtf8(u32);
    }

    // 2. If trailing character is a consonant preceded by a Hasant (conjunct node),
    // delete BOTH the consonant AND the Hasant to avoid leaving a broken dangling Hasant
    if (u32.size() >= 2 && IsConsonant(last) && u32[u32.size() - 2] == HASANT) {
        u32.pop_back(); // Pop consonant
        u32.pop_back(); // Pop Hasant
        return Utf32ToUtf8(u32);
    }

    // 3. If trailing character is a solitary Hasant, pop it
    if (last == HASANT) {
        u32.pop_back();
        return Utf32ToUtf8(u32);
    }

    // 4. Default: pop single base character
    u32.pop_back();
    return Utf32ToUtf8(u32);
}

bool UnicodeUtils::IsValidBengaliSequence(const std::string& utf8_text) {
    std::u32string u32 = Utf8ToUtf32(utf8_text);
    if (u32.empty()) return true;

    // Check no dangling Hasant at end of word
    if (u32.back() == HASANT) {
        return false;
    }

    for (size_t i = 0; i < u32.size(); i++) {
        char32_t cp = u32[i];
        // Cannot have double Hasant (্্)
        if (cp == HASANT && i + 1 < u32.size() && u32[i + 1] == HASANT) {
            return false;
        }
        // Kar cannot follow independent vowel directly without consonant
        if (IsKar(cp) && i > 0 && IsVowel(u32[i - 1])) {
            return false;
        }
    }
    return true;
}

bool UnicodeUtils::HasDanglingHasant(const std::string& utf8_text) {
    std::u32string u32 = Utf8ToUtf32(utf8_text);
    return !u32.empty() && u32.back() == HASANT;
}

} // namespace bangla
