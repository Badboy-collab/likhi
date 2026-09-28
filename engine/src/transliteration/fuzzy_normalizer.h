#pragma once
// fuzzy_normalizer.h — Likhi phonetic spelling normalizer
// Maps common user spelling variants to their canonical roman form
// before lexicon lookup. Pure string transformation, zero external deps.
// This is NOT a general spell checker: it only normalizes known phonetic
// patterns that Bangla users commonly alternate between.

#include <string>
#include <vector>

namespace bangla {

class FuzzyNormalizer {
public:
    FuzzyNormalizer();

    // Returns 1–3 canonical roman variants for the given input.
    // The input itself is always included as the first result (exact first).
    // Additional variants are returned in order of phonetic plausibility.
    std::vector<std::string> Normalize(const std::string& roman) const;

    // Single best canonical form (first result of Normalize).
    std::string BestCanonical(const std::string& roman) const;

private:
    // Apply a single-pass substitution table to produce one variant.
    // Returns empty string if no substitution was applicable.
    std::string ApplyRules(const std::string& roman) const;

    // Lowercase helper
    static std::string ToLower(const std::string& s);
};

} // namespace bangla
