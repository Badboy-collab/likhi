#ifndef BANGLA_PHONETIC_PARSER_H
#define BANGLA_PHONETIC_PARSER_H

#include <string>
#include <vector>
#include <unordered_map>
#include "../unicode/bangla_unicode.h"

namespace bangla {

struct PhoneticCandidate {
    std::string text;           // UTF-8 Bengali text
    float score;                // Phonetic confidence score
    std::string rule_path;      // Diagnostic rule trace
};

class PhoneticParser {
public:
    PhoneticParser();
    ~PhoneticParser() = default;

    /**
     * Parse a Romanized phonetic token and produce ranked Bengali candidates.
     * @param roman The input ASCII/Latin string (e.g. "ami", "office", "bhalo").
     * @param max_candidates Maximum number of phonetic variations to generate.
     * @return List of scored phonetic candidates.
     */
    std::vector<PhoneticCandidate> Parse(const std::string& roman, size_t max_candidates = 8) const;

private:
    struct ParseState {
        std::u32string bengali;
        size_t roman_pos;
        float penalty;
        char32_t last_consonant;
        bool in_consonant;
    };

    void ExpandState(const std::string& roman, const ParseState& state, std::vector<ParseState>& next_states) const;

    // Helper functions for phonetic mapping
    bool MatchPrefix(const std::string& str, size_t pos, const std::string& prefix) const;
    std::string ToLower(const std::string& str) const;
};

} // namespace bangla

#endif // BANGLA_PHONETIC_PARSER_H
