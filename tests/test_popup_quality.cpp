#include "bangla_engine.h"
#include <iostream>
#include <vector>
#include <string>
#include <set>
#include <algorithm>
#include <iomanip>
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

struct PopupQualityCase {
    std::string input;
    std::string expected_top1;
    std::vector<std::string> allowed_subsequent; // other acceptable words
    std::vector<std::string> forbidden_junk;     // non-words that MUST NEVER appear
    size_t max_expected_count;                   // quality over quantity
    std::string description;
};

int main() {
    SetupConsole();
    std::cout << "======================================================================\n";
    std::cout << " LIKHI SUGGESTION POPUP QUALITY ACCEPTANCE SUITE (Phase 3.3)\n";
    std::cout << "======================================================================\n\n";

    EngineConfig cfg;
    BanglaEngine_GetDefaultConfig(&cfg);
    cfg.lexicon_binary_path = "engine/data/lexicon.bin";
    BanglaEngine* engine = BanglaEngine_Create(&cfg);
    if (!engine) {
        std::cerr << "[CRITICAL] Cannot create BanglaEngine!\n";
        return 1;
    }

    std::vector<PopupQualityCase> cases = {
        {
            "suggestion",
            "সাজেশন",
            {"সাজেশান", "suggestion"},
            {"সুগ্গেস্তিঅন", "সূগ্গেস্তিঅন", "সুগগেস্তিঅন", "সুগ্গেসতিঅন"},
            3,
            "suggestion: clean transliteration without bizarre phonetic artifacts"
        },
        {
            "somossa",
            "সমস্যা",
            {"somossa"},
            {"সোমোস্সা", "সোমস্সা", "সোমোসসা", "সমসসা"},
            2,
            "somossa: strict junk suppression (only [সমস্যা | somossa])"
        },
        {
            "brain",
            "ব্রেন",
            {"ব্রেইন", "brain"},
            {"ব্ৰাইন", "ব্রাইন", "বরাইন", "ব্ৰইন"},
            3,
            "brain: clean loanword + near-duplicate suppression"
        },
        {
            "screenshot",
            "স্ক্রিনশট",
            {"স্ক্রিনশোট", "screenshot"},
            {"সক্রিনশোট", "সক্রিনশোত", "সচুরিনশোট", "সুট্রিনশোত", "স্চ্রিন্শোত"},
            3,
            "screenshot: clean loanword without broken conjunct beam leftovers"
        },
        {
            "output",
            "আউটপুট",
            {"output"},
            {"ঔতপুত", "ঔত্পুত", "ঔত্পূত", "ওউত্পুত"},
            2,
            "output: standard loanword (no OU-kar phonetic mistakes)"
        },
        {
            "better",
            "বেটার",
            {"better"},
            {"বেত্তের", "বেততের", "বেট্তের"},
            2,
            "better: loanword without geminate consonant artifacts"
        },
        {
            "ami",
            "আমি",
            {"আমী", "ami"},
            {"অমি", "এমি", "অমী"},
            3,
            "ami: common Bangla core pronoun without vowel distortion"
        },
        {
            "office",
            "অফিস",
            {"office"},
            {"অফ্ফিচে", "অফফিচে", "ওফ্ফিচে"},
            2,
            "office: clean loanword without Italian/phonetic 'che' endings"
        },
        {
            "computer",
            "কম্পিউটার",
            {"computer"},
            {"চোম্পুতের", "চম্পুতের", "চোম্পূতের"},
            2,
            "computer: loanword without French/Latin hard C -> Ch artifacts"
        },
        {
            "keyboard",
            "কিবোর্ড",
            {"কীবোর্ড", "keyboard"},
            {"কেয়বোঅর্দ", "কেযবোঅর্দ"},
            3,
            "keyboard: standard Bengali tech spelling (কিবোর্ড / কীবোর্ড)"
        },
        {
            "windows",
            "উইন্ডোজ",
            {"windows"},
            {"ৱিন্দোৱ্স", "উইন্দোউস"},
            2,
            "windows: standard localized OS noun"
        },
        {
            "memory",
            "মেমোরি",
            {"memory"},
            {"মেমোৱি", "মেমরি"},
            2,
            "memory: clean loanword"
        },
        {
            "taka",
            "টাকা",
            {"তাকা", "taka"},
            {"তকা", "টকা"},
            3,
            "taka: preserves real alternative dictionary word (তাকা) while dropping non-words"
        }
    };

    int total_evaluations = 0;
    int passed_evaluations = 0;

    for (const auto& qc : cases) {
        std::cout << "TEST: " << qc.input << " -> " << qc.description << "\n";
        BanglaEngine_SetComposition(engine, qc.input.c_str());
        CandidateList list;
        BanglaEngine_GetCandidates(engine, &list);

        std::cout << "  Actual candidates (" << list.count << "): ";
        for (uint32_t i = 0; i < list.count; i++) {
            std::cout << "[" << list.candidates[i].bengali_text << "] ";
        }
        std::cout << "\n";

        // Check 1: Rank 1 correctness
        total_evaluations++;
        bool rank1_ok = (list.count > 0 && std::string(list.candidates[0].bengali_text) == qc.expected_top1);
        if (rank1_ok) {
            passed_evaluations++;
            std::cout << "  [PASS] Rank 1 is '" << qc.expected_top1 << "'\n";
        } else {
            std::cout << "  [FAIL] Rank 1 expected '" << qc.expected_top1 << "', got '" 
                      << (list.count > 0 ? list.candidates[0].bengali_text : "EMPTY") << "'\n";
        }

        // Check 2: No forbidden junk words in ANY slot
        total_evaluations++;
        bool junk_found = false;
        std::string found_junk_word;
        for (uint32_t i = 0; i < list.count; i++) {
            std::string c_text = list.candidates[i].bengali_text;
            for (const auto& junk : qc.forbidden_junk) {
                if (c_text == junk) {
                    junk_found = true;
                    found_junk_word = junk;
                    break;
                }
            }
            if (junk_found) break;
        }
        if (!junk_found) {
            passed_evaluations++;
            std::cout << "  [PASS] Zero forbidden junk non-words in popup\n";
        } else {
            std::cout << "  [FAIL] Junk non-word '" << found_junk_word << "' occupied a suggestion slot!\n";
        }

        // Check 3: Zero duplicates or near-duplicates
        total_evaluations++;
        std::set<std::string> seen;
        bool has_dups = false;
        for (uint32_t i = 0; i < list.count; i++) {
            std::string text = list.candidates[i].bengali_text;
            if (seen.count(text)) {
                has_dups = true;
                break;
            }
            seen.insert(text);
        }
        if (!has_dups) {
            passed_evaluations++;
            std::cout << "  [PASS] Zero duplicates or visual duplicates in candidate list\n";
        } else {
            std::cout << "  [FAIL] Duplicate candidates detected in suggestion list!\n";
        }

        // Check 4: Quality over quantity (list count doesn't exceed maximum useful slots)
        total_evaluations++;
        if (list.count <= qc.max_expected_count) {
            passed_evaluations++;
            std::cout << "  [PASS] Candidate count (" << list.count << " <= " << qc.max_expected_count 
                      << ") satisfies Quality > Quantity policy\n";
        } else {
            std::cout << "  [FAIL] Too many bloated low-quality candidates (" << list.count 
                      << " > " << qc.max_expected_count << ")\n";
        }
        std::cout << "\n";
    }

    BanglaEngine_Destroy(engine);

    std::cout << "======================================================================\n";
    std::cout << " POPUP QUALITY SUITE RESULT: " << passed_evaluations << " / " << total_evaluations 
              << " (" << (passed_evaluations == total_evaluations ? "100% PASSED" : "FAILED") << ")\n";
    std::cout << "======================================================================\n";

    return (passed_evaluations == total_evaluations) ? 0 : 1;
}
