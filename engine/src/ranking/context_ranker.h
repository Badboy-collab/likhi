#ifndef BANGLA_CONTEXT_RANKER_H
#define BANGLA_CONTEXT_RANKER_H

#include "../dictionary/lexicon_trie.h"
#include "../transliteration/phonetic_parser.h"
#include <string>
#include <vector>
#include <unordered_map>

namespace bangla {

struct ScoredCandidate {
    std::string bengali_text;
    std::string roman_origin;
    float phonetic_score;
    float unigram_score;
    float bigram_score;
    float personal_score;
    float final_score;
    uint32_t category_flags;
    bool auto_correct_recommended;
};

class ContextRanker {
public:
    ContextRanker();
    ~ContextRanker() = default;

    void AddBigram(const std::string& word1, const std::string& word2, uint32_t count);
    float ComputeBigramProb(const std::string& prev_word, const std::string& cur_word) const;

    std::vector<ScoredCandidate> RankCandidates(
        const std::string& roman_input,
        const std::vector<PhoneticCandidate>& phonetic_candidates,
        const LexiconTrie& lexicon,
        const std::string& prev_word,
        const std::unordered_map<std::string, float>& user_dict_boosts,
        bool auto_correct_enabled,
        float auto_correct_threshold,
        size_t max_candidates = 6
    ) const;

    std::vector<ScoredCandidate> PredictNextWords(
        const std::string& prev_word,
        const LexiconTrie& lexicon,
        size_t max_predictions = 5
    ) const;

private:
    std::unordered_map<std::string, std::unordered_map<std::string, uint32_t>> bigrams_;
    void LoadDefaultBigrams();
    size_t LevenshteinDistance(const std::string& s1, const std::string& s2) const;
};

} // namespace bangla

#endif // BANGLA_CONTEXT_RANKER_H
