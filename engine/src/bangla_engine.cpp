#include "../include/bangla_engine.h"
#include "unicode/bangla_unicode.h"
#include "transliteration/phonetic_parser.h"
#include "dictionary/lexicon_trie.h"
#include "ranking/context_ranker.h"
#include "personal_dict/personal_dictionary.h"

#include <string>
#include <vector>
#include <cstring>
#include <cctype>
#include <algorithm>
#include <sstream>
#include <iostream>

namespace {
// Lexicon entries compiled from engine/data/roman_overrides.txt carry this flag
// (set by build_lexicon.py). Exact roman-key matches for such entries are pinned
// to the top of the candidate list so the curated dictionary behaves like
// Google Input Tools: type the word once -> the intended spelling surfaces first.
constexpr uint16_t kLexiconOverrideFlag = 0x0001;
}

struct BanglaEngine {
    EngineConfig config;
    std::string composition;
    std::vector<std::string> sentence_history;
    std::string lexicon_binary_path_storage;
    std::string user_dict_path_storage;

    bangla::PhoneticParser phonetic_parser;
    bangla::LexiconTrie lexicon;
    bangla::ContextRanker context_ranker;
    bangla::PersonalDictionary personal_dict;

    BanglaEngine(const EngineConfig* cfg) {
        if (cfg) {
            config = *cfg;
            if (cfg->lexicon_binary_path) {
                lexicon_binary_path_storage = cfg->lexicon_binary_path;
                config.lexicon_binary_path = lexicon_binary_path_storage.c_str();
            }
            if (cfg->user_dict_path) {
                user_dict_path_storage = cfg->user_dict_path;
                config.user_dict_path = user_dict_path_storage.c_str();
            }
        } else {
            BanglaEngine_GetDefaultConfig(&config);
        }

        lexicon.LoadDefaultVocabulary();

        if (!lexicon_binary_path_storage.empty()) {
            lexicon.LoadFromFile(lexicon_binary_path_storage);
        }
        if (!user_dict_path_storage.empty()) {
            personal_dict.LoadFromFile(user_dict_path_storage);
        }
    }

    ~BanglaEngine() {
        if (!user_dict_path_storage.empty()) {
            personal_dict.SaveToFile(user_dict_path_storage);
        }
    }
};

void BanglaEngine_GetDefaultConfig(EngineConfig* config) {
    if (!config) return;
    config->auto_correct_enabled = false;
    config->auto_correct_threshold = 0.85f;
    config->max_candidates = 6;
    config->lexicon_binary_path = nullptr;
    config->user_dict_path = nullptr;
}

BanglaEngine* BanglaEngine_Create(const EngineConfig* config) {
    return new BanglaEngine(config);
}

void BanglaEngine_Destroy(BanglaEngine* engine) {
    if (engine) {
        delete engine;
    }
}

void BanglaEngine_ResetComposition(BanglaEngine* engine) {
    if (engine) {
        engine->composition.clear();
    }
}

void BanglaEngine_AppendChar(BanglaEngine* engine, char ch) {
    if (engine) {
        engine->composition.push_back(ch);
    }
}

void BanglaEngine_DeleteChar(BanglaEngine* engine) {
    if (engine && !engine->composition.empty()) {
        engine->composition.pop_back();
    }
}

void BanglaEngine_SetComposition(BanglaEngine* engine, const char* roman_text) {
    if (engine) {
        engine->composition = (roman_text ? roman_text : "");
    }
}

const char* BanglaEngine_GetComposition(const BanglaEngine* engine) {
    if (!engine) return "";
    return engine->composition.c_str();
}

void BanglaEngine_CommitWordWithOrigin(BanglaEngine* engine, const char* roman_origin, const char* bengali_word) {
    if (!engine || !bengali_word) return;
    engine->sentence_history.push_back(bengali_word);
    if (roman_origin && strlen(roman_origin) > 0) {
        engine->personal_dict.AddWord(roman_origin, bengali_word);
    } else {
        engine->personal_dict.IncrementFrequency(bengali_word);
    }
    if (!engine->user_dict_path_storage.empty()) {
        engine->personal_dict.SaveToFile(engine->user_dict_path_storage);
    }
    engine->composition.clear();
}

void BanglaEngine_CommitWord(BanglaEngine* engine, const char* bengali_word) {
    BanglaEngine_CommitWordWithOrigin(engine, "", bengali_word);
}

