#include "phonetic_parser.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <queue>

namespace bangla {

using namespace codepoint;

PhoneticParser::PhoneticParser() {
}

std::string PhoneticParser::ToLower(const std::string& str) const {
    std::string out = str;
    for (char& c : out) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return out;
}

bool PhoneticParser::MatchPrefix(const std::string& str, size_t pos, const std::string& prefix) const {
    if (pos + prefix.size() > str.size()) return false;
    for (size_t i = 0; i < prefix.size(); i++) {
        if (std::tolower(static_cast<unsigned char>(str[pos + i])) != 
            std::tolower(static_cast<unsigned char>(prefix[i]))) {
            return false;
        }
    }
    return true;
}

void PhoneticParser::ExpandState(const std::string& roman, const ParseState& state, std::vector<ParseState>& next_states) const {
    size_t pos = state.roman_pos;
    size_t len = roman.size();
    if (pos >= len) return;

    char cur_char = roman[pos];

    // Helper lambda to append a consonant
    auto append_consonant = [&](char32_t cp, size_t consumed, float penalty) {
        if (state.in_consonant && state.last_consonant != 0) {
            // Branch 1: Conjunct cluster (Hasant)
            ParseState s = state;
            if (state.last_consonant == CONSONANT_RA && s.bengali.size() >= 1 && s.bengali.back() == CONSONANT_RA) {
                // Ref (র + ্ + consonant)
                s.bengali.push_back(HASANT);
                s.bengali.push_back(cp);
            } else if (cp == CONSONANT_RA) {
                // Ra-fala (consonant + ্ + র)
                s.bengali.push_back(HASANT);
                s.bengali.push_back(CONSONANT_RA);
            } else if (cp == CONSONANT_YYA || cp == CONSONANT_YA) {
                // Ja-fala (consonant + ্ + য)
                s.bengali.push_back(HASANT);
                s.bengali.push_back(CONSONANT_YA);
            } else {
                // General conjunct
                s.bengali.push_back(HASANT);
                s.bengali.push_back(cp);
            }
            s.roman_pos = pos + consumed;
            s.penalty += penalty;
            s.in_consonant = (cp != ANUSVARA && cp != VISARGA && cp != CHANDRABINDU);
            s.last_consonant = cp;
            next_states.push_back(s);

            // Branch 2: Inherent vowel separation (no Hasant)
            ParseState s2 = state;
            s2.bengali.push_back(cp);
            s2.roman_pos = pos + consumed;
            s2.penalty += penalty + 0.01f;
            s2.in_consonant = (cp != ANUSVARA && cp != VISARGA && cp != CHANDRABINDU);
            s2.last_consonant = cp;
            next_states.push_back(s2);
        } else {
            ParseState s = state;
            s.bengali.push_back(cp);
            s.roman_pos = pos + consumed;
            s.penalty += penalty;
            s.in_consonant = (cp != ANUSVARA && cp != VISARGA && cp != CHANDRABINDU);
            s.last_consonant = cp;
            next_states.push_back(s);
        }
    };

    // Helper lambda to append a vowel
    auto append_vowel = [&](char32_t indep, char32_t kar, size_t consumed, float penalty) {
        ParseState s = state;
        if (state.in_consonant) {
            if (kar != 0) {
                s.bengali.push_back(kar);
            }
        } else {
            if (indep != 0) {
                s.bengali.push_back(indep);
            }
        }
        s.roman_pos = pos + consumed;
        s.penalty += penalty;
        s.in_consonant = false;
        s.last_consonant = 0;
        next_states.push_back(s);
    };

    // 1. Check Specific Multi-character Clusters & Digraphs
    if (MatchPrefix(roman, pos, "cch") || MatchPrefix(roman, pos, "chch") || MatchPrefix(roman, pos, "cchh") || MatchPrefix(roman, pos, "chh") || MatchPrefix(roman, pos, "cch")) {
        ParseState s = state;
        if (state.in_consonant && state.last_consonant != 0) {
            s.bengali.push_back(HASANT);
        }
        s.bengali += {CONSONANT_CA, HASANT, CONSONANT_CHA}; // চ্ছ
        size_t consumed = 3;
        if (MatchPrefix(roman, pos, "chch") || MatchPrefix(roman, pos, "cchh")) consumed = 4;
        s.roman_pos = pos + consumed;
        s.penalty += 0.0f;
        s.in_consonant = true;
        s.last_consonant = CONSONANT_CHA;
        next_states.push_back(s);
        return;
    }
    if (MatchPrefix(roman, pos, "kkh")) {
        ParseState s = state;
        if (state.in_consonant && state.last_consonant != 0) {
            s.bengali.push_back(HASANT);
        }
        s.bengali += {CONSONANT_KA, HASANT, CONSONANT_SSA}; // ক্ষ
        s.roman_pos = pos + 3;
        s.penalty += 0.0f;
        s.in_consonant = true;
        s.last_consonant = CONSONANT_SSA;
        next_states.push_back(s);
        return;
    }
    if (MatchPrefix(roman, pos, "ggan") || MatchPrefix(roman, pos, "gyan")) {
        ParseState s = state;
        s.bengali += {CONSONANT_JA, HASANT, CONSONANT_NYA, KAR_AA, CONSONANT_NA}; // জ্ঞান
        s.roman_pos = pos + 4;
        s.penalty += 0.0f;
        s.in_consonant = false;
        s.last_consonant = 0;
        next_states.push_back(s);
        return;
    }
    if (MatchPrefix(roman, pos, "shch")) {
        ParseState s = state;
        s.bengali += {CONSONANT_SHA, HASANT, CONSONANT_CA}; // শ্চ
        s.roman_pos = pos + 4;
        s.penalty += 0.0f;
        s.in_consonant = true;
        s.last_consonant = CONSONANT_CA;
        next_states.push_back(s);
        return;
    }
    if (MatchPrefix(roman, pos, "tth")) {
        append_consonant(CONSONANT_TTHA, 3, 0.0f); // ঠ
        return;
    }
    if (MatchPrefix(roman, pos, "ddh")) {
        append_consonant(CONSONANT_DDHA, 3, 0.0f); // ঢ
        return;
    }
    if (MatchPrefix(roman, pos, "shsh") || MatchPrefix(roman, pos, "ssh")) {
        size_t consumed = MatchPrefix(roman, pos, "shsh") ? 4 : 3;
        ParseState s = state;
        if (state.in_consonant && state.last_consonant != 0) {
            s.bengali.push_back(HASANT);
        }
        s.bengali += {CONSONANT_SA, HASANT, CONSONANT_YA}; // স্য
        s.roman_pos = pos + consumed;
        s.penalty += 0.01f;
        s.in_consonant = true;
        s.last_consonant = CONSONANT_YA;
        next_states.push_back(s);

        ParseState s2 = state;
        if (state.in_consonant && state.last_consonant != 0) {
            s2.bengali.push_back(HASANT);
        }
        s2.bengali += {CONSONANT_SHA, HASANT, CONSONANT_SHA}; // শ + শ
        s2.roman_pos = pos + consumed;
        s2.penalty += 0.02f;
        s2.in_consonant = true;
        s2.last_consonant = CONSONANT_SHA;
        next_states.push_back(s2);
        return;
    }
    if (MatchPrefix(roman, pos, "ss")) {
        ParseState s = state;
        if (state.in_consonant && state.last_consonant != 0) {
            s.bengali.push_back(HASANT);
        }
        s.bengali += {CONSONANT_SA, HASANT, CONSONANT_YA}; // স্য
        s.roman_pos = pos + 2;
        s.penalty += 0.01f;
        s.in_consonant = true;
        s.last_consonant = CONSONANT_YA;
        next_states.push_back(s);

        ParseState s2 = state;
        if (state.in_consonant && state.last_consonant != 0) {
            s2.bengali.push_back(HASANT);
        }
        s2.bengali += {CONSONANT_SA, HASANT, CONSONANT_SA}; // স্স
        s2.roman_pos = pos + 2;
        s2.penalty += 0.02f;
        s2.in_consonant = true;
        s2.last_consonant = CONSONANT_SA;
        next_states.push_back(s2);
        return;
    }

    // 2. Digraph Consonants
    if (MatchPrefix(roman, pos, "kh")) {
        append_consonant(CONSONANT_KHA, 2, 0.0f);
        return;
    }
    if (MatchPrefix(roman, pos, "gh")) {
        append_consonant(CONSONANT_GHA, 2, 0.0f);
        return;
    }
    if (MatchPrefix(roman, pos, "ng")) {
        append_consonant(ANUSVARA, 2, 0.0f); // ং
        append_consonant(CONSONANT_NGA, 2, 0.05f); // ঙ
        return;
    }
    if (MatchPrefix(roman, pos, "ch")) {
        append_consonant(CONSONANT_CA, 2, 0.0f);   // চ
        append_consonant(CONSONANT_CHA, 2, 0.05f); // ছ
        return;
    }
    if (MatchPrefix(roman, pos, "jh")) {
        append_consonant(CONSONANT_JHA, 2, 0.0f);
        return;
    }
    if (MatchPrefix(roman, pos, "th")) {
        append_consonant(CONSONANT_THA, 2, 0.0f);  // থ
        append_consonant(CONSONANT_TTHA, 2, 0.05f); // ঠ
        return;
    }
    if (MatchPrefix(roman, pos, "dh")) {
        append_consonant(CONSONANT_DHA, 2, 0.0f);  // ধ
        append_consonant(CONSONANT_DDHA, 2, 0.05f); // ঢ
        return;
    }
    if (MatchPrefix(roman, pos, "ph")) {
        append_consonant(CONSONANT_PHA, 2, 0.0f);
        return;
    }
    if (MatchPrefix(roman, pos, "bh")) {
        append_consonant(CONSONANT_BHA, 2, 0.0f); // ভ
        return;
    }
    if (MatchPrefix(roman, pos, "sh")) {
        append_consonant(CONSONANT_SHA, 2, 0.0f); // শ
        append_consonant(CONSONANT_SA, 2, 0.01f);  // স (e.g. shocheton -> সচেতন)
        append_consonant(CONSONANT_SSA, 2, 0.05f); // ষ
        return;
    }
    if (MatchPrefix(roman, pos, "rh")) {
        append_consonant(CONSONANT_RRA, 2, 0.0f); // ড়
        append_consonant(CONSONANT_RHA, 2, 0.05f); // ঢ়
        return;
    }

    // 3. Digraph Vowels
    if (MatchPrefix(roman, pos, "aa")) {
        append_vowel(VOWEL_AA, KAR_AA, 2, 0.0f);
        return;
    }
    if (MatchPrefix(roman, pos, "ee") || MatchPrefix(roman, pos, "ii")) {
        append_vowel(VOWEL_I, KAR_I, 2, 0.0f);
        append_vowel(VOWEL_II, KAR_II, 2, 0.02f);
        return;
    }
    if (MatchPrefix(roman, pos, "oo") || MatchPrefix(roman, pos, "uu")) {
        append_vowel(VOWEL_U, KAR_U, 2, 0.0f);
        append_vowel(VOWEL_UU, KAR_UU, 2, 0.02f);
        return;
    }
    if (MatchPrefix(roman, pos, "rri")) {
        append_vowel(VOWEL_RI, KAR_RI, 3, 0.0f);
        return;
    }
    if (MatchPrefix(roman, pos, "ri")) {
        if (pos == 0) {
            append_vowel(VOWEL_RI, KAR_RI, 2, 0.02f);
        } else if (state.in_consonant) {
            append_vowel(VOWEL_RI, KAR_RI, 2, 0.05f);
        }
        // Fall through to allow 'r' + 'i' (CONSONANT_RA + KAR_I) to evaluate normally
    }
    if (MatchPrefix(roman, pos, "oi")) {
        append_vowel(VOWEL_OI, KAR_OI, 2, 0.0f);
        return;
    }
    if (MatchPrefix(roman, pos, "ou") || MatchPrefix(roman, pos, "ow")) {
        append_vowel(VOWEL_OU, KAR_OU, 2, 0.0f);
        return;
    }
    if (MatchPrefix(roman, pos, "oy") || (MatchPrefix(roman, pos, "ai") && pos + 2 == len)) {
        ParseState s = state;
        if (MatchPrefix(roman, pos, "ai")) {
            if (state.in_consonant) {
                s.bengali.push_back(KAR_AA);
            } else {
                s.bengali.push_back(VOWEL_AA);
            }
        }
        s.bengali.push_back(CONSONANT_YYA);
        s.roman_pos = pos + 2;
        s.penalty += 0.0f;
        s.in_consonant = false;
        s.last_consonant = 0;
        next_states.push_back(s);

        if (MatchPrefix(roman, pos, "oy")) {
            append_vowel(VOWEL_OI, KAR_OI, 2, 0.05f);
        }
        return;
    }

    // 4. Single Consonants
    switch (cur_char) {
        case 'k':
            append_consonant(CONSONANT_KA, 1, 0.0f);
            append_consonant(CONSONANT_KHA, 1, 0.02f); // kub -> খুব
            break;
        case 'K':
            append_consonant(CONSONANT_KHA, 1, 0.0f);
            append_consonant(CONSONANT_KA, 1, 0.02f);
            break;
        case 'g': case 'G':
            append_consonant(CONSONANT_GA, 1, 0.0f);
            break;
        case 'c':
            append_consonant(CONSONANT_CA, 1, 0.0f);
            append_consonant(CONSONANT_CHA, 1, 0.02f); // c -> ছ
            append_consonant(CONSONANT_KA, 1, 0.05f);
            break;
        case 'C':
            append_consonant(CONSONANT_CHA, 1, 0.0f);
            append_consonant(CONSONANT_CA, 1, 0.02f);
            break;
        case 'j':
            append_consonant(CONSONANT_JA, 1, 0.0f);
            append_consonant(CONSONANT_YA, 1, 0.01f);
            append_consonant(CONSONANT_JHA, 1, 0.03f); // j -> ঝ
            break;
        case 'J':
            append_consonant(CONSONANT_JHA, 1, 0.0f);
            append_consonant(CONSONANT_JA, 1, 0.02f);
            break;
        case 'z': case 'Z':
            append_consonant(CONSONANT_JA, 1, 0.0f);
            append_consonant(CONSONANT_YA, 1, 0.02f);
            break;
        case 't':
            append_consonant(CONSONANT_TA, 1, 0.0f);
            append_consonant(CONSONANT_TTA, 1, 0.03f);
            break;
        case 'T':
            append_consonant(CONSONANT_TTA, 1, 0.0f);
            append_consonant(CONSONANT_TA, 1, 0.05f);
            break;
        case 'd':
            append_consonant(CONSONANT_DA, 1, 0.0f);
            append_consonant(CONSONANT_DDA, 1, 0.03f);
            break;
        case 'D':
            append_consonant(CONSONANT_DDA, 1, 0.0f);
            append_consonant(CONSONANT_DA, 1, 0.05f);
            break;
        case 'n':
            append_consonant(CONSONANT_NA, 1, 0.0f);
            append_consonant(CONSONANT_NNA, 1, 0.05f);
            break;
        case 'N':
            append_consonant(CONSONANT_NNA, 1, 0.0f);
            append_consonant(CONSONANT_NA, 1, 0.02f);
            break;
        case 'p': case 'P':
            append_consonant(CONSONANT_PA, 1, 0.0f);
            break;
        case 'f': case 'F':
            append_consonant(CONSONANT_PHA, 1, 0.0f);
            break;
        case 'b': case 'B':
            append_consonant(CONSONANT_BA, 1, 0.0f);
            break;
        case 'v': case 'V':
            append_consonant(CONSONANT_BHA, 1, 0.0f);
            append_consonant(CONSONANT_BA, 1, 0.05f);
            break;
        case 'm': case 'M':
            append_consonant(CONSONANT_MA, 1, 0.0f);
            break;
        case 'r':
            append_consonant(CONSONANT_RA, 1, 0.0f);
            append_consonant(CONSONANT_RRA, 1, 0.03f); // r -> ড়
            append_consonant(CONSONANT_RHA, 1, 0.05f); // r -> ঢ়
            break;
        case 'R':
            append_consonant(CONSONANT_RRA, 1, 0.0f);
            append_consonant(CONSONANT_RA, 1, 0.03f);
            append_consonant(CONSONANT_RHA, 1, 0.05f);
            break;
        case 'l': case 'L':
            append_consonant(CONSONANT_LA, 1, 0.0f);
            break;
        case 's':
            append_consonant(CONSONANT_SA, 1, 0.0f);
            append_consonant(CONSONANT_SHA, 1, 0.02f); // s -> শ
            append_consonant(CONSONANT_SSA, 1, 0.04f); // s -> ষ
            break;
        case 'S':
            append_consonant(CONSONANT_SHA, 1, 0.0f);
            append_consonant(CONSONANT_SSA, 1, 0.03f);
            break;
        case 'h': case 'H':
            append_consonant(CONSONANT_HA, 1, 0.0f);
            break;
        case 'w': case 'W':
            append_consonant(CONSONANT_BA, 1, 0.05f);
            break;
        case 'y': case 'Y':
            append_consonant(CONSONANT_YYA, 1, 0.0f);
            append_consonant(CONSONANT_YA, 1, 0.02f);
            break;

        // 5. Single Vowels
        case 'a': case 'A':
            if (!state.in_consonant) {
                append_vowel(VOWEL_AA, KAR_AA, 1, 0.0f);
                append_vowel(VOWEL_A, 0, 1, 0.01f);
                append_vowel(VOWEL_E, KAR_E, 1, 0.02f);
            } else {
                append_vowel(VOWEL_AA, KAR_AA, 1, 0.0f);
                append_vowel(VOWEL_A, 0, 1, 0.01f);
            }
            break;
        case 'i': case 'I':
            append_vowel(VOWEL_I, KAR_I, 1, 0.0f);
            append_vowel(VOWEL_II, KAR_II, 1, 0.05f);
            break;
        case 'u': case 'U':
            append_vowel(VOWEL_U, KAR_U, 1, 0.0f);
            append_vowel(VOWEL_UU, KAR_UU, 1, 0.01f);
            break;
        case 'e': case 'E':
            append_vowel(VOWEL_E, KAR_E, 1, 0.0f);
            break;
        case 'o': case 'O':
            if (!state.in_consonant) {
                append_vowel(VOWEL_A, 0, 1, 0.0f);
                append_vowel(VOWEL_O, KAR_O, 1, 0.02f);
            } else {
                append_vowel(VOWEL_O, KAR_O, 1, 0.0f);
                append_vowel(VOWEL_A, 0, 1, 0.01f);
            }
            break;

        // 6. Bengali Punctuation & Bengali Numbers
        case '.':
            append_consonant(DARI, 1, 0.0f);
            break;
        case '0': append_consonant(0x09E6, 1, 0.0f); break;
        case '1': append_consonant(0x09E7, 1, 0.0f); break;
        case '2': append_consonant(0x09E8, 1, 0.0f); break;
        case '3': append_consonant(0x09E9, 1, 0.0f); break;
        case '4': append_consonant(0x09EA, 1, 0.0f); break;
        case '5': append_consonant(0x09EB, 1, 0.0f); break;
        case '6': append_consonant(0x09EC, 1, 0.0f); break;
        case '7': append_consonant(0x09ED, 1, 0.0f); break;
        case '8': append_consonant(0x09EE, 1, 0.0f); break;
        case '9': append_consonant(0x09EF, 1, 0.0f); break;

        default:
            ParseState s = state;
            s.bengali.push_back(static_cast<char32_t>(cur_char));
            s.roman_pos = pos + 1;
            s.penalty += 0.0f;
            s.in_consonant = false;
            s.last_consonant = 0;
            next_states.push_back(s);
            break;
    }
}

std::vector<PhoneticCandidate> PhoneticParser::Parse(const std::string& roman, size_t max_candidates) const {
    std::vector<PhoneticCandidate> results;
    if (roman.empty()) return results;

    auto cmp = [](const ParseState& a, const ParseState& b) {
        return a.penalty > b.penalty;
    };
    std::priority_queue<ParseState, std::vector<ParseState>, decltype(cmp)> pq(cmp);

    ParseState initial;
    initial.roman_pos = 0;
    initial.penalty = 0.0f;
    initial.last_consonant = 0;
    initial.in_consonant = false;
    pq.push(initial);

    std::vector<ParseState> complete_states;
    size_t beam_width = 64;

    while (!pq.empty() && complete_states.size() < beam_width * 2) {
        ParseState cur = pq.top();
        pq.pop();

        if (cur.roman_pos == roman.size()) {
            complete_states.push_back(cur);
            continue;
        }

        std::vector<ParseState> next_states;
        ExpandState(roman, cur, next_states);

        for (const auto& next : next_states) {
            pq.push(next);
        }
    }

    std::sort(complete_states.begin(), complete_states.end(), [](const ParseState& a, const ParseState& b) {
        return a.penalty < b.penalty;
    });

    std::vector<std::string> seen;
    for (const auto& st : complete_states) {
        std::string utf8 = UnicodeUtils::Utf32ToUtf8(st.bengali);
        if (std::find(seen.begin(), seen.end(), utf8) == seen.end()) {
            seen.push_back(utf8);
            PhoneticCandidate cand;
            cand.text = utf8;
            cand.score = std::exp(-st.penalty);
            cand.rule_path = "beam_search";
            results.push_back(cand);
            if (results.size() >= max_candidates) break;
        }
    }

    return results;
}

} // namespace bangla
