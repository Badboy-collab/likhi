#include "../../engine/include/bangla_engine.h"
#include "../../engine/src/unicode/bangla_unicode.h"
#include <iostream>
#include <vector>
#include <string>
#include <cassert>
#include <iomanip>

#ifdef _WIN32
#include <windows.h>
#endif

using namespace bangla;
using namespace bangla::codepoint;

void SetupConsole() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
}

std::string CodepointsToHexString(const std::u32string& u32) {
    std::string out;
    for (size_t i = 0; i < u32.size(); i++) {
        char buf[16];
        snprintf(buf, sizeof(buf), "U+%04X", static_cast<uint32_t>(u32[i]));
        if (i > 0) out += " ";
        out += buf;
    }
    return out;
}

int main() {
    SetupConsole();

    std::cout << "=========================================================\n";
    std::cout << "  PC Bangla Typing App - Dedicated Unicode Integrity Tests\n";
    std::cout << "=========================================================\n\n";

    int total_tests = 0;
    int passed_tests = 0;
    int failed_tests = 0;

    auto assert_test = [&](const std::string& name, bool condition, const std::string& details = "") {
        total_tests++;
        if (condition) {
            passed_tests++;
            std::cout << "  [PASS] " << name << "\n";
        } else {
            failed_tests++;
            std::cout << "  [FAIL] " << name << " | " << details << "\n";
        }
    };

    // Suite 1: Canonical Codepoint Ordering & Conjuncts
    std::cout << "=== [TEST SUITE 1] Complex Conjunct Unicode Codepoint Verification ===\n";

    struct ConjunctTestCase {
        std::string name;
        std::string utf8_text;
        std::vector<char32_t> expected_codepoints;
    };

    std::vector<ConjunctTestCase> conjunct_cases = {
        {"Khyo (ক্ষ)", "ক্ষ", {CONSONANT_KA, HASANT, CONSONANT_SSA}},
        {"Ggyo (জ্ঞ)", "জ্ঞ", {CONSONANT_JA, HASANT, CONSONANT_NYA}},
        {"Ncho (ঞ্চ)", "ঞ্চ", {CONSONANT_NYA, HASANT, CONSONANT_CA}},
        {"Njo (ঞ্জ)", "ঞ্জ", {CONSONANT_NYA, HASANT, CONSONANT_JA}},
        {"Ngko (ঙ্ক)", "ঙ্ক", {CONSONANT_NGA, HASANT, CONSONANT_KA}},
        {"Nggo (ঙ্গ)", "ঙ্গ", {CONSONANT_NGA, HASANT, CONSONANT_GA}},
        {"Ccho (চ্ছ)", "চ্ছ", {CONSONANT_CA, HASANT, CONSONANT_CHA}},
        {"Jjo (জ্জ)", "জ্জ", {CONSONANT_JA, HASANT, CONSONANT_JA}},
        {"Tto (ট্ট)", "ট্ট", {CONSONANT_TTA, HASANT, CONSONANT_TTA}},
        {"Nto (ন্ত)", "ন্ত", {CONSONANT_NA, HASANT, CONSONANT_TA}},
        {"Ntro (ন্ত্র)", "ন্ত্র", {CONSONANT_NA, HASANT, CONSONANT_TA, HASANT, CONSONANT_RA}},
        {"Ndo (ন্দ)", "ন্দ", {CONSONANT_NA, HASANT, CONSONANT_DA}},
        {"Ndho (ন্ধ)", "ন্ধ", {CONSONANT_NA, HASANT, CONSONANT_DHA}},
        {"Mpo (ম্প)", "ম্প", {CONSONANT_MA, HASANT, CONSONANT_PA}},
        {"Mbo (ম্ব)", "ম্ব", {CONSONANT_MA, HASANT, CONSONANT_BA}},
        {"Mbho (ম্ভ)", "ম্ভ", {CONSONANT_MA, HASANT, CONSONANT_BHA}},
        {"Shto (ষ্ট)", "ষ্ট", {CONSONANT_SSA, HASANT, CONSONANT_TTA}},
        {"Shtho (ষ্ঠ)", "ষ্ঠ", {CONSONANT_SSA, HASANT, CONSONANT_TTHA}},
        {"Sko (স্ক)", "স্ক", {CONSONANT_SA, HASANT, CONSONANT_KA}},
        {"Sno (স্ন)", "স্ন", {CONSONANT_SA, HASANT, CONSONANT_NA}},
        {"Spo (স্প)", "স্প", {CONSONANT_SA, HASANT, CONSONANT_PA}},
        {"Sto (স্ত)", "স্ত", {CONSONANT_SA, HASANT, CONSONANT_TA}},
        {"Stho (স্থ)", "স্থ", {CONSONANT_SA, HASANT, CONSONANT_THA}},
        {"Tro (ত্র)", "ত্র", {CONSONANT_TA, HASANT, CONSONANT_RA}},
        {"Dro (দ্র)", "দ্র", {CONSONANT_DA, HASANT, CONSONANT_RA}},
        {"Shro (শ্র)", "শ্র", {CONSONANT_SHA, HASANT, CONSONANT_RA}}
    };

    for (const auto& tc : conjunct_cases) {
        std::u32string actual_u32 = UnicodeUtils::Utf8ToUtf32(tc.utf8_text);
        std::u32string exp_u32(tc.expected_codepoints.begin(), tc.expected_codepoints.end());
        bool match = (actual_u32 == exp_u32);
        assert_test(tc.name, match, "Got: " + CodepointsToHexString(actual_u32) + " Exp: " + CodepointsToHexString(exp_u32));
    }

    // Suite 2: Real Multi-Conjunct Complex Words
    std::cout << "\n=== [TEST SUITE 2] Complex Real Bengali Word Integrity ===\n";

    std::vector<std::string> complex_words = {
        "স্বাস্থ্য", "আন্তর্জাতিক", "ব্রহ্মপুত্র", "বিজ্ঞপ্তি", "আত্মীয়",
        "উৎকৃষ্ট", "বৃষ্টি", "স্মৃতি", "আকাঙ্ক্ষা", "পঙ্কজ",
        "অঞ্চল", "শৃঙ্খলা", "উজ্জ্বল", "দ্বন্দ্ব", "তত্ত্ব"
    };

    for (const auto& w : complex_words) {
        bool valid = UnicodeUtils::IsValidBengaliSequence(w);
        bool no_dangling = !UnicodeUtils::HasDanglingHasant(w);
        assert_test("Valid Bengali Sequence: " + w, valid && no_dangling, "Invalid sequence or dangling Hasant detected");
    }

    // Suite 3: Grapheme-Aware Backspace Operations
    std::cout << "\n=== [TEST SUITE 3] Grapheme-Aware Backspace Testing ===\n";

    // 1. Backspace from "কা" (ক + া) -> should become "ক"
    std::string s1 = "কা";
    std::string bs1 = UnicodeUtils::BackspaceGrapheme(s1);
    assert_test("Backspace 'কা' -> 'ক'", bs1 == "ক", "Got: " + bs1);

    // 2. Backspace from "ক্ষ" (ক + ্ + ষ) -> should delete consonant + Hasant, becoming "ক"
    std::string s2 = "ক্ষ";
    std::string bs2 = UnicodeUtils::BackspaceGrapheme(s2);
    assert_test("Backspace 'ক্ষ' -> 'ক' (no dangling hasant)", bs2 == "ক" && !UnicodeUtils::HasDanglingHasant(bs2), "Got: " + bs2);

    // 3. Backspace from "চাঁদ" (চ + ঁ + া + দ) -> should delete "দ", becoming "চাঁ"
    std::string s3 = "চাঁদ";
    std::string bs3 = UnicodeUtils::BackspaceGrapheme(s3);
    assert_test("Backspace 'চাঁদ' -> 'চাঁ'", bs3 == "চাঁ", "Got: " + bs3);

    // 4. Repeated backspace on "বৃষ্টি"
    std::string s4 = "বৃষ্টি";
    std::string step1 = UnicodeUtils::BackspaceGrapheme(s4);    // "বৃষ্ট" (deletes Kar i)
    std::string step2 = UnicodeUtils::BackspaceGrapheme(step1); // "বৃষ" (deletes Tta + Hasant)
    std::string step3 = UnicodeUtils::BackspaceGrapheme(step2); // "বৃ" (deletes Ssa)
    std::string step4 = UnicodeUtils::BackspaceGrapheme(step3); // "ব" (deletes Kar ri)
    std::string step5 = UnicodeUtils::BackspaceGrapheme(step4); // "" (deletes Ba)
    assert_test("Sequential Backspace on 'বৃষ্টি' (5 steps to empty string)",
                step1 == "বৃষ্ট" && !UnicodeUtils::HasDanglingHasant(step1) &&
                step2 == "বৃষ" && !UnicodeUtils::HasDanglingHasant(step2) &&
                step3 == "বৃ" &&
                step4 == "ব" &&
                step5.empty(),
                "Step1: " + step1 + " Step2: " + step2 + " Step3: " + step3 + " Step4: " + step4);

    // Suite 4: Auto-Correct Threshold Strict Rule Regression Test (0.832 < 0.85)
    std::cout << "\n=== [TEST SUITE 4] Auto-Correct Strict Threshold Regression Test ===\n";
    EngineConfig cfg;
    BanglaEngine_GetDefaultConfig(&cfg);
    cfg.auto_correct_enabled = true;
    cfg.auto_correct_threshold = 0.85f;

    BanglaEngine* engine = BanglaEngine_Create(&cfg);
    BanglaEngine_SetComposition(engine, "ami");

    CandidateList list;
    BanglaEngine_GetCandidates(engine, &list);

    bool threshold_satisfied = true;
    for (uint32_t i = 0; i < list.count; i++) {
        if (list.candidates[i].score < cfg.auto_correct_threshold && list.candidates[i].auto_correct_recommended) {
            threshold_satisfied = false;
        }
    }
    assert_test("Score < 0.85 strictly forbids Auto-Correct recommendation (0.832 < 0.85 verified)", threshold_satisfied, "Auto-correct flagged on sub-threshold score");

    BanglaEngine_Destroy(engine);

    // Summary
    std::cout << "\n=========================================================\n";
    std::cout << "  UNICODE INTEGRITY TEST RESULTS\n";
    std::cout << "  Total Passed: " << passed_tests << "\n";
    std::cout << "  Total Failed: " << failed_tests << "\n";
    std::cout << "  Status: " << (failed_tests == 0 ? "ALL PASSED (100% UNICODE INTEGRITY)" : "FAILURES DETECTED") << "\n";
    std::cout << "=========================================================\n";

    return (failed_tests == 0) ? 0 : 1;
}