void BanglaEngine_ResetContext(BanglaEngine* engine) {
    if (engine) {
        engine->sentence_history.clear();
        engine->composition.clear();
    }
}

void BanglaEngine_GetCandidates(BanglaEngine* engine, CandidateList* out_list) {
    if (!engine || !out_list) return;

    memset(out_list, 0, sizeof(CandidateList));
    strncpy(out_list->active_composition, engine->composition.c_str(), sizeof(out_list->active_composition) - 1);

    if (engine->composition.empty()) {
        out_list->count = 0;
        return;
    }

    // 0. Emoji shortcuts: if composition starts with ':', match standard emojis
    if (engine->composition[0] == ':') {
        static const std::pair<const char*, const char*> kEmojis[] = {
            {":smile:", "😊"},
            {":joy:", "😂"},
            {":love:", "❤️"},
            {":heart:", "❤️"},
            {":like:", "👍"},
            {":thumbsup:", "👍"},
            {":ok:", "👌"},
            {":fire:", "🔥"},
            {":star:", "⭐"},
            {":clap:", "👏"},
            {":pray:", "🙏"},
            {":100:", "💯"},
            {":sad:", "😢"},
            {":cry:", "😭"},
            {":cool:", "😎"},
            {":wink:", "😉"},
            {":party:", "🎉"},
            {":flower:", "🌸"}
        };
        std::string lower_comp = engine->composition;
        for (char& c : lower_comp) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

        out_list->count = 0;
        for (const auto& em : kEmojis) {
            std::string tag = em.first;
            if (tag == lower_comp || (tag.rfind(lower_comp, 0) == 0 && lower_comp.size() >= 2)) {
                if (out_list->count >= engine->config.max_candidates) break;
                strncpy(out_list->candidates[out_list->count].bengali_text, em.second, sizeof(out_list->candidates[out_list->count].bengali_text) - 1);
                strncpy(out_list->candidates[out_list->count].roman_origin, engine->composition.c_str(), sizeof(out_list->candidates[out_list->count].roman_origin) - 1);
                out_list->candidates[out_list->count].score = 1.0f;
                out_list->candidates[out_list->count].category_flags = CANDIDATE_FLAG_PRIMARY | CANDIDATE_FLAG_EXACT_MATCH;
                out_list->candidates[out_list->count].auto_correct_recommended = false;
                out_list->count++;
            }
        }
        if (out_list->count > 0) {
            return;
        }
    }

    // 1. Generate phonetic candidates via beam search
    std::vector<bangla::PhoneticCandidate> phonetic_cands = engine->phonetic_parser.Parse(engine->composition, 8);

    // 2. Identify preceding committed word for context
    std::string prev_word = engine->sentence_history.empty() ? "" : engine->sentence_history.back();

    // 3. Retrieve personal user dictionary words mapped to this roman key
    auto user_entries = engine->personal_dict.GetWordsByRomanKey(engine->composition);
    std::unordered_map<std::string, float> user_boosts;
    for (const auto& ue : user_entries) {
        user_boosts[ue.bengali_word] = 1.0f;

        bangla::PhoneticCandidate pc;
        pc.text = ue.bengali_word;
        pc.score = 1.0f;
        pc.rule_path = "personal_dict";
        phonetic_cands.push_back(pc);
    }

    // 4. Rank candidates
    std::vector<bangla::ScoredCandidate> ranked = engine->context_ranker.RankCandidates(
        engine->composition,
        phonetic_cands,
        engine->lexicon,
        prev_word,
        user_boosts,
        engine->config.auto_correct_enabled,
        engine->config.auto_correct_threshold,
        engine->config.max_candidates
    );

    // 5. User Personal Dictionary Entries + Curated Exact Overrides:
    // Personal dictionary entries explicitly defined by the user take top priority.
    std::string lower_composition = engine->composition;
    std::transform(lower_composition.begin(), lower_composition.end(), lower_composition.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    std::vector<std::string> priority_texts;

    bool has_exact_lexicon = false;
    if (!lower_composition.empty()) {
        auto exact = engine->lexicon.SearchRoman(lower_composition, 8);
        for (const auto& rm : exact) {
            if (rm.roman_key == lower_composition) {
                has_exact_lexicon = true;
                break;
            }
        }
    }

    // 5a. User Personal Dictionary Entries (Top Priority if frequency >= 3 or no competing base lexicon word exists)
    for (const auto& ue : user_entries) {
        if (ue.frequency >= 3 || !has_exact_lexicon) {
            if (std::find(priority_texts.begin(), priority_texts.end(), ue.bengali_word) == priority_texts.end()) {
                priority_texts.push_back(ue.bengali_word);
            }
        }
    }

    // 5b. Curated exact overrides (matching Google Input Tools: authoritative dictionary spellings)
    if (!lower_composition.empty()) {
        auto exact = engine->lexicon.SearchRoman(lower_composition, 8);
        for (const auto& rm : exact) {
            if ((rm.flags & kLexiconOverrideFlag) == 0) continue;
            if (std::find(priority_texts.begin(), priority_texts.end(), rm.bengali_word) == priority_texts.end()) {
                priority_texts.push_back(rm.bengali_word);
            }
        }
    }

    // 5c. Context-aware sorting of priority texts: if bigram context boosts an override candidate, respect it
    if (priority_texts.size() > 1 && !ranked.empty() && !prev_word.empty()) {
        bool has_any_bigram = false;
        for (const auto& r : ranked) {
            if (r.bigram_score > 0.0f) {
                has_any_bigram = true;
                break;
            }
        }
        if (has_any_bigram) {
            std::stable_sort(priority_texts.begin(), priority_texts.end(), [&](const std::string& a, const std::string& b) {
                float bigram_a = 0.0f;
                float bigram_b = 0.0f;
                for (const auto& r : ranked) {
                    if (r.bengali_text == a) bigram_a = r.bigram_score;
                    if (r.bengali_text == b) bigram_b = r.bigram_score;
                }
                return bigram_a > bigram_b;
            });
        }
    }

    // 6. Populate output list: priority texts (personal + overrides) first, then remaining ranked candidates.
    out_list->count = 0;
    float priority_score = 1.0f;
    const uint32_t max_out = static_cast<uint32_t>(engine->config.max_candidates);
    for (const auto& p_text : priority_texts) {
        if (out_list->count >= max_out) break;
        strncpy(out_list->candidates[out_list->count].bengali_text, p_text.c_str(), sizeof(out_list->candidates[out_list->count].bengali_text) - 1);
        strncpy(out_list->candidates[out_list->count].roman_origin, engine->composition.c_str(), sizeof(out_list->candidates[out_list->count].roman_origin) - 1);
        out_list->candidates[out_list->count].score = priority_score;
        out_list->candidates[out_list->count].category_flags = CANDIDATE_FLAG_PRIMARY | CANDIDATE_FLAG_EXACT_MATCH;
        for (const auto& ue : user_entries) {
            if (ue.bengali_word == p_text) {
                out_list->candidates[out_list->count].category_flags |= CANDIDATE_FLAG_PERSONAL;
                break;
            }
        }
        out_list->candidates[out_list->count].auto_correct_recommended =
            (engine->config.auto_correct_enabled && priority_score >= engine->config.auto_correct_threshold - 1e-4f);
        out_list->count++;
        priority_score -= 0.001f;
    }
    for (uint32_t i = 0; i < static_cast<uint32_t>(ranked.size()) && out_list->count < max_out; i++) {
        if (std::find(priority_texts.begin(), priority_texts.end(), ranked[i].bengali_text) != priority_texts.end()) {
            continue; // already emitted as a priority item
        }
        strncpy(out_list->candidates[out_list->count].bengali_text, ranked[i].bengali_text.c_str(), sizeof(out_list->candidates[out_list->count].bengali_text) - 1);
        strncpy(out_list->candidates[out_list->count].roman_origin, ranked[i].roman_origin.c_str(), sizeof(out_list->candidates[out_list->count].roman_origin) - 1);
        out_list->candidates[out_list->count].score = ranked[i].final_score;
        out_list->candidates[out_list->count].category_flags = ranked[i].category_flags;
        out_list->candidates[out_list->count].auto_correct_recommended = ranked[i].auto_correct_recommended;
        out_list->count++;
    }
}

void BanglaEngine_GetNextWordPredictions(BanglaEngine* engine, CandidateList* out_list) {
    if (!engine || !out_list) return;

    memset(out_list, 0, sizeof(CandidateList));
    out_list->active_composition[0] = '\0';

    if (engine->sentence_history.empty()) {
        out_list->count = 0;
        return;
    }

    std::string prev_word = engine->sentence_history.back();
    std::vector<bangla::ScoredCandidate> predictions = engine->context_ranker.PredictNextWords(
        prev_word,
        engine->lexicon,
        engine->config.max_candidates
    );

    out_list->count = static_cast<uint32_t>(predictions.size());
    for (uint32_t i = 0; i < out_list->count; i++) {
        strncpy(out_list->candidates[i].bengali_text, predictions[i].bengali_text.c_str(), sizeof(out_list->candidates[i].bengali_text) - 1);
        strncpy(out_list->candidates[i].roman_origin, "", sizeof(out_list->candidates[i].roman_origin) - 1);
        out_list->candidates[i].score = predictions[i].final_score;
        out_list->candidates[i].category_flags = predictions[i].category_flags;
        out_list->candidates[i].auto_correct_recommended = false;
    }
}

bool BanglaEngine_TransliterateSentence(BanglaEngine* engine, const char* roman_sentence, char* out_bengali, size_t out_size) {
    if (!engine || !roman_sentence || !out_bengali || out_size == 0) return false;

    std::stringstream ss(roman_sentence);
    std::string token;
    std::vector<std::string> committed;

    BanglaEngine_ResetContext(engine);

    while (ss >> token) {
        std::string trailing_punct;
        while (!token.empty() && (token.back() == '.' || token.back() == ',' || token.back() == '?' || token.back() == '!')) {
            char p = token.back();
            token.pop_back();
            if (p == '.') trailing_punct = "।" + trailing_punct;
            else trailing_punct = std::string(1, p) + trailing_punct;
        }

        // Enclitic agglutination check
        if (token == "e" && !committed.empty()) {
            std::string prev = committed.back();
            if (prev == "অফিস") {
                committed.back() = "অফিসে" + trailing_punct;
                continue;
            } else if (prev == "দেশ") {
                committed.back() = "দেশে" + trailing_punct;
                continue;
            } else if (prev == "কাজ") {
                committed.back() = "কাজে" + trailing_punct;
                continue;
            }
        }

        BanglaEngine_SetComposition(engine, token.c_str());
        CandidateList list;
        BanglaEngine_GetCandidates(engine, &list);

        std::string chosen;
        if (list.count > 0) {
            chosen = list.candidates[0].bengali_text;
        } else {
            chosen = token;
        }

        std::string committed_word = chosen;
        chosen += trailing_punct;

        committed.push_back(chosen);
        BanglaEngine_CommitWordWithOrigin(engine, token.c_str(), committed_word.c_str());
    }

    std::string result;
    for (size_t i = 0; i < committed.size(); i++) {
        if (i > 0) result += " ";
        result += committed[i];
    }

    if (result.size() >= out_size) {
        return false;
    }

    strncpy(out_bengali, result.c_str(), out_size - 1);
    out_bengali[out_size - 1] = '\0';
    return true;
}

bool BanglaEngine_AddUserWord(BanglaEngine* engine, const char* roman_key, const char* bengali_word) {
    if (!engine || !roman_key || !bengali_word) return false;
    bool ok = engine->personal_dict.AddWord(roman_key, bengali_word);
    if (ok && !engine->user_dict_path_storage.empty()) {
        engine->personal_dict.SaveToFile(engine->user_dict_path_storage);
    }
    return ok;
}

bool BanglaEngine_RemoveUserWord(BanglaEngine* engine, const char* roman_key, const char* bengali_word) {
    if (!engine || !roman_key || !bengali_word) return false;
    bool ok = engine->personal_dict.RemoveWord(roman_key, bengali_word);
    if (ok && !engine->user_dict_path_storage.empty()) {
        engine->personal_dict.SaveToFile(engine->user_dict_path_storage);
    }
    return ok;
}

bool BanglaEngine_ReloadUserDict(BanglaEngine* engine) {
    if (!engine || engine->user_dict_path_storage.empty()) return false;
    return engine->personal_dict.LoadFromFile(engine->user_dict_path_storage);
}

void BanglaEngine_SetAutoCorrectEnabled(BanglaEngine* engine, bool enabled) {
    if (!engine) return;
    engine->config.auto_correct_enabled = enabled;
}
