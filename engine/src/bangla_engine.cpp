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
#include <cmath>
#include <sstream>
#include <iostream>
#include <unordered_map>
#include <fstream>
#include <chrono>

namespace {
// Lexicon entries compiled from engine/data/roman_overrides.txt carry this flag
// (set by build_lexicon.py). Exact roman-key matches for such entries are pinned
// to the top of the candidate list so the curated dictionary behaves like
// Google Input Tools: type the word once -> the intended spelling surfaces first.
constexpr uint16_t kLexiconOverrideFlag = 0x0001;
}

// --- File-static helpers (no BanglaEngine dependency) ----------------------

// Personal context bigrams live beside the user dictionary so they persist
// across sessions entirely OFFLINE. Derived name: user_bigrams.tsv in the
// same directory as the user TSV.
static std::string BigramsFilePathFor(const std::string& user_dict_path) {
    if (user_dict_path.empty()) return "";
    size_t pos = user_dict_path.find_last_of("\\/");
    std::string dir = (pos == std::string::npos) ? "" : user_dict_path.substr(0, pos + 1);
    return dir + "user_bigrams.tsv";
}

static uint64_t NowSeconds() {
    using namespace std::chrono;
    return static_cast<uint64_t>(duration_cast<seconds>(system_clock::now().time_since_epoch()).count());
}

// Gradual decay for unused preferences: 1.0 now, ~0.5 after 45 idle days,
// -> 0.3 after 90 days. Used only to order otherwise-equal habits (ties), so
// it can never erase a strongly held preference — it only lets a stale,
// equally-used habit fall behind a fresh one.
static float RecencyFactor(uint64_t last_used, uint64_t now) {
    if (last_used == 0 || now <= last_used) return 1.0f;
    double days = static_cast<double>(now - last_used) / 86400.0;
    return static_cast<float>(1.0 / (1.0 + days / 45.0));
}

struct BanglaEngine {
    EngineConfig config;
    std::string composition;
    std::vector<std::string> sentence_history;

    bangla::PhoneticParser phonetic_parser;
    bangla::LexiconTrie lexicon;
    bangla::ContextRanker context_ranker;
    bangla::PersonalDictionary personal_dict;

    // EngineConfig only stores raw const char* pointers; whoever passed the
    // paths may free them right after Create() returns (the CompositionManager
    // hands out local std::string storage). So the ENGINE OWNS copies of both
    // paths — reading or saving them later (e.g. the personal-dictionary save
    // in the destructor) must go through these members, never through config.
    std::string lexicon_path_;
    std::string user_dict_path_;

    // Personal learning state (LOCAL, offline-first).
    bool learning_enabled = true;         // "Pause learning" control
    std::string bigram_path_;             // user_bigrams.tsv beside user_dict
    std::unordered_map<std::string, std::unordered_map<std::string, uint32_t>> user_bigrams_;

    BanglaEngine(const EngineConfig* cfg) {
        if (cfg) {
            config = *cfg;
        } else {
            BanglaEngine_GetDefaultConfig(&config);
        }

        // Copy paths into engine-owned storage, then neutralize the raw
        // pointers inside config so nothing later dereferences a dangling one.
        if (config.lexicon_binary_path && strlen(config.lexicon_binary_path) > 0) {
            lexicon_path_ = config.lexicon_binary_path;
        }
        if (config.user_dict_path && strlen(config.user_dict_path) > 0) {
            user_dict_path_ = config.user_dict_path;
        }
        config.lexicon_binary_path = nullptr;
        config.user_dict_path = nullptr;

        lexicon.LoadDefaultVocabulary();

        if (!lexicon_path_.empty()) {
            lexicon.LoadFromFile(lexicon_path_);
        }
        if (!user_dict_path_.empty()) {
            personal_dict.LoadFromFile(user_dict_path_);
            bigram_path_ = BigramsFilePathFor(user_dict_path_);

            // Load the learned word-context bigrams (offline personal context).
            std::ifstream bin(bigram_path_);
            if (bin.is_open()) {
                std::string line;
                while (std::getline(bin, line)) {
                    if (line.empty()) continue;
                    std::stringstream ss(line);
                    std::string prev, cur;
                    uint32_t count = 0;
                    if (std::getline(ss, prev, '\t') && std::getline(ss, cur, '\t') && (ss >> count)) {
                        if (!prev.empty() && !cur.empty() && count > 0) {
                            user_bigrams_[prev][cur] += count;
                        }
                    }
                }
            }
        }
    }

