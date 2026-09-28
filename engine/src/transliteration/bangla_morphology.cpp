#include "bangla_morphology.h"
#include <algorithm>
#include <cctype>

namespace bangla {

namespace {

std::string ToLowerAscii(const std::string& str) {
    std::string out = str;
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return out;
}

// Recognized suffixes in lowercase
const char* const kKnownSuffixes[] = {
    "er", "r", "o", "ke", "re", "e", "te", "ta", "ti",
    "ra", "der", "gulo", "guli", "khana", "khani", "tuku", "tai", nullptr
};

} // namespace

bool BanglaMorphology::IsRecognizedSuffix(const std::string& roman_suffix) {
    std::string lower = ToLowerAscii(roman_suffix);
    for (size_t i = 0; kKnownSuffixes[i] != nullptr; ++i) {
        if (lower == kKnownSuffixes[i]) return true;
    }
    return false;
}

bool BanglaMorphology::DecomposeHyphenatedToken(const std::string& token,
                                                std::string& out_stem,
                                                std::string& out_suffix) {
    if (token.size() < 3) return false;

    // Find the last hyphen in the token
    size_t hyphen_pos = token.rfind('-');
    if (hyphen_pos == std::string::npos || hyphen_pos == 0 || hyphen_pos == token.size() - 1) {
        return false;
    }

    std::string stem = token.substr(0, hyphen_pos);
    std::string suffix = token.substr(hyphen_pos + 1);

    if (IsRecognizedSuffix(suffix)) {
        out_stem = stem;
        out_suffix = suffix;
        return true;
    }

    return false;
}

bool BanglaMorphology::DecomposeCompoundToken(const std::string& token,
                                             std::string& out_stem,
                                             std::string& out_suffix) {
    if (token.size() < 4) return false;

    // Check for common emphatic / conditional compound verb endings:
    // e.g. "korleo" -> stem "korle", suffix "o"
    //      "geleo"  -> stem "gele",  suffix "o"
    //      "peleo"  -> stem "pele",  suffix "o"
    std::string lower = ToLowerAscii(token);

    // Endings like "leo" -> stem ends with "le" + suffix "o"
    if (lower.size() >= 5 && lower.substr(lower.size() - 3) == "leo") {
        out_stem = token.substr(0, token.size() - 1); // e.g. "korle", "gele", "pele"
        out_suffix = token.substr(token.size() - 1);  // "o"
        return true;
    }

    return false;
}

bool BanglaMorphology::EndsInKar(const std::string& bengali_word) {
    if (bengali_word.size() < 3) return false;
    // Check last 3 bytes of UTF-8 string
    size_t len = bengali_word.size();
    unsigned char b0 = static_cast<unsigned char>(bengali_word[len - 3]);
    unsigned char b1 = static_cast<unsigned char>(bengali_word[len - 2]);
    unsigned char b2 = static_cast<unsigned char>(bengali_word[len - 1]);

    if (b0 == 0xE0) {
        // Kar range: 0x09BE (AA), 0x09BF (I), 0x09C0..0x09CC
        if (b1 == 0xA6 && (b2 == 0xBE || b2 == 0xBF)) return true;
        if (b1 == 0xA7 && b2 >= 0x80 && b2 <= 0x8C) return true;
    }
    return false;
}

bool BanglaMorphology::EndsInIndependentVowel(const std::string& bengali_word) {
    if (bengali_word.size() < 3) return false;
    size_t len = bengali_word.size();
    unsigned char b0 = static_cast<unsigned char>(bengali_word[len - 3]);
    unsigned char b1 = static_cast<unsigned char>(bengali_word[len - 2]);
    unsigned char b2 = static_cast<unsigned char>(bengali_word[len - 1]);

    if (b0 == 0xE0) {
        // Bengali independent vowels: 0x0985..0x0994
        // UTF-8: E0 A6 85 .. E0 A6 94
        if (b1 == 0xA6 && b2 >= 0x85 && b2 <= 0x94) return true;
    }
    return false;
}

std::string BanglaMorphology::AttachSuffix(const std::string& bengali_stem,
                                          const std::string& roman_suffix) {
    if (bengali_stem.empty()) return "";
    std::string s = ToLowerAscii(roman_suffix);

    const std::string e_kar = "\xE0\xA7\x87";         // ে
    const std::string ra    = "\xE0\xA6\xB0";         // র
    const std::string o_indep = "\xE0\xA6\x93";       // ও
    const std::string ke    = "\xE0\xA6\x95\xE0\xA7\x87"; // কে
    const std::string re    = "\xE0\xA6\xB0\xE0\xA7\x87"; // রে
    const std::string ya_nukta = "\xE0\xA7\x9F";      // য়
    const std::string te    = "\xE0\xA6\xA4\xE0\xA7\x87"; // তে
    const std::string ta    = "\xE0\xA6\x9F\xE0\xA6\xBE"; // টা
    const std::string ti    = "\xE0\xA6\x9F\xE0\xA6\xBF"; // টি
    const std::string ra_pl = "\xE0\xA6\xB0\xE0\xA6\xBE"; // রা
    const std::string der   = "\xE0\xA6\xA6\xE0\xA7\x87\xE0\xA6\xB0"; // দের
    const std::string gulo  = "\xE0\xA6\x97\xE0\xA7\x81\xE0\xA6\xB2\xE0\xA7\x8B"; // গুলো
    const std::string guli  = "\xE0\xA6\x97\xE0\xA7\x81\xE0\xA6\xB2\xE0\xA6\xBF"; // গুলি
    const std::string khana = "\xE0\xA6\x96\xE0\xA6\xBE\xE0\xA6\xA8\xE0\xA6\xBE"; // খানা
    const std::string khani = "\xE0\xA6\x96\xE0\xA6\xBE\xE0\xA6\xA8\xE0\xA6\xBF"; // খানি
    const std::string tuku  = "\xE0\xA6\x9F\xE0\xA7\x81\xE0\xA6\x95\xE0\xA7\x81"; // টুকু
    const std::string tai   = "\xE0\xA6\x9F\xE0\xA6\xBE\xE0\xA6\x87"; // তাই

    bool has_kar = EndsInKar(bengali_stem);
    bool has_indep = EndsInIndependentVowel(bengali_stem);

    // 1. Genitive -er
    if (s == "er") {
        if (has_kar) {
            return bengali_stem + ra; // e.g. ঢাকা -> ঢাকার, লিখি -> লিখির
        } else if (has_indep) {
            return bengali_stem + ya_nukta + e_kar + ra; // e.g. বই -> বইয়ের
        } else {
            return bengali_stem + e_kar + ra; // e.g. গুগল -> গুগলের, মানুষ -> মানুষের, দেশ -> দেশের, অফিস -> অফিসের
        }
    }

    // 2. Genitive -r
    if (s == "r") {
        if (has_kar || has_indep) {
            return bengali_stem + ra;
        } else {
            return bengali_stem + e_kar + ra;
        }
    }

    // 3. Emphatic -o
    if (s == "o") {
        return bengali_stem + o_indep; // e.g. পেলে -> পেলেও, গেলে -> গেলেও, করলে -> করলেও, আজ -> আজও
    }

    // 4. Accusative / Dative -ke / -re
    if (s == "ke") {
        return bengali_stem + ke; // e.g. লিখি -> লিখিকে, দেশ -> দেশকে, গুগল -> গুগলকে
    }
    if (s == "re") {
        return bengali_stem + re;
    }

    // 5. Locative -e
    if (s == "e") {
        if (!has_kar && !has_indep) {
            return bengali_stem + e_kar; // e.g. অফিস -> অফিসে, দেশ -> দেশে, ঘর -> ঘরে
        } else {
            return bengali_stem + ya_nukta; // e.g. ঢাকা -> ঢাকায়
        }
    }

    // 6. Locative / Instrumental -te
    if (s == "te") {
        return bengali_stem + te; // e.g. গাড়ি -> গাড়িতে, বাড়ি -> বাড়িতে
    }

    // 7. Definiteness -ta / -ti
    if (s == "ta") {
        return bengali_stem + ta; // e.g. বই -> বইটা, গুগল -> গুগলটা
    }
    if (s == "ti") {
        return bengali_stem + ti; // e.g. বই -> বইটি, লিখি -> লিখিটি
    }

    // 8. Plural -ra / -der
    if (s == "ra") {
        return bengali_stem + ra_pl; // e.g. বন্ধু -> বন্ধুরা
    }
    if (s == "der") {
        return bengali_stem + der; // e.g. মানুষ -> মানুষদের, বন্ধু -> বন্ধুদের
    }

    // 9. Plural -gulo / -guli
    if (s == "gulo") {
        return bengali_stem + gulo; // e.g. দেশ -> দেশগুলো, বই -> বইগুলো
    }
    if (s == "guli") {
        return bengali_stem + guli; // e.g. দেশ -> দেশগুলি
    }

    // 10. Classifiers
    if (s == "khana") return bengali_stem + khana;
    if (s == "khani") return bengali_stem + khani;
    if (s == "tuku")  return bengali_stem + tuku;
    if (s == "tai")   return bengali_stem + tai;

    return bengali_stem;
}

} // namespace bangla
