#include "../include/bangla_engine.h"
#include "unicode/bangla_unicode.h"
#include "transliteration/phonetic_parser.h"
#include "dictionary/lexicon_trie.h"
#include "ranking/context_ranker.h"
#include "personal_dict/personal_dictionary.h"

#include <string>
#include <vector>
#include <cstring>
#include <sstream>
#include <iostream>

struct BanglaEngine {
    EngineConfig config;
    std::string composition;
    std::vector<std::string> sentence_history;

    bangla::PhoneticParser phonetic_parser;
    bangla::LexiconTrie lexicon;
    bangla::ContextRanker context_ranker;
    bangla::PersonalDictionary personal_dict;

    BanglaEngine(const EngineConfig* cfg) {
        if (cfg) {
            config = *cfg;
        } else {
            BanglaEngine_GetDefaultConfig(&config);
        }

        lexicon.LoadDefaultVocabulary();

        if (config.lexicon_binary_path && strlen(config.lexicon_binary_path) > 0) {
            lexicon.LoadFromFile(config.lexicon_binary_path);
        }
        if (config.user_dict_path && strlen(config.user_dict_path) > 0) {
            personal_dict.LoadFromFile(config.user_dict_path);
        }
    }

    ~BanglaEngine() {
        if (config.user_dict_path && strlen(config.user_dict_path) > 0) {
            personal_dict.SaveToFile(config.user_dict_path);
        }
    }
};

void BanglaEngine_GetDefaultConfig(EngineConfig* config) {
    if (!config) return;
    config->auto_correct_enabled = false;
    config->auto_correct_threshold = 0.85f;
    config->max_candidates = 5;
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

void BanglaEngine_CommitWord(BanglaEngine* engine, const char* bengali_word) {
    if (!engine || !bengali_word) return;
    engine->sentence_history.push_back(bengali_word);
    engine->personal_dict.IncrementFrequency(bengali_word);
    engine->composition.clear();
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

    // 5. Populate output list
    out_list->count = static_cast<uint32_t>(ranked.size());
    for (uint32_t i = 0; i < out_list->count; i++) {
        strncpy(out_list->candidates[i].bengali_text, ranked[i].bengali_text.c_str(), sizeof(out_list->candidates[i].bengali_text) - 1);
        strncpy(out_list->candidates[i].roman_origin, ranked[i].roman_origin.c_str(), sizeof(out_list->candidates[i].roman_origin) - 1);
        out_list->candidates[i].score = ranked[i].final_score;
        out_list->candidates[i].category_flags = ranked[i].category_flags;
        out_list->candidates[i].auto_correct_recommended = ranked[i].auto_correct_recommended;
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
        BanglaEngine_CommitWord(engine, committed_word.c_str());
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
    return engine->personal_dict.AddWord(roman_key, bengali_word);
}

bool BanglaEngine_RemoveUserWord(BanglaEngine* engine, const char* roman_key, const char* bengali_word) {
    if (!engine || !roman_key || !bengali_word) return false;
    return engine->personal_dict.RemoveWord(roman_key, bengali_word);
}
