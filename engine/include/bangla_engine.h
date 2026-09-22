#ifndef BANGLA_ENGINE_H
#define BANGLA_ENGINE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Candidate Category & Status Flags
#define CANDIDATE_FLAG_PRIMARY        0x0001
#define CANDIDATE_FLAG_EXACT_MATCH    0x0002
#define CANDIDATE_FLAG_CONTEXTUAL     0x0004
#define CANDIDATE_FLAG_PERSONAL       0x0008
#define CANDIDATE_FLAG_PREDICTION     0x0010

// Maximum limits
#define MAX_CANDIDATES_COUNT          16
#define MAX_WORD_BYTES                128
#define MAX_COMPOSITION_BYTES         256

// Engine Configuration Options
typedef struct {
    bool auto_correct_enabled;        // Default: false (strictly off by default)
    float auto_correct_threshold;     // Confidence threshold for auto-correct (default: 0.85)
    uint32_t max_candidates;          // Maximum candidate suggestions to return (default: 6)
    const char* lexicon_binary_path;  // Optional custom path to compiled lexicon binary
    const char* user_dict_path;       // Optional custom path to SQLite/binary user dictionary
} EngineConfig;

// A single transliteration / prediction candidate
typedef struct {
    char bengali_text[MAX_WORD_BYTES];
    char roman_origin[MAX_WORD_BYTES];
    float score;                      // Deterministic normalized confidence / ranking score (0.0 - 1.0)
    uint32_t category_flags;          // Bitwise combination of CANDIDATE_FLAG_*
    bool auto_correct_recommended;    // True if engine suggests auto-replacing text
} Candidate;

// Result container returned by candidate queries
typedef struct {
    Candidate candidates[MAX_CANDIDATES_COUNT];
    uint32_t count;
    char active_composition[MAX_COMPOSITION_BYTES];
} CandidateList;

// Opaque Engine Instance Handle
typedef struct BanglaEngine BanglaEngine;

// Lifecycle & Configuration Management
void BanglaEngine_GetDefaultConfig(EngineConfig* config);
BanglaEngine* BanglaEngine_Create(const EngineConfig* config);
void BanglaEngine_Destroy(BanglaEngine* engine);

// Composition Buffer Operations
void BanglaEngine_ResetComposition(BanglaEngine* engine);
void BanglaEngine_AppendChar(BanglaEngine* engine, char ch);
void BanglaEngine_DeleteChar(BanglaEngine* engine);
void BanglaEngine_SetComposition(BanglaEngine* engine, const char* roman_text);
const char* BanglaEngine_GetComposition(const BanglaEngine* engine);

// Candidate Retrieval & Context State
void BanglaEngine_GetCandidates(BanglaEngine* engine, CandidateList* out_list);
void BanglaEngine_GetNextWordPredictions(BanglaEngine* engine, CandidateList* out_list);
void BanglaEngine_CommitWord(BanglaEngine* engine, const char* bengali_word);
void BanglaEngine_CommitWordWithOrigin(BanglaEngine* engine, const char* roman_origin, const char* bengali_word);
void BanglaEngine_ResetContext(BanglaEngine* engine);

// Full Sentence Transliteration
bool BanglaEngine_TransliterateSentence(BanglaEngine* engine, const char* roman_sentence, char* out_bengali, size_t out_size);

// Personal User Dictionary Operations
bool BanglaEngine_AddUserWord(BanglaEngine* engine, const char* roman_key, const char* bengali_word);
bool BanglaEngine_RemoveUserWord(BanglaEngine* engine, const char* roman_key, const char* bengali_word);
bool BanglaEngine_ReloadUserDict(BanglaEngine* engine);
void BanglaEngine_SetAutoCorrectEnabled(BanglaEngine* engine, bool enabled);
void BanglaEngine_SetMaxCandidates(BanglaEngine* engine, uint32_t max_candidates);

#ifdef __cplusplus
}
#endif

#endif // BANGLA_ENGINE_H
