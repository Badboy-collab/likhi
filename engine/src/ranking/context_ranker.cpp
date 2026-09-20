#include "context_ranker.h"
#include "../include/bangla_engine.h"
#include "../unicode/bangla_unicode.h"
#include <cmath>
#include <algorithm>
#include <unordered_set>

namespace bangla {

ContextRanker::ContextRanker() {
    LoadDefaultBigrams();
}

void ContextRanker::AddBigram(const std::string& word1, const std::string& word2, uint32_t count) {
    bigrams_[word1][word2] += count;
}

void ContextRanker::LoadDefaultBigrams() {
    // Subject + Verb / Adverb Bigrams
    AddBigram("আমি", "যাব", 8000);
    AddBigram("আমি", "আছি", 7500);
    AddBigram("আমি", "ভাত", 6000);
    AddBigram("আমি", "বাংলায়", 6500);
    AddBigram("আমি", "আজকে", 9000);
    AddBigram("আমি", "এখন", 8500);
    AddBigram("আমি", "খাব", 7000);
    AddBigram("আমি", "করব", 7500);
    AddBigram("আমি", "জানি", 6500);
    AddBigram("আমি", "পারি", 6000);

    AddBigram("তুমি", "কেমন", 9500);
    AddBigram("তুমি", "কোথায়", 8000);
    AddBigram("তুমি", "কি", 9000);
    AddBigram("তুমি", "যাবে", 7500);
    AddBigram("তুমি", "আছ", 8000);
    AddBigram("তুমি", "খাবে", 7000);
    AddBigram("তুমি", "করবে", 7500);

    AddBigram("সে", "এখন", 8500);
    AddBigram("সে", "যাবে", 8000);
    AddBigram("সে", "আছে", 7500);
    AddBigram("সে", "সব", 7000);

    AddBigram("আমরা", "সবাই", 8500);
    AddBigram("আমরা", "একসাথে", 8000);
    AddBigram("আমরা", "কাজ", 7500);
    AddBigram("আমরা", "যাব", 7000);

    AddBigram("আপনি", "কেমন", 9000);
    AddBigram("আপনি", "কোথায়", 7500);
    AddBigram("আপনি", "আছেন", 8000);
    AddBigram("আপনার", "সাথে", 9000);
    AddBigram("আপনার", "নাম", 8500);

    AddBigram("কেমন", "আছ", 9500);
    AddBigram("কেমন", "আছেন", 9000);

    AddBigram("আজকে", "অফিসে", 8500);
    AddBigram("আজকে", "বৃষ্টি", 8000);
    AddBigram("আজকে", "যাব", 7500);
    AddBigram("আজকে", "কাজ", 7000);
    AddBigram("আজকে", "ছুটি", 6500);

    AddBigram("অফিসে", "যাব", 9000);
    AddBigram("অফিসে", "কাজ", 8000);
    AddBigram("অফিস", "যাব", 4000);

    AddBigram("গান", "গাই", 6000);
    AddBigram("গান", "শুনব", 5500);

    AddBigram("কথা", "বলে", 8000);
    AddBigram("কথা", "বলতে", 7500);
    AddBigram("বলে", "ভালো", 8500);
    AddBigram("ভালো", "লাগল", 9000);
    AddBigram("ভালো", "আছি", 8500);
    AddBigram("ভালো", "হবে", 8000);

    AddBigram("সবাই", "একসাথে", 8500);
    AddBigram("একসাথে", "কাজ", 8000);
    AddBigram("কাজ", "করব", 9000);
    AddBigram("কাজ", "শেষ", 8500);
    AddBigram("কাজ", "করছি", 8000);

    AddBigram("বৃষ্টি", "হচ্ছে", 9000);
    AddBigram("বৃষ্টি", "নামছে", 7000);

    AddBigram("এই", "বই", 8000);
    AddBigram("এই", "দেশ", 8500);
    AddBigram("এই", "সময়", 7500);
    AddBigram("বই", "টা", 8500);
    AddBigram("বই", "পড়ব", 8000);
    AddBigram("টা", "অনেক", 8000);
    AddBigram("টা", "সুন্দর", 8500);
    AddBigram("অনেক", "সুন্দর", 9000);
    AddBigram("অনেক", "ভালো", 8500);
    AddBigram("অনেক", "ধন্যবাদ", 9500);

    AddBigram("ভাত", "খেয়েছ", 8000);
    AddBigram("ভাত", "খাব", 8500);
    AddBigram("চা", "খাব", 8000);

    AddBigram("সব", "বুঝতে", 7500);
    AddBigram("সব", "কিছু", 8500);
    AddBigram("বুঝতে", "পারবে", 8500);
    AddBigram("বুঝতে", "পারি", 8000);

    AddBigram("বাংলাদেশ", "আমার", 9500);
    AddBigram("বাংলাদেশ", "একটি", 8500);
    AddBigram("আমার", "জন্মভূমি", 9000);
    AddBigram("আমার", "দেশ", 8500);
    AddBigram("আমার", "বন্ধু", 8000);

    AddBigram("আমাদের", "স্বাস্থ্য", 8000);
    AddBigram("আমাদের", "দেশ", 8500);
    AddBigram("স্বাস্থ্য", "সচেতন", 8500);
    AddBigram("সচেতন", "হতে", 8500);
    AddBigram("হতে", "হবে", 9000);

    AddBigram("বিজ্ঞান", "ও", 8500);
    AddBigram("ও", "প্রযুক্তি", 9000);
    AddBigram("প্রযুক্তি", "আমাদের", 8000);
    AddBigram("নতুন", "রূপ", 8500);
    AddBigram("রূপ", "দিচ্ছে", 8500);

    AddBigram("সব", "শিক্ষক", 8000);
    AddBigram("শিক্ষক", "ও", 8500);
    AddBigram("ও", "ছাত্র", 8500);
    AddBigram("ছাত্র", "একসাথে", 8000);
    AddBigram("পরীক্ষা", "দিচ্ছে", 8500);

    AddBigram("শিক্ষা", "হলো", 8500);
    AddBigram("শিক্ষা", "জাতির", 8000);

    AddBigram("খুব", "সুন্দর", 9000);
    AddBigram("খুব", "ভালো", 9000);
    AddBigram("খুব", "দ্রুত", 8500);

    AddBigram("পরিবর্তন", "করবো", 9500);
    AddBigram("একটা", "পরিবর্তন", 8500);
}

float ContextRanker::ComputeBigramProb(const std::string& prev_word, const std::string& cur_word) const {
    if (prev_word.empty() || cur_word.empty()) return 0.0f;
    auto it1 = bigrams_.find(prev_word);
    if (it1 == bigrams_.end()) return 0.0f;

    auto it2 = it1->second.find(cur_word);
    if (it2 == it1->second.end()) return 0.0f;

    float count = static_cast<float>(it2->second);
    return std::min(1.0f, std::log10(count + 1.0f) / 4.0f);
}

static int FastLevenshtein(const std::string& s1, const std::string& s2) {
    int m = static_cast<int>(s1.size());
    int n = static_cast<int>(s2.size());
    if (std::abs(m - n) > 2) return 99;
    std::vector<int> v0(n + 1);
    std::vector<int> v1(n + 1);
    for (int i = 0; i <= n; i++) v0[i] = i;
    for (int i = 0; i < m; i++) {
        v1[0] = i + 1;
        for (int j = 0; j < n; j++) {
            int cost = (s1[i] == s2[j]) ? 0 : 1;
            v1[j + 1] = std::min({v1[j] + 1, v0[j + 1] + 1, v0[j] + cost});
        }
        v0 = v1;
    }
    return v0[n];
}

std::vector<ScoredCandidate> ContextRanker::RankCandidates(
    const std::string& roman_input,
    const std::vector<PhoneticCandidate>& phonetic_candidates,
    const LexiconTrie& lexicon,
    const std::string& prev_word,
    const std::unordered_map<std::string, float>& user_dict_boosts,
    bool auto_correct_enabled,
    float auto_correct_threshold,
    size_t max_candidates
) const {
    std::unordered_map<std::string, ScoredCandidate> candidate_map;
    bool has_dictionary_match = false;

    // 1. Ingest phonetic candidates
    for (const auto& pc : phonetic_candidates) {
        if (!UnicodeUtils::IsValidBengaliSequence(pc.text)) continue;

        ScoredCandidate sc;
        sc.bengali_text = pc.text;
        sc.roman_origin = roman_input;
        sc.phonetic_score = pc.score;
        sc.unigram_score = 0.0f;
        sc.bigram_score = 0.0f;
        sc.personal_score = 0.0f;
        sc.category_flags = CANDIDATE_FLAG_PRIMARY;
        sc.auto_correct_recommended = false;

        const LexiconEntry* entry = lexicon.Find(pc.text);
        if (entry) {
            sc.unigram_score = std::min(1.0f, std::log10(static_cast<float>(entry->frequency) + 1.0f) / 6.0f);
            sc.category_flags |= CANDIDATE_FLAG_EXACT_MATCH;
            has_dictionary_match = true;
        }

        candidate_map[pc.text] = sc;
    }

    // 2. Ingest Roman lexicon exact/prefix matches
    std::vector<LexiconEntry> roman_matches = lexicon.SearchRoman(roman_input, 5);
    for (const auto& rm : roman_matches) {
        if (!UnicodeUtils::IsValidBengaliSequence(rm.bengali_word)) continue;

        if (candidate_map.find(rm.bengali_word) == candidate_map.end()) {
            ScoredCandidate sc;
            sc.bengali_text = rm.bengali_word;
            sc.roman_origin = rm.roman_key;
            sc.phonetic_score = 0.95f;
            sc.unigram_score = std::min(1.0f, std::log10(static_cast<float>(rm.frequency) + 1.0f) / 6.0f);
            sc.bigram_score = 0.0f;
            sc.personal_score = 0.0f;
            sc.category_flags = CANDIDATE_FLAG_EXACT_MATCH;
            sc.auto_correct_recommended = false;
            candidate_map[rm.bengali_word] = sc;
            has_dictionary_match = true;
        } else {
            candidate_map[rm.bengali_word].category_flags |= CANDIDATE_FLAG_EXACT_MATCH;
            if (candidate_map[rm.bengali_word].phonetic_score < 0.90f) {
                candidate_map[rm.bengali_word].phonetic_score = 0.90f;
            }
            has_dictionary_match = true;
        }
    }

    // 2b. Ingest Controlled Fuzzy Banglish Candidates (P0-3)
    // Only search fuzzy variants if no exact Roman lexicon match exists
    if (roman_matches.empty() && roman_input.size() >= 3 && roman_input.size() <= 24) {
        std::unordered_set<std::string> variants;

        // Consonant doubling / deduplication
        for (size_t i = 0; i < roman_input.size(); i++) {
            if (i + 1 < roman_input.size() && roman_input[i] == roman_input[i + 1]) {
                variants.insert(roman_input.substr(0, i) + roman_input.substr(i + 1));
            }
            char c = roman_input[i];
            if (c == 't' || c == 's' || c == 'p' || c == 'b' || c == 'l' || 
                c == 'm' || c == 'n' || c == 'd' || c == 'r' || c == 'k') {
                variants.insert(roman_input.substr(0, i + 1) + c + roman_input.substr(i + 1));
            }
        }

        // Vowel substitutions (a <-> e, e <-> i, i <-> y, o <-> u, a <-> o)
        const char vowels[] = {'a', 'e', 'i', 'o', 'u', 'y'};
        for (size_t i = 0; i < roman_input.size(); i++) {
            char c = roman_input[i];
            bool is_v = false;
            for (char v : vowels) { if (c == v) { is_v = true; break; } }
            if (is_v) {
                for (char alt : vowels) {
                    if (alt != c) {
                        std::string v = roman_input;
                        v[i] = alt;
                        variants.insert(v);
                    }
                }
            }
        }

        // Combined: consonant variation + vowel variation (e.g. battary -> battery, batery -> battery)
        std::vector<std::string> base_variants(variants.begin(), variants.end());
        for (const auto& bv : base_variants) {
            for (size_t i = 0; i < bv.size(); i++) {
                char c = bv[i];
                if (c == 'a' || c == 'e' || c == 'i' || c == 'y') {
                    for (char alt : {'a', 'e', 'i', 'y'}) {
                        if (alt != c) {
                            std::string v2 = bv;
                            v2[i] = alt;
                            variants.insert(v2);
                        }
                    }
                }
            }
        }

        for (const auto& var : variants) {
            std::vector<LexiconEntry> f_matches = lexicon.SearchRoman(var, 2);
            for (const auto& fm : f_matches) {
                if (!UnicodeUtils::IsValidBengaliSequence(fm.bengali_word)) continue;
                int dist = FastLevenshtein(roman_input, fm.roman_key);
                if (dist <= 2) {
                    float max_len = static_cast<float>(std::max(roman_input.size(), fm.roman_key.size()));
                    float sim = 1.0f - (static_cast<float>(dist) / max_len);
                    if (sim >= 0.65f) {
                        if (candidate_map.find(fm.bengali_word) == candidate_map.end()) {
                            ScoredCandidate sc;
                            sc.bengali_text = fm.bengali_word;
                            sc.roman_origin = fm.roman_key;
                            sc.phonetic_score = 0.85f * sim;
                            sc.unigram_score = std::min(1.0f, std::log10(static_cast<float>(fm.frequency) + 1.0f) / 6.0f);
                            sc.bigram_score = 0.0f;
                            sc.personal_score = 0.0f;
                            sc.category_flags = CANDIDATE_FLAG_PRIMARY;
                            sc.auto_correct_recommended = false;
                            candidate_map[fm.bengali_word] = sc;
                            has_dictionary_match = true;
                        } else {
                            if (candidate_map[fm.bengali_word].unigram_score == 0.0f) {
                                candidate_map[fm.bengali_word].unigram_score =
                                    std::min(1.0f, std::log10(static_cast<float>(fm.frequency) + 1.0f) / 6.0f);
                                has_dictionary_match = true;
                            }
                        }
                    }
                }
            }
        }
    }

    // 3. Ingest Personal Dictionary words for this roman key
    for (const auto& pair : user_dict_boosts) {
        if (candidate_map.find(pair.first) != candidate_map.end()) {
            candidate_map[pair.first].personal_score = pair.second;
            candidate_map[pair.first].category_flags |= CANDIDATE_FLAG_PERSONAL;
        }
    }

    // 4. Compute Unified Ranking Score & Suppress Non-Dictionary Junk
    for (auto& pair : candidate_map) {
        ScoredCandidate& sc = pair.second;
        sc.bigram_score = ComputeBigramProb(prev_word, sc.bengali_text);
        if (sc.bigram_score > 0.0f) {
            sc.category_flags |= CANDIDATE_FLAG_CONTEXTUAL;
        }

        float lexicon_presence_bonus = (sc.unigram_score > 0.0f) ? 0.15f : 0.0f;
        float personal_weight = (sc.personal_score >= 0.70f) ? 0.45f : 0.20f;
        float remaining_weight = 1.0f - personal_weight;

        float score = (remaining_weight * 0.45f * sc.phonetic_score) +
                      (remaining_weight * 0.40f * sc.unigram_score) +
                      (remaining_weight * 0.15f * sc.bigram_score) +
                      (personal_weight * sc.personal_score) +
                      lexicon_presence_bonus;

        if (has_dictionary_match && sc.unigram_score <= 0.0f && sc.personal_score <= 0.0f) {
            score *= 0.40f;
        }

        sc.final_score = std::min(1.0f, score);
    }

    // 5. Flatten and Sort
    std::vector<ScoredCandidate> sorted_candidates;
    sorted_candidates.reserve(candidate_map.size());
    for (auto& pair : candidate_map) {
        sorted_candidates.push_back(pair.second);
    }

    std::sort(sorted_candidates.begin(), sorted_candidates.end(), [](const ScoredCandidate& a, const ScoredCandidate& b) {
        return a.final_score > b.final_score;
    });

    // 6. Strict Auto-Correct Recommendation Gate
    if (auto_correct_enabled && !sorted_candidates.empty()) {
        float top_score = sorted_candidates[0].final_score;
        float second_score = (sorted_candidates.size() > 1) ? sorted_candidates[1].final_score : 0.0f;
        float margin = top_score - second_score;

        if (top_score >= (auto_correct_threshold - 1e-4f) && (margin >= 0.02f || sorted_candidates.size() == 1)) {
            sorted_candidates[0].auto_correct_recommended = true;
        } else {
            sorted_candidates[0].auto_correct_recommended = false;
        }
    } else if (!sorted_candidates.empty()) {
        sorted_candidates[0].auto_correct_recommended = false;
    }

    // 7. Ensure original English candidate is preserved in candidate list for mixed typing
    if (!roman_input.empty()) {
        bool has_raw = false;
        for (const auto& sc : sorted_candidates) {
            if (sc.bengali_text == roman_input) {
                has_raw = true;
                break;
            }
        }
        if (!has_raw) {
            ScoredCandidate raw_sc;
            raw_sc.bengali_text = roman_input;
            raw_sc.roman_origin = roman_input;
            raw_sc.phonetic_score = 0.5f;
            raw_sc.unigram_score = 0.0f;
            raw_sc.bigram_score = 0.0f;
            raw_sc.personal_score = 0.0f;
            raw_sc.final_score = 0.01f;
            raw_sc.category_flags = 0;
            raw_sc.auto_correct_recommended = false;
            if (sorted_candidates.size() < max_candidates) {
                sorted_candidates.push_back(raw_sc);
            } else if (!sorted_candidates.empty()) {
                sorted_candidates[max_candidates - 1] = raw_sc;
            }
        }
    }

    if (sorted_candidates.size() > max_candidates) {
        sorted_candidates.resize(max_candidates);
    }

    return sorted_candidates;
}

std::vector<ScoredCandidate> ContextRanker::PredictNextWords(
    const std::string& prev_word,
    const LexiconTrie& lexicon,
    size_t max_predictions
) const {
    std::vector<ScoredCandidate> predictions;
    if (prev_word.empty()) return predictions;

    auto it = bigrams_.find(prev_word);
    if (it != bigrams_.end()) {
        for (const auto& pair : it->second) {
            ScoredCandidate sc;
            sc.bengali_text = pair.first;
            sc.roman_origin = "";
            sc.phonetic_score = 0.0f;
            sc.unigram_score = 0.0f;
            sc.bigram_score = std::min(1.0f, std::log10(static_cast<float>(pair.second) + 1.0f) / 4.0f);
            sc.personal_score = 0.0f;
            sc.final_score = sc.bigram_score;
            sc.category_flags = CANDIDATE_FLAG_PREDICTION | CANDIDATE_FLAG_CONTEXTUAL;
            sc.auto_correct_recommended = false;

            const LexiconEntry* entry = lexicon.Find(pair.first);
            if (entry) {
                sc.unigram_score = std::min(1.0f, std::log10(static_cast<float>(entry->frequency) + 1.0f) / 6.0f);
                sc.final_score = (0.70f * sc.bigram_score) + (0.30f * sc.unigram_score);
            }

            predictions.push_back(sc);
        }
    }

    std::sort(predictions.begin(), predictions.end(), [](const ScoredCandidate& a, const ScoredCandidate& b) {
        return a.final_score > b.final_score;
    });

    if (predictions.size() > max_predictions) {
        predictions.resize(max_predictions);
    }
    return predictions;
}

} // namespace bangla
