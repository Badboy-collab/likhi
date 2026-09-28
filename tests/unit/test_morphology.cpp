#include "bangla_engine.h"
#include <iostream>
#include <vector>
#include <string>
#include <cassert>
#include <cstring>

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
    std::string roman_input;
    std::string expected_top1;
    std::string label;
};

int main() {
    SetupConsole();
    std::cout << "======================================================================\n";
    std::cout << " LIKHI BANGLISH MORPHOLOGY REGRESSION SUITE (Release Blocker 1)\n";
    std::cout << "======================================================================\n\n";

    EngineConfig cfg;
    BanglaEngine_GetDefaultConfig(&cfg);
    cfg.lexicon_binary_path = "engine/data/lexicon.bin";
    BanglaEngine* engine = BanglaEngine_Create(&cfg);
    if (!engine) {
        std::cerr << "[CRITICAL] Cannot create BanglaEngine!\n";
        return 1;
    }

    std::vector<TestCase> test_cases = {
        // Hyphenated foreign/loanword and native stems
        {"Google-er",  "গুগলের",   "Genitive consonant stem (Google-er -> গুগলের)"},
        {"pele-o",     "পেলেও",    "Emphatic verb stem (pele-o -> পেলেও)"},
        {"Likhi-ke",   "লিখিকে",   "Accusative stem (Likhi-ke -> লিখিকে)"},
        {"office-e",   "অফিসে",    "Locative consonant stem (office-e -> অফিসে)"},
        {"desh-ke",    "দেশকে",    "Accusative native stem (desh-ke -> দেশকে)"},
        {"manush-er",  "মানুষের",  "Genitive native stem (manush-er -> মানুষের)"},
        {"Dhaka-r",    "ঢাকার",    "Genitive vowel-kar stem (Dhaka-r -> ঢাকার)"},
        {"manush-der", "মানুষদের", "Plural suffix (manush-der -> মানুষদের)"},
        {"boi-gulo",   "বইগুলো",   "Plural classifier (boi-gulo -> বইগুলো)"},
        {"Likhi-ti",   "লিখিটি",   "Definiteness marker (Likhi-ti -> লিখিটি)"},
        {"gele-o",     "গেলেও",    "Emphatic verb stem (gele-o -> গেলেও)"},
        {"korle-o",    "করলেও",    "Emphatic verb stem (korle-o -> করলেও)"},

        // Compound unhyphenated forms
        {"geleo",      "গেলেও",    "Compound unhyphenated verb (geleo -> গেলেও)"},
        {"korleo",     "করলেও",    "Compound unhyphenated verb (korleo -> করলেও)"},
        {"peleo",      "পেলেও",    "Compound unhyphenated verb (peleo -> পেলেও)"},

        // Regular vocabulary words
        {"amar",       "আমার",     "Standard pronoun (amar -> আমার)"},
        {"tomar",      "তোমার",    "Standard pronoun (tomar -> তোমার)"},
        {"ajke",       "আজকে",     "Temporal adverb (ajke -> আজকে)"}
    };

    int passed = 0;
    int failed = 0;

    for (size_t i = 0; i < test_cases.size(); ++i) {
        const auto& tc = test_cases[i];
        BanglaEngine_SetComposition(engine, tc.roman_input.c_str());
        CandidateList list;
        BanglaEngine_GetCandidates(engine, &list);

        std::string actual_top1 = (list.count > 0) ? list.candidates[0].bengali_text : "";

        if (actual_top1 == tc.expected_top1) {
            std::cout << "  [PASS] " << tc.label << " => " << actual_top1 << "\n";
            passed++;
        } else {
            std::cout << "  [FAIL] " << tc.label << "\n";
            std::cout << "         Expected: " << tc.expected_top1 << "\n";
            std::cout << "         Actual:   " << actual_top1 << "\n";
            std::cout << "         All candidates: ";
            for (uint32_t c = 0; c < list.count; ++c) {
                std::cout << list.candidates[c].bengali_text << " (" << list.candidates[c].score << ") ";
            }
            std::cout << "\n";
            failed++;
        }
    }

    std::cout << "\n--- Sentence Transliteration Checks ---\n";
    {
        char out_buf[512];
        std::memset(out_buf, 0, sizeof(out_buf));

        // Test 1
        BanglaEngine_TransliterateSentence(engine, "Google-er shob kisu bhalo.", out_buf, sizeof(out_buf));
        std::string s1 = out_buf;
        if (s1.find("গুগলের") != std::string::npos) {
            std::cout << "  [PASS] Sentence with Google-er: " << s1 << "\n";
            passed++;
        } else {
            std::cout << "  [FAIL] Sentence with Google-er: " << s1 << " (expected to contain গুগলের)\n";
            failed++;
        }

        // Test 2
        std::memset(out_buf, 0, sizeof(out_buf));
        BanglaEngine_TransliterateSentence(engine, "Likhi-ke amar khub pochondo.", out_buf, sizeof(out_buf));
        std::string s2 = out_buf;
        if (s2.find("লিখিকে") != std::string::npos) {
            std::cout << "  [PASS] Sentence with Likhi-ke: " << s2 << "\n";
            passed++;
        } else {
            std::cout << "  [FAIL] Sentence with Likhi-ke: " << s2 << " (expected to contain লিখিকে)\n";
            failed++;
        }

        // Test 3
        std::memset(out_buf, 0, sizeof(out_buf));
        BanglaEngine_TransliterateSentence(engine, "pele-o ami jabo na.", out_buf, sizeof(out_buf));
        std::string s3 = out_buf;
        if (s3.find("পেলেও") != std::string::npos) {
            std::cout << "  [PASS] Sentence with pele-o: " << s3 << "\n";
            passed++;
        } else {
            std::cout << "  [FAIL] Sentence with pele-o: " << s3 << " (expected to contain পেলেও)\n";
            failed++;
        }
    }

    BanglaEngine_Destroy(engine);

    std::cout << "\n======================================================================\n";
    std::cout << " Morphology Results: " << passed << " passed, " << failed << " failed.\n";
    std::cout << "======================================================================\n";

    return (failed == 0) ? 0 : 1;
}
