// fuzzy_normalizer.cpp — Likhi phonetic spelling normalizer
// Maps common Banglish spelling variants to their canonical roman form.
// Rules are applied in order; the first matching rule for each position wins.

#include "fuzzy_normalizer.h"
#include <algorithm>
#include <cctype>
#include <cstring>

namespace bangla {

// ---------------------------------------------------------------------------
// Substitution rule table
// Each entry is { from, to } — case-insensitive on input.
// Longer patterns must appear BEFORE shorter ones (longest-match).
// ---------------------------------------------------------------------------
struct SubRule {
    const char* from;
    const char* to;
};

static const SubRule kSubRules[] = {
    // ── Doubled consonants that users commonly write as single ──────────────
    // "tt" → "t"  (battary → battery: handled separately below)
    // "ll" → "l", "ss" → "sh", etc. are context-dependent; handle below.

    // ── Common vowel alternations ────────────────────────────────────────────
    // "ou" and "ow" are treated the same phonetically
    {"ow",  "ou"},
    // Final "y" after vowel → "i" (battary → battery path via "ary"→"ary")
    // handled by specific suffixes below

    // ── Consonant alternations ───────────────────────────────────────────────
    // "ck" → "k"  (track → trak etc)
    {"ck",  "k"},
    // "ph" → "f"  (phon → fon)  only at start — done in special logic

    // ── Geminate / double-consonant normalization ────────────────────────────
    // Double → single for consonants where gemination is not phonemic in Bangla
    {"ss",  "s"},
    {"tt",  "t"},
    {"ll",  "l"},
    {"nn",  "n"},
    {"mm",  "m"},
    {"rr",  "r"},
    {"pp",  "p"},
    {"bb",  "b"},
    {"ff",  "f"},
    {"cc",  "c"},
    {"dd",  "d"},
    {"gg",  "g"},

    // ── Suffix normalizations ────────────────────────────────────────────────
    // "-ary" → "-ery" → "-ary" (battary, batery → battery path)
    // These are done via suffix rules in ApplyRules()
};

// Suffix normalization table: if the string ENDS with `from`, replace with `to`
struct SuffixRule {
    const char* from;
    const char* to;
};

static const SuffixRule kSuffixRules[] = {
    // battery variants: battary, batary, batery, batteri → battery
    {"ary",   "ery"},   // battary → buttery / battery
    {"ari",   "ery"},   // battari → battery
    {"eri",   "ery"},   // batteri → battery
    {"arry",  "ery"},   // batarry → battery
    // somossa variants: somossa → somossha (alternate canonical)
    {"ossa",  "ossha"},
    {"osha",  "ossha"},
    // poriborton variants: poribarton, poribortan → poriborton
    {"barton","borton"},
    {"bortan","borton"},
    {"bertan","borton"},
    // porishkar variants: poriskar, parishkar → porishkar
    {"iskar", "ishkar"},
    {"arishkar","orishkar"},
    // chair variants
    {"eyar",  "air"},   // cheyar → chair
    {"ear",   "air"},   // chear  → chair
    // kontrol → control
    {"ontrol","ontrol"},
};

// Special whole-word mappings (for names and irregulars that rules miss)
struct WordMap {
    const char* from;
    const char* to;
};

static const WordMap kWordMaps[] = {
    // battery
    {"battary",   "battery"},
    {"batery",    "battery"},
    {"batary",    "battery"},
    {"battari",   "battery"},
    {"batari",    "battery"},
    {"batteri",   "battery"},
    {"batarry",   "battery"},
    // somossa / samossya
    {"somossa",   "somossha"},
    {"somosha",   "somossha"},
    {"shomossa",  "somossha"},
    {"shomossha", "somossha"},
    {"shomosya",  "somossha"},
    {"shomoshya", "somossha"},
    {"somosya",   "somossha"},
    {"somoshya",  "somossha"},
    // poriborton
    {"poribarton","poriborton"},
    {"poribortan","poriborton"},
    {"poribertan","poriborton"},
    {"paribartan","poriborton"},
    // anwar
    {"anoyar",    "anwar"},
    {"anowar",    "anwar"},
    {"anuar",     "anwar"},
    {"anuwar",    "anwar"},
    // porishkar
    {"poriskar",  "porishkar"},
    {"porizkar",  "porishkar"},
    {"parishkar", "porishkar"},
    // chair
    {"cheyar",    "chair"},
    {"chear",     "chair"},
    {"cheyear",   "chair"},
    // table
    {"tebil",     "table"},
    {"tabil",     "table"},
    {"teble",     "table"},
    // control
    {"kontrol",   "control"},
    {"conrol",    "control"},
    {"kontrole",  "control"},
    // computer
    {"compyuter", "computer"},
    {"computar",  "computer"},
    {"compyutar", "computer"},
    {"kompiutar", "computer"},
    // internet
    {"intarnet",  "internet"},
    {"internit",  "internet"},
    {"intenet",   "internet"},
    {"enternet",  "internet"},
    // fan
    {"phan",      "fan"},
    {"fyan",      "fan"},
    // mouse
    {"maus",      "mouse"},
    {"maush",     "mouse"},
    // office
    {"ofis",      "office"},
    {"ofice",     "office"},
    {"ophis",     "office"},
    // screenshot
    {"skrinshot",  "screenshot"},
    {"skrinshat",  "screenshot"},
    // output
    {"outpot",     "output"},
    // brain
    {"bren",       "brain"},
    // better / betor
    {"betor",      "better"},
    {"betar",      "better"},
    // powered
    {"powerd",     "powered"},
    // suggestion
    {"suggession", "suggestion"},
    {"sajeshon",   "suggestion"},
    {"sajeson",    "suggestion"},
    // windows
    {"window",     "windows"},
    // tension
    {"tenshon",   "tension"},
    // challenge
    // (no common misspelling needed — "challenge" → covered)
    // bangladesh
    {"bangladeshi","bangladesh"},
    // khushi / khusi
    {"khusi",     "khushi"},
    // manush variants
    {"manish",    "manush"},
    {"manosh",    "manush"},
    // সংগীত
    {"shongit",   "shongeet"},
    {"sangit",    "shongeet"},
    // sobuj
    {"shobuj",    "sobuj"},
    // valo/bhalo variants — both canonical; keep as-is, engine handles both
};

// ───────────────────────────────────────────────────────────────────────────

FuzzyNormalizer::FuzzyNormalizer() = default;

std::string FuzzyNormalizer::ToLower(const std::string& s) {
    std::string out = s;
    for (char& c : out) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return out;
}

std::string FuzzyNormalizer::ApplyRules(const std::string& roman) const {
    std::string lower = ToLower(roman);

    // 1. Whole-word exact map (fastest path)
    for (const auto& wm : kWordMaps) {
        if (lower == wm.from) {
            return std::string(wm.to);
        }
    }

    // 2. Suffix normalization
    std::string result = lower;
    bool changed = false;
    for (const auto& sr : kSuffixRules) {
        size_t flen = strlen(sr.from);
        if (result.size() >= flen &&
            result.compare(result.size() - flen, flen, sr.from) == 0) {
            result = result.substr(0, result.size() - flen) + sr.to;
            changed = true;
            break;
        }
    }

    if (!changed) {
        // 3. Geminate normalization pass
        std::string degeminated;
        degeminated.reserve(result.size());
        for (size_t i = 0; i < result.size(); ++i) {
            if (i > 0 && result[i] == result[i-1] &&
                // Don't collapse sh, ch, th, dh, ph, bh, kh, gh, rh, ng, ng
                !(i > 0 && (result[i-1] == 's' || result[i-1] == 'c' ||
                             result[i-1] == 't' || result[i-1] == 'd' ||
                             result[i-1] == 'p' || result[i-1] == 'b' ||
                             result[i-1] == 'k' || result[i-1] == 'g' ||
                             result[i-1] == 'r') &&
                  i + 1 < result.size() && result[i+1] == 'h')) {
                // collapse geminate (but only for simple consonants)
                char c = result[i];
                if (c == 't' || c == 'l' || c == 'n' || c == 'm' ||
                    c == 'r' || c == 'p' || c == 'b' || c == 'f' ||
                    c == 'd' || c == 'g') {
                    changed = true;
                    continue; // skip duplicate
                }
            }
            degeminated.push_back(result[i]);
        }
        if (changed) result = degeminated;
    }

    return changed ? result : std::string();
}

std::vector<std::string> FuzzyNormalizer::Normalize(const std::string& roman) const {
    std::vector<std::string> variants;
    std::string lower = ToLower(roman);
    variants.push_back(lower);

    std::string canonical = ApplyRules(lower);
    if (!canonical.empty() && canonical != lower) {
        variants.push_back(canonical);

        // One more level: apply rules to the canonical form
        std::string canonical2 = ApplyRules(canonical);
        if (!canonical2.empty() && canonical2 != canonical && canonical2 != lower) {
            variants.push_back(canonical2);
        }
    }

    return variants;
}

std::string FuzzyNormalizer::BestCanonical(const std::string& roman) const {
    auto v = Normalize(roman);
    return v.empty() ? roman : v.back();
}

} // namespace bangla
