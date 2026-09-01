#ifndef BANGLA_UNICODE_H
#define BANGLA_UNICODE_H

#include <string>
#include <vector>
#include <cstdint>

namespace bangla {

// Bengali Unicode Codepoints
namespace codepoint {
    // Independent Vowels
    constexpr char32_t VOWEL_A   = 0x0985; // অ
    constexpr char32_t VOWEL_AA  = 0x0986; // আ
    constexpr char32_t VOWEL_I   = 0x0987; // ই
    constexpr char32_t VOWEL_II  = 0x0988; // ঈ
    constexpr char32_t VOWEL_U   = 0x0989; // উ
    constexpr char32_t VOWEL_UU  = 0x098A; // ঊ
    constexpr char32_t VOWEL_RI  = 0x098B; // ঋ
    constexpr char32_t VOWEL_E   = 0x098F; // এ
    constexpr char32_t VOWEL_OI  = 0x0990; // ঐ
    constexpr char32_t VOWEL_O   = 0x0993; // ও
    constexpr char32_t VOWEL_OU  = 0x0994; // ঔ

    // Consonants
    constexpr char32_t CONSONANT_KA  = 0x0995; // ক
    constexpr char32_t CONSONANT_KHA = 0x0996; // খ
    constexpr char32_t CONSONANT_GA  = 0x0997; // গ
    constexpr char32_t CONSONANT_GHA = 0x0998; // ঘ
    constexpr char32_t CONSONANT_NGA = 0x0999; // ঙ
    constexpr char32_t CONSONANT_CA  = 0x099A; // চ
    constexpr char32_t CONSONANT_CHA = 0x099B; // ছ
    constexpr char32_t CONSONANT_JA  = 0x099C; // জ
    constexpr char32_t CONSONANT_JHA = 0x099D; // ঝ
    constexpr char32_t CONSONANT_NYA = 0x099E; // ঞ
    constexpr char32_t CONSONANT_TTA = 0x099F; // ট
    constexpr char32_t CONSONANT_TTHA= 0x09A0; // ঠ
    constexpr char32_t CONSONANT_DDA = 0x09A1; // ড
    constexpr char32_t CONSONANT_DDHA= 0x09A2; // ঢ
    constexpr char32_t CONSONANT_NNA = 0x09A3; // ণ
    constexpr char32_t CONSONANT_TA  = 0x09A4; // ত
    constexpr char32_t CONSONANT_THA = 0x09A5; // থ
    constexpr char32_t CONSONANT_DA  = 0x09A6; // দ
    constexpr char32_t CONSONANT_DHA = 0x09A7; // ধ
    constexpr char32_t CONSONANT_NA  = 0x09A8; // ন
    constexpr char32_t CONSONANT_PA  = 0x09AA; // প
    constexpr char32_t CONSONANT_PHA = 0x09AB; // ফ
    constexpr char32_t CONSONANT_BA  = 0x09AC; // ব
    constexpr char32_t CONSONANT_BHA = 0x09AD; // ভ
    constexpr char32_t CONSONANT_MA  = 0x09AE; // ম
    constexpr char32_t CONSONANT_YA  = 0x09AF; // য
    constexpr char32_t CONSONANT_RA  = 0x09B0; // র
    constexpr char32_t CONSONANT_LA  = 0x09B2; // ল
    constexpr char32_t CONSONANT_SHA = 0x09B6; // শ
    constexpr char32_t CONSONANT_SSA = 0x09B7; // ষ
    constexpr char32_t CONSONANT_SA  = 0x09B8; // স
    constexpr char32_t CONSONANT_HA  = 0x09B9; // হ
    constexpr char32_t CONSONANT_RRA = 0x09DC; // ড়
    constexpr char32_t CONSONANT_RHA = 0x09DD; // ঢ়
    constexpr char32_t CONSONANT_YYA = 0x09DF; // য়

    // Dependent Vowel Signs (Kar)
    constexpr char32_t KAR_AA  = 0x09BE; // া
    constexpr char32_t KAR_I   = 0x09BF; // ি
    constexpr char32_t KAR_II  = 0x09C0; // ী
    constexpr char32_t KAR_U   = 0x09C1; // ু
    constexpr char32_t KAR_UU  = 0x09C2; // ূ
    constexpr char32_t KAR_RI  = 0x09C3; // ৃ
    constexpr char32_t KAR_E   = 0x09C7; // ে
    constexpr char32_t KAR_OI  = 0x09C8; // ৈ
    constexpr char32_t KAR_O   = 0x09CB; // ো
    constexpr char32_t KAR_OU  = 0x09CC; // ৌ

    // Virama & Modifiers
    constexpr char32_t HASANT       = 0x09CD; // ্
    constexpr char32_t KHANDA_TA    = 0x09CE; // ৎ
    constexpr char32_t ANUSVARA     = 0x0982; // ং
    constexpr char32_t VISARGA      = 0x0983; // ঃ
    constexpr char32_t CHANDRABINDU = 0x0981; // ঁ

    // Punctuation & Controls
    constexpr char32_t DARI         = 0x0964; // ।
    constexpr char32_t DOUBLE_DARI  = 0x0965; // ॥
    constexpr char32_t ZWNJ         = 0x200C; // \u200C
    constexpr char32_t ZWJ          = 0x200D; // \u200D
}

// Structure representing an atomic Bengali Grapheme Cluster
struct GraphemeCluster {
    std::u32string codepoints;
    bool has_hasant;
    bool has_kar;
    bool has_modifier;
};

// Helper utilities for UTF-8 conversions and codepoint properties
class UnicodeUtils {
public:
    static std::string CodepointToUtf8(char32_t cp);
    static std::string Utf32ToUtf8(const std::u32string& u32str);
    static std::u32string Utf8ToUtf32(const std::string& utf8str);

    static bool IsBengali(char32_t cp);
    static bool IsVowel(char32_t cp);
    static bool IsConsonant(char32_t cp);
    static bool IsKar(char32_t cp);
    static bool IsModifier(char32_t cp);

    static char32_t VowelToKar(char32_t vowel);
    static char32_t KarToVowel(char32_t kar);

    // Grapheme cluster operations
    static std::vector<GraphemeCluster> SegmentGraphemes(const std::string& utf8_text);
    static std::string BackspaceGrapheme(const std::string& utf8_text);
    static bool IsValidBengaliSequence(const std::string& utf8_text);
    static bool HasDanglingHasant(const std::string& utf8_text);
};

} // namespace bangla

#endif // BANGLA_UNICODE_H
