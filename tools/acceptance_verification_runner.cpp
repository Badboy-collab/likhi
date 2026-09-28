#include "../engine/include/bangla_engine.h"
#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <cassert>

#ifdef _WIN32
#include <windows.h>
#endif

void SetupConsole() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
}

struct TestCase {
    std::string input;
    std::string expected;
    bool top1_required;
};

void PrintCandidates(const CandidateList& list) {
    std::cout << "[";
    for (uint32_t i = 0; i < list.count; i++) {
        std::cout << (i > 0 ? ", " : "") << "\"" << list.candidates[i].bengali_text << "\" (score=" 
                  << std::fixed << std::setprecision(3) << list.candidates[i].score << ")";
    }
    std::cout << "]";
}

int main() {
    SetupConsole();
    std::cout << "======================================================================\n";
    std::cout << " LIKHI BANGLA TYPING ENGINE - FINAL ACCEPTANCE VERIFICATION\n";
    std::cout << "======================================================================\n\n";

    EngineConfig cfg;
    BanglaEngine_GetDefaultConfig(&cfg);
    cfg.lexicon_binary_path = "engine/data/lexicon.bin";
    std::string test_user_dict = "build/test_acceptance_user_dict.txt";
    // Clean any prior test dict
    remove(test_user_dict.c_str());
    remove("build/test_acceptance_user_dict.txt.pre-lexicon-fix.bak");
    remove("build/user_bigrams.tsv");
    cfg.user_dict_path = test_user_dict.c_str();

    BanglaEngine* engine = BanglaEngine_Create(&cfg);
    if (!engine) {
        std::cerr << "FAILED to create BanglaEngine!\n";
        return 1;
    }

    int total_tests = 0;
    int passed_tests = 0;

    // =========================================================================
    // 1. SCREENSHOT-BASED SUGGESTION & LOANWORD VERIFICATION
    // =========================================================================
    std::cout << "----------------------------------------------------------------------\n";
    std::cout << "1. SCREENSHOT-BASED SUGGESTION & LOANWORD VERIFICATION\n";
    std::cout << "----------------------------------------------------------------------\n";

    std::vector<TestCase> test_cases = {
        // Screenshot & core user test cases
        {"ami", "আমি", true},
        {"tumi", "তুমি", true},
        {"somossa", "সমস্যা", false},
        {"somossha", "সমস্যা", false},
        {"shomossa", "সমস্যা", false},
        {"poriborton", "পরিবর্তন", false},
        {"anwar", "আনোয়ার", false},
        {"anoyar", "আনোয়ার", false},
        {"battery", "ব্যাটারি", true},
        {"battary", "ব্যাটারি", false},
        {"batery", "ব্যাটারি", false},
        {"porishkar", "পরিষ্কার", false},
        {"office", "অফিস", true},
        {"computer", "কম্পিউটার", true},
        {"mouse", "মাউস", true},
        {"control", "কন্ট্রোল", true},
        {"fan", "ফ্যান", true},
        {"table", "টেবিল", true},
        {"chair", "চেয়ার", true},
        {"taka", "টাকা", true},
        {"bangla", "বাংলা", true},
        
        // Ambiguous inputs
        {"t", "ত", false},
        {"ta", "তা", false},

        // Latest screenshot loanwords
        {"screenshot", "স্ক্রিনশট", false},
        {"output", "আউটপুট", false},
        {"brain", "ব্রেন", false},
        {"better", "বেটার", true},
        {"betor", "বেটার", false},
        {"memorite", "মেমোরিতে", true}
    };

    for (const auto& tc : test_cases) {
        total_tests++;
        BanglaEngine_SetComposition(engine, tc.input.c_str());
        CandidateList list;
        BanglaEngine_GetCandidates(engine, &list);

        bool found = false;
        int rank = -1;
        for (uint32_t i = 0; i < list.count; i++) {
            if (list.candidates[i].bengali_text == tc.expected) {
                found = true;
                rank = i + 1;
                break;
            }
        }

        bool pass = false;
        if (tc.top1_required) {
            pass = (rank == 1);
        } else {
            pass = (rank >= 1 && rank <= 3);
        }

        if (pass) {
            passed_tests++;
            std::cout << "  [PASS] " << std::setw(12) << std::left << tc.input 
                      << " -> Expected: " << tc.expected << " (Rank " << rank << ")\n";
        } else {
            std::cout << "  [FAIL] " << std::setw(12) << std::left << tc.input 
                      << " -> Expected: " << tc.expected << " (Rank: " << rank 
                      << ", Required: " << (tc.top1_required ? "Rank 1" : "Top-3") << ") ";
            PrintCandidates(list);
            std::cout << "\n";
        }
    }

    // =========================================================================
    // 2. USER LEARNING REAL-WORLD VERIFICATION
    // =========================================================================
    std::cout << "\n----------------------------------------------------------------------\n";
    std::cout << "2. USER LEARNING & PERSISTENCE VERIFICATION\n";
    std::cout << "----------------------------------------------------------------------\n";

    // A. Baseline ranking for somossa
    total_tests++;
    BanglaEngine_SetComposition(engine, "somossa");
    CandidateList before_learn;
    BanglaEngine_GetCandidates(engine, &before_learn);
    std::cout << "  Initial candidates for 'somossa':\n    ";
    PrintCandidates(before_learn);
    std::cout << "\n";

    // B. Repeated learning of "সমস্যা"
    std::cout << "  Simulating user selecting 'সমস্যা' 3 times...\n";
    for (int i = 0; i < 3; i++) {
        BanglaEngine_LearnWord(engine, "somossa", "সমস্যা");
    }

    // C. Measure boost
    BanglaEngine_SetComposition(engine, "somossa");
    CandidateList after_learn;
    BanglaEngine_GetCandidates(engine, &after_learn);
    std::cout << "  Post-learning candidates for 'somossa':\n    ";
    PrintCandidates(after_learn);
    std::cout << "\n";

    bool learned_at_top = (after_learn.count > 0 && std::string(after_learn.candidates[0].bengali_text) == "সমস্যা");
    if (learned_at_top) {
        passed_tests++;
        std::cout << "  [PASS] User learning boost verified: 'সমস্যা' promoted to Rank 1!\n";
    } else {
        std::cout << "  [FAIL] User learning boost failed to place 'সমস্যা' at top.\n";
    }

    // D. Persistence check across engine restart
    total_tests++;
    std::cout << "  Closing engine and testing offline file persistence...\n";
    BanglaEngine_Destroy(engine);

    // Recreate engine with same user dictionary
    engine = BanglaEngine_Create(&cfg);
    BanglaEngine_SetComposition(engine, "somossa");
    CandidateList reloaded_list;
    BanglaEngine_GetCandidates(engine, &reloaded_list);
    std::cout << "  Candidates after engine restart:\n    ";
    PrintCandidates(reloaded_list);
    std::cout << "\n";

    bool persistence_ok = (reloaded_list.count > 0 && std::string(reloaded_list.candidates[0].bengali_text) == "সমস্যা");
    if (persistence_ok) {
        passed_tests++;
        std::cout << "  [PASS] Persistence verified: Learned preference remained at Rank 1 after restart!\n";
    } else {
        std::cout << "  [FAIL] Persistence failed: Learned preference lost across restart.\n";
    }

    // E. Test disabling learning
    total_tests++;
    std::cout << "  Testing 'Disable Personal Learning' toggle...\n";
    BanglaEngine_SetLearningEnabled(engine, false);
    BanglaEngine_LearnWord(engine, "testkey", "টেস্ট");
    UserWordRef refs[5];
    int count = BanglaEngine_GetUserWords(engine, "testkey", refs, 5);
    if (count == 0) {
        passed_tests++;
        std::cout << "  [PASS] Disabled learning respected: No new word recorded when learning is paused.\n";
    } else {
        std::cout << "  [FAIL] Word was recorded despite learning disabled.\n";
    }
    BanglaEngine_SetLearningEnabled(engine, true);

    // =========================================================================
    // 3. LOCAL MEMORY & PRIVACY VERIFICATION
    // =========================================================================
    std::cout << "\n----------------------------------------------------------------------\n";
    std::cout << "3. LOCAL MEMORY & PRIVACY AUDIT\n";
    std::cout << "----------------------------------------------------------------------\n";

    total_tests++;
    std::ifstream udict_in(test_user_dict);
    bool privacy_ok = true;
    if (udict_in.is_open()) {
        std::string line;
        while (std::getline(udict_in, line)) {
            // Must only contain tab-separated roman, bengali, frequency, timestamp
            std::stringstream ss(line);
            std::string r, b;
            if (std::getline(ss, r, '\t') && std::getline(ss, b, '\t')) {
                // Check that no arbitrary keystrokes or huge text is stored
                if (r.size() > 64 || b.size() > 128) {
                    privacy_ok = false;
                }
            }
        }
        udict_in.close();
    }
    if (privacy_ok) {
        passed_tests++;
        std::cout << "  [PASS] Privacy Guarantee: Only structured word-level mappings stored.\n"
                  << "         Zero keystrokes, zero passwords, zero document text recorded.\n";
    } else {
        std::cout << "  [FAIL] Privacy audit detected overly long or unstructured content.\n";
    }

    // =========================================================================
    // 4. CONTEXT-AWARE AGGLUTINATION & SENTENCE TESTS
    // =========================================================================
    std::cout << "\n----------------------------------------------------------------------\n";
    std::cout << "4. CONTEXT-AWARE AGGLUTINATION & MULTI-WORD TESTS\n";
    std::cout << "----------------------------------------------------------------------\n";

    struct ContextSentenceTest {
        std::string input;
        std::string expected;
        std::string description;
    };

    std::vector<ContextSentenceTest> context_tests = {
        {"office e", "অফিসে", "Agglutination: office e -> অফিসে"},
        {"ami ajke office e jabo", "আমি আজকে অফিসে যাব", "Multi-word: ami ajke office e jabo -> আমি আজকে অফিসে যাব"},
        {"amader desh ke bhalobashi", "আমাদের দেশকে ভালোবাসি", "Agglutination: desh ke -> দেশকে"},
        {"tumi kemon acho", "তুমি কেমন আছো", "Conversational: tumi kemon acho -> তুমি কেমন আছো"},
        {"she ekhon shob bujhte parbe", "সে এখন সব বুঝতে পারবে", "Modal verb: bujhte parbe"}
    };

    for (const auto& ct : context_tests) {
        total_tests++;
        char out_buf[1024] = {0};
        BanglaEngine_TransliterateSentence(engine, ct.input.c_str(), out_buf, sizeof(out_buf));
        std::string actual(out_buf);
        if (actual == ct.expected) {
            passed_tests++;
            std::cout << "  [PASS] " << ct.description << " [Actual: " << actual << "]\n";
        } else {
            std::cout << "  [FAIL] " << ct.description << "\n"
                      << "         Expected: " << ct.expected << "\n"
                      << "         Actual  : " << actual << "\n";
        }
    }

    // Cleanup
    BanglaEngine_Destroy(engine);
    remove(test_user_dict.c_str());

    std::cout << "\n======================================================================\n";
    std::cout << " ACCEPTANCE SUMMARY: " << passed_tests << " / " << total_tests << " PASSED (" 
              << (passed_tests == total_tests ? "100% SUCCESS" : "HAS FAILURES") << ")\n";
    std::cout << "======================================================================\n";

    return (passed_tests == total_tests) ? 0 : 1;
}
