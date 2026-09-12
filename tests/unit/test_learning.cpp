// Unit tests for the personal learning engine: usage-count ranking, learned
// candidates surfacing first, and file persistence across engine instances.
// Uses a throwaway TSV file in the working (build) directory.
#include <bangla_engine.h>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <iostream>
#include <fstream>

static const char* kUserFile = "learning_test_user.txt";

static int g_fail = 0;
static int g_total = 0;

static void Check(bool cond, const char* what) {
    g_total++;
    if (!cond) {
        g_fail++;
        std::cout << "  [FAIL] " << what << "\n";
    }
}

static std::string TopCandidate(BanglaEngine* e, const char* roman) {
    BanglaEngine_SetComposition(e, roman);
    CandidateList list;
    BanglaEngine_GetCandidates(e, &list);
    if (list.count == 0) return "";
    return list.candidates[0].bengali_text;
}

int main() {
    std::remove(kUserFile);

    EngineConfig cfg;
    BanglaEngine_GetDefaultConfig(&cfg);
    cfg.user_dict_path = kUserFile;

    // --- Fresh engine: nothing learned yet -----------------------------
    BanglaEngine* e1 = BanglaEngine_Create(&cfg);
    UserWordRef buf[16];
    int n = BanglaEngine_GetUserWords(e1, "xamar", buf, 16);
    Check(n == 0, "no learned words on a fresh engine");

    // --- Learn 'আমার' for 'xamar' 4 times, 'আমরা' once -----------------
    for (int i = 0; i < 4; i++) BanglaEngine_LearnWord(e1, "xamar", "আমার");
    BanglaEngine_LearnWord(e1, "xamar", "আমরা");

    n = BanglaEngine_GetUserWords(e1, "xamar", buf, 16);
    Check(n == 2, "two learned spellings stored");
    Check(n >= 2 && buf[0].frequency == 4 && std::string(buf[0].bengali_text) == "আমার",
          "most-used spelling listed first (4 > 1)");

    // --- Learned most-used spelling surfaces as candidate #1 -------------
    Check(TopCandidate(e1, "xamar") == "আমার", "top candidate is the most-used learned word");
    BanglaEngine_SetComposition(e1, "xamar");
    CandidateList list;
    BanglaEngine_GetCandidates(e1, &list);
    bool found_amra = false;
    for (uint32_t i = 0; i < list.count; i++) {
        if (std::string(list.candidates[i].bengali_text) == "আমরা") found_amra = true;
    }
    Check(found_amra, "second learned spelling still offered");

    // --- Flip the habit: 'আমরা' becomes the dominant choice -------------
    for (int i = 0; i < 9; i++) BanglaEngine_LearnWord(e1, "xamar", "আমরা");
    n = BanglaEngine_GetUserWords(e1, "xamar", buf, 16);
    Check(n == 2 && buf[0].frequency == 10 && std::string(buf[0].bengali_text) == "আমরা",
          "ranking flipped: আমরা (10) now first, আমার (4) second");
    Check(TopCandidate(e1, "xamar") == "আমরা", "top candidate follows the new habit");

    // --- Persistence: destroy saves, a new engine loads -----------------
    BanglaEngine_Destroy(e1);

    BanglaEngine* e2 = BanglaEngine_Create(&cfg); // loads kUserFile
    n = BanglaEngine_GetUserWords(e2, "xamar", buf, 16);
    Check(n == 2 && std::string(buf[0].bengali_text) == "আমরা",
          "learned data persisted across engine instances");
    Check(TopCandidate(e2, "xamar") == "আমরা", "persisted habit still ranks first");
    BanglaEngine_Destroy(e2);

    // --- Personal context (bigram) file was persisted offline -------------
    {
        std::ifstream bigrams("user_bigrams.tsv");
        std::string first_line;
        bool has_line = static_cast<bool>(std::getline(bigrams, first_line)) && !first_line.empty();
        Check(has_line, "personal context bigrams persisted to user_bigrams.tsv");
    }

    // --- Learning pause / resume ---------------------------------------
    BanglaEngine* e3 = BanglaEngine_Create(&cfg); // loads persisted file
    BanglaEngine_SetLearningEnabled(e3, false);
    Check(!BanglaEngine_IsLearningEnabled(e3), "learning reports paused");
    BanglaEngine_LearnWord(e3, "xnotun", "নতুন");
    Check(BanglaEngine_GetUserWords(e3, "xnotun", buf, 16) == 0,
          "paused learning records nothing");
    BanglaEngine_SetLearningEnabled(e3, true);
    BanglaEngine_LearnWord(e3, "xnotun", "নতুন");
    Check(BanglaEngine_GetUserWords(e3, "xnotun", buf, 16) == 1,
          "learning resumes after unpause");

    // --- Source separation: explicit words survive 'Clear learned data' ----
    BanglaEngine_AddUserWord(e3, "xmydict", "আমারশব্দ");
    BanglaEngine_LearnWord(e3, "xlearn", "শেখাশব্দ");
    size_t cleared = BanglaEngine_ClearLearnedData(e3);
    Check(cleared >= 1, "clear learned removed auto-learned rows");
    Check(BanglaEngine_GetUserWords(e3, "xlearn", buf, 16) == 0,
          "auto-learned row gone after clear");
    Check(BanglaEngine_GetUserWords(e3, "xmydict", buf, 16) == 1,
          "explicit user-added word preserved by 'clear learned'");
    size_t cleared2 = BanglaEngine_ClearUserWords(e3);
    Check(cleared2 >= 1, "clear user words removed explicit rows");
    Check(BanglaEngine_GetUserWords(e3, "xmydict", buf, 16) == 0,
          "explicit row gone after clear");
    BanglaEngine_Destroy(e3);

    // --- Personal context (bigram) file was persisted offline -------------
    BanglaEngine* e4 = BanglaEngine_Create(&cfg); // re-loads words + bigrams
    Check(e4 != nullptr, "engine reloads cleanly after clearing learned data");
    BanglaEngine_Destroy(e4);

    std::remove(kUserFile);
    std::remove("user_bigrams.tsv");

    std::cout << "test_learning: " << (g_total - g_fail) << "/" << g_total << " passed\n";
    if (g_fail == 0) {
        std::cout << "Status: SUCCESS (ALL PASSED)\n";
        return 0;
    }
    std::cout << "Status: FAILURE (" << g_fail << " failed)\n";
    return 1;
}