    ~BanglaEngine() {
        if (!bigram_path_.empty()) {
            std::ofstream bout(bigram_path_);
            if (bout.is_open()) {
                for (const auto& outer : user_bigrams_) {
                    for (const auto& inner : outer.second) {
                        bout << outer.first << "\t" << inner.first << "\t" << inner.second << "\n";
                    }
                }
            }
        }
        if (!user_dict_path_.empty()) {
            personal_dict.SaveToFile(user_dict_path_);
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

    // 3. Retrieve personal user dictionary words mapped to this roman key.
    //    Boost grows with usage count (same log curve the ranker uses for
    //    unigrams), so the spelling the user actually commits MOST often ranks
    //    first — the core "intelligent" behaviour: the app learns your habit
    //    and defaults to it (Google-Input-Tools style).
    auto user_entries = engine->personal_dict.GetWordsByRomanKey(engine->composition);
    std::unordered_map<std::string, float> user_boosts;
    for (const auto& ue : user_entries) {
        float b = std::min(1.0f, 0.2f + std::log10(static_cast<float>(ue.frequency) + 1.0f) / 3.0f);
        user_boosts[ue.bengali_word] = b;
        bangla::PhoneticCandidate pc;
        pc.text = ue.bengali_word;
        pc.score = 1.0f;
        pc.rule_path = "personal_dict";
        phonetic_cands.push_back(pc);
    }

    // 3b. Learned word-context (bigram) boost: if the user has committed
    //     `candidate` right after the previous word before, raise that
    //     candidate's personal signal. Stored offline; purely a soft signal
    //     on top of the base ranking (never a hard reorder).
    if (!prev_word.empty() && !engine->user_bigrams_.empty()) {
        auto bit = engine->user_bigrams_.find(prev_word);
        if (bit != engine->user_bigrams_.end()) {
            for (const auto& ctx : bit->second) {
                float b = std::min(1.0f, 0.35f + std::log10(static_cast<float>(ctx.second) + 1.0f) / 4.0f);
                auto it = user_boosts.find(ctx.first);
                if (it == user_boosts.end() || b > it->second) user_boosts[ctx.first] = b;
            }
        }
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

    // 4b. Learned-habit lift with controlled evidence:
    //     only a spelling the user has committed at least kMinEvidence (2) times
    //     is lifted, most-used first. This guarantees that ONE accidental
    //     selection never reorders anything, while repeated choices take over.
    //     Equal-frequency habits are ordered by recency (unused old habits
    //     gradually decay). Curated overrides still outrank everything.
    constexpr uint32_t kMinEvidence = 2;
    if (!user_entries.empty() && !ranked.empty()) {
        uint64_t now = NowSeconds();
        std::vector<bangla::UserWordEntry> lifted;
        for (const auto& ue : user_entries) {
            if (ue.frequency >= kMinEvidence) lifted.push_back(ue);
        }
        if (!lifted.empty()) {
            std::stable_sort(lifted.begin(), lifted.end(),
                             [now](const bangla::UserWordEntry& a, const bangla::UserWordEntry& b) {
                                 if (a.frequency != b.frequency) return a.frequency > b.frequency;
                                 return RecencyFactor(a.last_used_timestamp, now) >
                                        RecencyFactor(b.last_used_timestamp, now);
                             });
            std::vector<std::string> pref_texts;
            for (const auto& ue : lifted) {
                if (std::find(pref_texts.begin(), pref_texts.end(), ue.bengali_word) == pref_texts.end()) {
                    pref_texts.push_back(ue.bengali_word);
                }
            }
            if (!pref_texts.empty()) {
                std::vector<bangla::ScoredCandidate> front, rest;
                front.reserve(ranked.size());
                rest.reserve(ranked.size());
                for (const auto& sc : ranked) {
                    bool is_pref = std::find(pref_texts.begin(), pref_texts.end(), sc.bengali_text) != pref_texts.end();
                    (is_pref ? front : rest).push_back(sc);
                }
                front.insert(front.end(), rest.begin(), rest.end());
                ranked.swap(front);
            }
        }
    }

    // 5. Curated override dictionary wins: any lexicon entry flagged as an
    //    override that exactly matches the typed roman key is emitted first,
    //    in lexicon frequency order (i.e. file order in roman_overrides.txt).
    std::string lower_composition = engine->composition;
    std::transform(lower_composition.begin(), lower_composition.end(), lower_composition.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    std::vector<std::string> override_texts;
    if (!lower_composition.empty()) {
        auto exact = engine->lexicon.SearchRoman(lower_composition, 8);
        for (const auto& rm : exact) {
            if ((rm.flags & kLexiconOverrideFlag) == 0) continue;
            if (std::find(override_texts.begin(), override_texts.end(), rm.bengali_word) == override_texts.end()) {
                override_texts.push_back(rm.bengali_word);
            }
        }
    }

    // 6. Populate output list: overrides first, then remaining ranked candidates.
    out_list->count = 0;
    float override_score = 1.0f;
    const uint32_t max_out = static_cast<uint32_t>(engine->config.max_candidates);
    for (const auto& ov_text : override_texts) {
        if (out_list->count >= max_out) break;
        strncpy(out_list->candidates[out_list->count].bengali_text, ov_text.c_str(), sizeof(out_list->candidates[out_list->count].bengali_text) - 1);
        strncpy(out_list->candidates[out_list->count].roman_origin, engine->composition.c_str(), sizeof(out_list->candidates[out_list->count].roman_origin) - 1);
        out_list->candidates[out_list->count].score = override_score;
        out_list->candidates[out_list->count].category_flags = CANDIDATE_FLAG_PRIMARY | CANDIDATE_FLAG_EXACT_MATCH;
        out_list->candidates[out_list->count].auto_correct_recommended =
            (engine->config.auto_correct_enabled && override_score >= engine->config.auto_correct_threshold - 1e-4f);
        out_list->count++;
        override_score -= 0.001f;
    }
    for (uint32_t i = 0; i < static_cast<uint32_t>(ranked.size()) && out_list->count < max_out; i++) {
        if (std::find(override_texts.begin(), override_texts.end(), ranked[i].bengali_text) != override_texts.end()) {
            continue; // already emitted as an override
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
    return engine->personal_dict.AddWord(roman_key, bengali_word, /*auto_learned=*/false);
}

bool BanglaEngine_RemoveUserWord(BanglaEngine* engine, const char* roman_key, const char* bengali_word) {
    if (!engine || !roman_key || !bengali_word) return false;
    return engine->personal_dict.RemoveWord(roman_key, bengali_word);
}

void BanglaEngine_SetLearningEnabled(BanglaEngine* engine, bool enabled) {
    if (engine) engine->learning_enabled = enabled;
}

bool BanglaEngine_IsLearningEnabled(const BanglaEngine* engine) {
    return engine ? engine->learning_enabled : false;
}

size_t BanglaEngine_ClearLearnedData(BanglaEngine* engine) {
    if (!engine) return 0;
    size_t removed = engine->personal_dict.ClearAutoLearned();
    engine->user_bigrams_.clear(); // context always comes from learning
    return removed;
}

size_t BanglaEngine_ClearUserWords(BanglaEngine* engine) {
    if (!engine) return 0;
    return engine->personal_dict.ClearExplicitUserWords();
}

void BanglaEngine_ClearAllPersonalData(BanglaEngine* engine) {
    if (!engine) return;
    engine->personal_dict.ClearAll();
    engine->user_bigrams_.clear();
}

bool BanglaEngine_LearnWord(BanglaEngine* engine, const char* roman_key, const char* bengali_word) {
    if (!engine || !roman_key || !bengali_word) return false;
    if (!engine->learning_enabled) return false; // "Pause learning" — no recording

    // Previous committed word = personal context for the (prev -> current) bigram.
    std::string prev;
    if (!engine->sentence_history.empty()) prev = engine->sentence_history.back();

    bool ok = engine->personal_dict.AddWord(roman_key, bengali_word, /*auto_learned=*/true);

    if (!prev.empty()) {
        engine->user_bigrams_[prev][bengali_word]++;
    }

    // N-gram context history (same as CommitWord) so next-word prediction and
    // the bigram boosts still see the sentence flow.
    engine->sentence_history.push_back(bengali_word);
    engine->composition.clear();
    return ok;
}

int BanglaEngine_GetUserWords(BanglaEngine* engine, const char* roman_key,
                              UserWordRef* out, int max_out) {
    if (!engine || !roman_key || !out || max_out <= 0) return 0;
    std::vector<bangla::UserWordEntry> list = engine->personal_dict.GetWordsByRomanKey(roman_key);
    // Most-used first — deterministic ordering for the caller.
    std::stable_sort(list.begin(), list.end(), [](const bangla::UserWordEntry& a,
                                                  const bangla::UserWordEntry& b) {
        return a.frequency > b.frequency;
    });
    int n = (int)std::min<size_t>(list.size(), static_cast<size_t>(max_out));
    for (int i = 0; i < n; i++) {
        strncpy(out[i].bengali_text, list[i].bengali_word.c_str(), sizeof(out[i].bengali_text) - 1);
        out[i].bengali_text[sizeof(out[i].bengali_text) - 1] = '\0';
        out[i].frequency = list[i].frequency;
    }
    return n;
}
