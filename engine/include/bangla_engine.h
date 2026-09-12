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
    uint32_t max_candidates;          // Maximum candidate suggestions to return (default: 5)
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
void BanglaEngine_ResetContext(BanglaEngine* engine);

// Full Sentence Transliteration
bool BanglaEngine_TransliterateSentence(BanglaEngine* engine, const char* roman_sentence, char* out_bengali, size_t out_size);

// Personal User Dictionary Operations
bool BanglaEngine_AddUserWord(BanglaEngine* engine, const char* roman_key, const char* bengali_word);
bool BanglaEngine_RemoveUserWord(BanglaEngine* engine, const char* roman_key, const char* bengali_word);

// A roman word mapped to one user-chosen Bengali spelling plus how many times
// the user actually committed it (usage count drives learned ranking).
typedef struct {
    char bengali_text[MAX_WORD_BYTES];
    uint32_t frequency;
} UserWordRef;

// Personal learning policy — separate controls, LOCAL first.
// Learning is OFF only when the user pauses it; explicit user additions
// (BanglaEngine_AddUserWord) are never affected by the learning flag.
void BanglaEngine_SetLearningEnabled(BanglaEngine* engine, bool enabled);
bool BanglaEngine_IsLearningEnabled(const BanglaEngine* engine);

// Clear controls: learned-from-typing data only / explicit user-added words
// only / everything. Return the number of entries removed.
size_t BanglaEngine_ClearLearnedData(BanglaEngine* engine);
size_t BanglaEngine_ClearUserWords(BanglaEngine* engine);
void BanglaEngine_ClearAllPersonalData(BanglaEngine* engine);

// Personal learning (used by the live TSF commit path): records that the user
// chose `bengali_word` for `roman_key`, increments its usage count, stores the
// (previous word -> word) context bigram, and feeds the N-gram history.
// No-op while learning is paused. Persisted when the engine is destroyed.
bool BanglaEngine_LearnWord(BanglaEngine* engine, const char* roman_key, const char* bengali_word);

// Returns the user's own spellings for `roman_key`, most-used first. Returns
// the number of entries copied (0 if none). `out` may hold up to `max_out`.
int BanglaEngine_GetUserWords(BanglaEngine* engine, const char* roman_key,
                              UserWordRef* out, int max_out);

#ifdef __cplusplus
}
#endif

#endif // BANGLA_ENGINE_H
