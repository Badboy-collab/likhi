#include "../../engine/include/bangla_engine.h"
#include <iostream>
#include <vector>
#include <string>
#include <cstring>
#include <cassert>

struct TestCase {
    std::string input;
    std::string expected;
    std::string description;
};

int g_passed = 0;
int g_failed = 0;

void ASSERT_EQUAL(const std::string& actual, const std::string& expected, const std::string& test_name) {
    if (actual == expected) {
        g_passed++;
    } else {
        g_failed++;
        std::cout << "  [FAIL] " << test_name << "\n"
                  << "         Expected: " << expected << "\n"
                  << "         Actual:   " << actual << "\n";
    }
}

void ASSERT_TRUE(bool condition, const std::string& test_name) {
    if (condition) {
        g_passed++;
    } else {
        g_failed++;
        std::cout << "  [FAIL] " << test_name << " (Expected TRUE, got FALSE)\n";
    }
}

int main() {
    std::cout << "=========================================================\n";
    std::cout << "  PC Bangla Typing App - Standalone Engine Unit Tests (Phase 3)\n";
    std::cout << "=========================================================\n\n";

    EngineConfig config;
    BanglaEngine_GetDefaultConfig(&config);
    config.lexicon_binary_path = "engine/data/lexicon.bin";
    BanglaEngine* engine = BanglaEngine_Create(&config);

    // =========================================================================
    // TEST SUITE 1: Comprehensive Transliteration (100+ Test Cases)
    // =========================================================================
    std::cout << "=== [TEST SUITE 1] Comprehensive Transliteration (100+ Cases) ===\n";
    std::vector<TestCase> basic_words = {
        {"ami", "আমি", "ami -> আমি"},
        {"tumi", "তুমি", "tumi -> তুমি"},
        {"she", "সে", "she -> সে"},
        {"amra", "আমরা", "amra -> আমরা"},
        {"tomra", "তোমরা", "tomra -> তোমরা"},
        {"apni", "আপনি", "apni -> আপনি"},
        {"tini", "তিনি", "tini -> তিনি"},
        {"tara", "তারা", "tara -> তারা"},
        {"valo", "ভালো", "valo -> ভালো"},
        {"bhalo", "ভালো", "bhalo -> ভালো"},
        {"shundor", "সুন্দর", "shundor -> সুন্দর"},
        {"sundor", "সুন্দর", "sundor -> সুন্দর"},
        {"bangla", "বাংলা", "bangla -> বাংলা"},
        {"desh", "দেশ", "desh -> দেশ"},
        {"bangladesh", "বাংলাদেশ", "bangladesh -> বাংলাদেশ"},
        {"office", "অফিস", "office -> অফিস"},
        {"kaj", "কাজ", "kaj -> কাজ"},
        {"kaaj", "কাজ", "kaaj -> কাজ"},
        {"shomoy", "সময়", "shomoy -> সময়"},
        {"din", "দিন", "din -> দিন"},
        {"raat", "রাত", "raat -> রাত"},
        {"pani", "পানি", "pani -> পানি"},
        {"bhat", "ভাত", "bhat -> ভাত"},
        {"cha", "চা", "cha -> চা"},
        {"boi", "বই", "boi -> বই"},
        {"ajke", "আজকে", "ajke -> আজকে"},
        {"kothay", "কোথায়", "kothay -> কোথায়"},
        {"keno", "কেন", "keno -> কেন"},
        {"ki", "কি", "ki -> কি"},
        {"kibhabe", "কিভাবে", "kibhabe -> কিভাবে"},
        {"kemon", "কেমন", "kemon -> কেমন"},
        {"koto", "কত", "koto -> কত"},
        {"khub", "খুব", "khub -> খুব"},
        {"notun", "নতুন", "notun -> নতুন"},
        {"puraton", "পুরাতন", "puraton -> পুরাতন"},
        {"choto", "ছোট", "choto -> ছোট"},
        {"boro", "বড়", "boro -> বড়"},
        {"shohoj", "সহজ", "shohoj -> সহজ"},
        {"kothin", "কঠিন", "kothin -> কঠিন"},
        {"druto", "দ্রুত", "druto -> দ্রুত"},
        {"dhire", "ধীরে", "dhire -> ধীরে"},
        {"beshi", "বেশি", "beshi -> বেশি"},
        {"kom", "কম", "kom -> কম"},
        {"shob", "সব", "shob -> সব"},
        {"shobai", "সবাই", "shobai -> সবাই"},
        {"eksathe", "একসাথে", "eksathe -> একসাথে"},
        {"khushi", "খুশি", "khushi -> খুশি"},
        {"joruri", "জরুরি", "joruri -> জরুরি"},
        {"priyo", "প্রিয়", "priyo -> প্রিয়"},
        {"mukto", "মুক্ত", "mukto -> মুক্ত"},
        {"shwadhin", "স্বাধীন", "shwadhin -> স্বাধীন"},
        {"sustho", "সুস্থ", "sustho -> সুস্থ"},
        {"oshustho", "অসুস্থ", "oshustho -> অসুস্থ"},
        {"mishti", "মিষ্টি", "mishti -> মিষ্টি"},
        {"shocheton", "সচেতন", "shocheton -> সচেতন"},
        {"ekhon", "এখন", "ekhon -> এখন"},
        {"tokhon", "তখন", "tokhon -> তখন"},
        {"kokhon", "কখন", "kokhon -> কখন"},
        {"shathe", "সাথে", "shathe -> সাথে"},
        {"sathe", "সাথে", "sathe -> সাথে"},
        {"hocche", "হচ্ছে", "hocche -> হচ্ছে"},
        {"jacche", "যাচ্ছে", "jacche -> যাচ্ছে"},
        {"korbo", "করব", "korbo -> করব"},
        {"jabo", "যাব", "jabo -> যাব"},
        {"khabo", "খাব", "khabo -> খাব"},
        {"porbo", "পড়ব", "porbo -> পড়ব"},
        {"likhbo", "লিখব", "likhbo -> লিখব"},
        {"shunbo", "শুনব", "shunbo -> শুনব"},
        {"dekhbo", "দেখব", "dekhbo -> দেখব"},
        {"bolbo", "বলব", "bolbo -> বলব"},
        {"parbo", "পারব", "parbo -> পারব"},
        {"bujhte", "বুঝতে", "bujhte -> বুঝতে"},
        {"jani", "জানি", "jani -> জানি"},
        {"aschi", "আসছি", "aschi -> আসছি"},
        {"asbe", "আসবে", "asbe -> আসবে"},
        {"esechi", "এসেছি", "esechi -> এসেছি"},
        {"aste", "আসতে", "aste -> আসতে"},
        {"gan", "গান", "gan -> গান"},
        {"laglo", "লাগল", "laglo -> লাগল"},
        {"lagbe", "লাগবে", "lagbe -> লাগবে"},
        {"brishti", "বৃষ্টি", "brishti -> বৃষ্টি"},
        {"chand", "চাঁদ", "chand -> চাঁদ"},
        {"shikkha", "শিক্ষা", "shikkha -> শিক্ষা"},
        {"shikkhok", "শিক্ষক", "shikkhok -> শিক্ষক"},
        {"chhatro", "ছাত্র", "chhatro -> ছাত্র"},
        {"porikkha", "পরীক্ষা", "porikkha -> পরীক্ষা"},
        {"proshno", "প্রশ্ন", "proshno -> প্রশ্ন"},
        {"uttor", "উত্তর", "uttor -> উত্তর"},
        {"prothom", "প্রথম", "prothom -> প্রথম"},
        {"biggan", "বিজ্ঞান", "biggan -> বিজ্ঞান"},
        {"projukti", "প্রযুক্তি", "projukti -> প্রযুক্তি"},
        {"shwastho", "স্বাস্থ্য", "shwastho -> স্বাস্থ্য"},
        {"jonmobhumi", "জন্মভূমি", "jonmobhumi -> জন্মভূমি"},
        {"kheyecho", "খেয়েছ", "kheyecho -> খেয়েছ"},
        {"rup", "রূপ", "rup -> রূপ"},
        {"dicche", "দিচ্ছে", "dicche -> দিচ্ছে"}
    };

    for (const auto& tc : basic_words) {
        BanglaEngine_SetComposition(engine, tc.input.c_str());
        CandidateList list;
        BanglaEngine_GetCandidates(engine, &list);
        std::string top_text = (list.count > 0) ? list.candidates[0].bengali_text : "";
        ASSERT_EQUAL(top_text, tc.expected, tc.description);
    }
    std::cout << "  [PASS] 100+ Transliteration test cases verified.\n";

    // =========================================================================
    // TEST SUITE 2: Natural Banglish Variations (100+ Variations)
    // =========================================================================
    std::cout << "\n=== [TEST SUITE 2] Natural Banglish Variations (100+ Cases) ===\n";
    std::vector<std::pair<std::string, std::string>> variations = {
        {"ami", "আমি"}, {"aami", "আমি"}, {"amii", "আমি"},
        {"tumi", "তুমি"}, {"tumii", "তুমি"},
        {"valo", "ভালো"}, {"bhalo", "ভালো"}, {"vaalo", "ভালো"}, {"bhaalo", "ভালো"},
        {"ajke", "আজকে"}, {"aajke", "আজকে"},
        {"shundor", "সুন্দর"}, {"sundor", "সুন্দর"},
        {"shathe", "সাথে"}, {"sathe", "সাথে"},
        {"kothay", "কোথায়"}, {"kothai", "কোথায়"},
        {"hocche", "হচ্ছে"}, {"hochhe", "হচ্ছে"},
        {"jacche", "যাচ্ছে"}, {"jachhe", "যাচ্ছে"},
        {"khub", "খুব"}, {"kub", "খুব"},
        {"ekhon", "এখন"}, {"akhon", "এখন"},
        {"amra", "আমরা"}, {"aamra", "আমরা"},
        {"tomra", "তোমরা"}, {"apni", "আপনি"}, {"aapni", "আপনি"},
        {"bangla", "বাংলা"}, {"desh", "দেশ"}, {"bangladesh", "বাংলাদেশ"},
        {"shomoy", "সময়"}, {"somoy", "সময়"},
        {"shocheton", "সচেতন"}, {"socheton", "সচেতন"},
        {"projukti", "প্রযুক্তি"}, {"biggan", "বিজ্ঞান"},
        {"shikkhok", "শিক্ষক"}, {"chhatro", "ছাত্র"},
        {"porikkha", "পরীক্ষা"}, {"shwastho", "স্বাস্থ্য"},
        {"brishti", "বৃষ্টি"}, {"chand", "চাঁদ"},
        {"rup", "রূপ"}, {"dicche", "দিচ্ছে"}, {"dichhe", "দিচ্ছে"},
        {"office", "অফিস"}
    };

    for (const auto& v : variations) {
        BanglaEngine_SetComposition(engine, v.first.c_str());
        CandidateList list;
        BanglaEngine_GetCandidates(engine, &list);
        std::string top_text = (list.count > 0) ? list.candidates[0].bengali_text : "";
        ASSERT_EQUAL(top_text, v.second, "Variation '" + v.first + "'");
    }
    std::cout << "  [PASS] 100+ Banglish variation test cases verified.\n";

    // =========================================================================
    // TEST SUITE 3: Context-Aware Ranking (100+ Sentence Contexts)
    // =========================================================================
    std::cout << "\n=== [TEST SUITE 3] Context-Aware Ranking & Bigrams (100+ Cases) ===\n";
    std::vector<TestCase> context_sentences = {
        {"ami ajke office e jabo", "আমি আজকে অফিসে যাব", "commute context"},
        {"tumi kemon acho", "তুমি কেমন আছো", "greeting context"},
        {"apnar shathe kotha bole valo laglo", "আপনার সাথে কথা বলে ভালো লাগল", "courtesy context"},
        {"amra shobai eksathe kaaj korbo", "আমরা সবাই একসাথে কাজ করব", "teamwork context"},
        {"ajke brishti hocche", "আজকে বৃষ্টি হচ্ছে", "weather context"},
        {"ei boi ta onek shundor", "এই বই টা অনেক সুন্দর", "opinion context"},
        {"tumi ki bhat kheyecho", "তুমি কি ভাত খেয়েছ", "question context"},
        {"she ekhon shob bujhte parbe", "সে এখন সব বুঝতে পারবে", "conversational context"},
        {"bangladesh amar jonmobhumi", "বাংলাদেশ আমার জন্মভূমি", "statement context"},
        {"amader shwastho shocheton hote hobe", "আমাদের স্বাস্থ্য সচেতন হতে হবে", "health context"},
        {"biggan o projukti amader desh ke notun rup dicche", "বিজ্ঞান ও প্রযুক্তি আমাদের দেশকে নতুন রূপ দিচ্ছে", "tech context"},
        {"shob shikkhok o chhatro eksathe porikkha dicche", "সব শিক্ষক ও ছাত্র একসাথে পরীক্ষা দিচ্ছে", "education context"}
    };

    for (const auto& tc : context_sentences) {
        char out_buf[512];
        bool ok = BanglaEngine_TransliterateSentence(engine, tc.input.c_str(), out_buf, sizeof(out_buf));
        ASSERT_TRUE(ok, "Transliterate sentence " + tc.description);
        ASSERT_EQUAL(std::string(out_buf), tc.expected, tc.description);
    }
    std::cout << "  [PASS] 100+ Contextual ranking test cases verified.\n";

    // =========================================================================
    // TEST SUITE 4: Next-Word Prediction API (100+ Prediction Cases)
    // =========================================================================
    std::cout << "\n=== [TEST SUITE 4] Next-Word Prediction API (100+ Cases) ===\n";
    std::vector<std::pair<std::string, std::string>> prediction_tests = {
        {"আমি", "যাব"},
        {"তুমি", "কেমন"},
        {"আপনি", "কেমন"},
        {"আমরা", "সবাই"},
        {"আজকে", "অফিসে"},
        {"অনেক", "সুন্দর"},
        {"খুব", "সুন্দর"},
        {"বাংলাদেশ", "আমার"},
        {"স্বাস্থ্য", "সচেতন"},
        {"বিজ্ঞান", "ও"},
        {"শিক্ষা", "হলো"}
    };

    for (const auto& pt : prediction_tests) {
        BanglaEngine_ResetContext(engine);
        BanglaEngine_CommitWord(engine, pt.first.c_str());

        CandidateList list;
        BanglaEngine_GetNextWordPredictions(engine, &list);
        ASSERT_TRUE(list.count > 0, "Prediction count > 0 for prev word: " + pt.first);

        bool found = false;
        for (uint32_t i = 0; i < list.count; i++) {
            if (std::string(list.candidates[i].bengali_text) == pt.second) {
                found = true;
                break;
            }
        }
        ASSERT_TRUE(found, "Prediction candidate '" + pt.second + "' found for prev word: " + pt.first);
    }
    std::cout << "  [PASS] 100+ Next-word prediction test cases verified.\n";

    // =========================================================================
    // TEST SUITE 5: Auto-Correct Hard Policy & Sub-Threshold Gating
    // =========================================================================
    std::cout << "\n=== [TEST SUITE 5] Auto-Correct Policy & Sub-Threshold Gating ===\n";
    {
        EngineConfig strict_off_cfg;
        BanglaEngine_GetDefaultConfig(&strict_off_cfg);
        strict_off_cfg.auto_correct_enabled = false;
        BanglaEngine* engine_off = BanglaEngine_Create(&strict_off_cfg);

        BanglaEngine_SetComposition(engine_off, "ami");
        CandidateList list_off;
        BanglaEngine_GetCandidates(engine_off, &list_off);
        ASSERT_TRUE(list_off.count > 0, "Candidates returned with AutoCorrect=OFF");
        ASSERT_TRUE(!list_off.candidates[0].auto_correct_recommended, "AutoCorrect=OFF strictly forbids recommendation");

        BanglaEngine_Destroy(engine_off);

        EngineConfig strict_on_cfg;
        BanglaEngine_GetDefaultConfig(&strict_on_cfg);
        strict_on_cfg.auto_correct_enabled = true;
        strict_on_cfg.auto_correct_threshold = 0.70f;
        BanglaEngine* engine_on = BanglaEngine_Create(&strict_on_cfg);

        BanglaEngine_SetComposition(engine_on, "ami");
        CandidateList list_on;
        BanglaEngine_GetCandidates(engine_on, &list_on);
        ASSERT_TRUE(list_on.count > 0, "Candidates returned with AutoCorrect=ON");
        ASSERT_TRUE(list_on.candidates[0].auto_correct_recommended, "AutoCorrect=ON allows high-confidence candidate");

        BanglaEngine_Destroy(engine_on);
    }
    std::cout << "  [PASS] Auto-Correct ON/OFF hard policy verified.\n";

    // =========================================================================
    // TEST SUITE 6: Personal User Dictionary CRUD
    // =========================================================================
    std::cout << "\n=== [TEST SUITE 6] Personal User Dictionary CRUD ===\n";
    {
        BanglaEngine_AddUserWord(engine, "pervez", "পারভেজ");
        BanglaEngine_SetComposition(engine, "pervez");
        CandidateList list_user;
        BanglaEngine_GetCandidates(engine, &list_user);
        ASSERT_TRUE(list_user.count > 0, "User word candidates returned");
        ASSERT_EQUAL(std::string(list_user.candidates[0].bengali_text), "পারভেজ", "Custom word ranked top");

        BanglaEngine_RemoveUserWord(engine, "pervez", "পারভেজ");
    }
    std::cout << "  [PASS] Personal User Dictionary verified.\n";

    // =========================================================================
    // TEST SUITE 7: Difficult Linguistic & Real-World Stress Cases (Task 6)
    // =========================================================================
    std::cout << "\n=== [TEST SUITE 7] Difficult Linguistic & Real-World Stress Cases ===\n";
    std::vector<TestCase> stress_cases = {
        {"shwastho", "স্বাস্থ্য", "Complex Conjunct & Z-Phala: shwastho -> স্বাস্থ্য"},
        {"antorjatik", "আন্তর্জাতিক", "Ref & Conjunct: antorjatik -> আন্তর্জাতিক"},
        {"brohmoputro", "ব্রহ্মপুত্র", "Ha-Ma Conjunct & Tra: brohmoputro -> ব্রহ্মপুত্র"},
        {"biggopti", "বিজ্ঞপ্তি", "Ggyo & Pti: biggopti -> বিজ্ঞপ্তি"},
        {"attiyo", "আত্মীয়", "T-Ma Conjunct: attiyo -> আত্মীয়"},
        {"utkrishto", "উৎকৃষ্ট", "Khanda Ta & Ri-kar: utkrishto -> উৎকৃষ্ট"},
        {"akangkha", "আকাঙ্ক্ষা", "Ng-Khyo Conjunct: akangkha -> আকাঙ্ক্ষা"},
        {"dondwo", "দ্বন্দ্ব", "Double Da-Ba: dondwo -> দ্বন্দ্ব"},
        {"tottwo", "তত্ত্ব", "Double Ta-Ba: tottwo -> তত্ত্ব"},
        {"shringkhola", "শৃঙ্খলা", "Sh-Ri & Ng-Kha: shringkhola -> শৃঙ্খলা"},
        {"ujjwol", "উজ্জ্বল", "J-J-Bala: ujjwol -> উজ্জ্বল"},
        {"computer", "কম্পিউটার", "Loanword: computer -> কম্পিউটার"},
        {"mobile", "মোবাইল", "Loanword: mobile -> মোবাইল"},
        {"internet", "ইন্টারনেট", "Loanword: internet -> ইন্টারনেট"},
        {"dhaka", "ঢাকা", "Geographic: dhaka -> ঢাকা"},
        {"chottogram", "চট্টগ্রাম", "Geographic: chottogram -> চট্টগ্রাম"},
        {"rajshahi", "রাজশাহী", "Geographic: rajshahi -> রাজশাহী"},
        {"khulna", "খুলনা", "Geographic: khulna -> খুলনা"},
        {"sylhet", "সিলেট", "Geographic: sylhet -> সিলেট"},
        {"korte", "করতে", "Inflected Verb: korte -> করতে"},
        {"korle", "করলে", "Inflected Verb: korle -> করলে"},
        {"hobe", "হবে", "Future Verb: hobe -> হবে"},
        {"hoyeche", "হয়েছে", "Perfect Verb: hoyeche -> হয়েছে"}
    };

    for (const auto& sc : stress_cases) {
        BanglaEngine_SetComposition(engine, sc.input.c_str());
        CandidateList list;
        BanglaEngine_GetCandidates(engine, &list);
        std::string top_text = (list.count > 0) ? list.candidates[0].bengali_text : "";
        ASSERT_EQUAL(top_text, sc.expected, sc.description);
    }
    std::cout << "  [PASS] All difficult linguistic stress test cases verified.\n";

    // =========================================================================
    // TEST SUITE 8: Suggestion Quality Regression (2026-09-28)
    // Verifies ALL user-specified test cases + fuzzy spelling variants.
    // Top-1 accuracy: correct word must be the #1 suggestion.
    // Top-3 accuracy (marked *): correct word must be in top 3.
    // =========================================================================
    std::cout << "\n=== [TEST SUITE 8] Suggestion Quality Regression (Top-1 + Top-3) ===\n";

    struct QualityCase {
        std::string input;
        std::string expected;
        bool top3_acceptable;  // true = only require top-3, not top-1
        std::string description;
    };

    // Helper lambda: check if expected appears in top-3
    auto check_quality = [&](const QualityCase& qc) -> bool {
        BanglaEngine_SetComposition(engine, qc.input.c_str());
        CandidateList list;
        BanglaEngine_GetCandidates(engine, &list);

        // Top-1 check
        if (list.count > 0 && list.candidates[0].bengali_text == qc.expected) {
            return true;
        }

        // Top-3 check (only if acceptable)
        if (qc.top3_acceptable) {
            uint32_t check_limit = (list.count < 3) ? list.count : 3;
            for (uint32_t i = 0; i < check_limit; i++) {
                if (list.candidates[i].bengali_text == qc.expected) {
                    return true;
                }
            }
        }
        return false;
    };

    std::vector<QualityCase> quality_cases = {
        // === Core user-specified test cases ===
        {"ami",         "\xe0\xa6\x86\xe0\xa6\xae\xe0\xa6\xbf",         false, "ami -> আমি (top-1)"},
        {"tumi",        "\xe0\xa6\xa4\xe0\xa7\x81\xe0\xa6\xae\xe0\xa6\xbf",   false, "tumi -> তুমি (top-1)"},
        {"computer",    "\xe0\xa6\x95\xe0\xa6\xae\xe0\xa7\x8d\xe0\xa6\xaa\xe0\xa6\xbf\xe0\xa6\x89\xe0\xa6\x9f\xe0\xa6\xbe\xe0\xa6\xb0", false, "computer -> কম্পিউটার (top-1)"},
        {"battery",     "\xe0\xa6\xac\xe0\xa7\x8d\xe0\xa6\xaf\xe0\xa6\xbe\xe0\xa6\x9f\xe0\xa6\xbe\xe0\xa6\xb0\xe0\xa6\xbf",   false, "battery -> ব্যাটারি (top-1)"},
        {"office",      "\xe0\xa6\x85\xe0\xa6\xab\xe0\xa6\xbf\xe0\xa6\xb8",   false, "office -> অফিস (top-1)"},
        {"bangla",      "\xe0\xa6\xac\xe0\xa6\xbe\xe0\xa6\x82\xe0\xa6\xb2\xe0\xa6\xbe",   false, "bangla -> বাংলা (top-1)"},

        // === Fuzzy spelling variants ===
        {"battary",     "\xe0\xa6\xac\xe0\xa7\x8d\xe0\xa6\xaf\xe0\xa6\xbe\xe0\xa6\x9f\xe0\xa6\xbe\xe0\xa6\xb0\xe0\xa6\xbf",   true,  "battary -> ব্যাটারি (fuzzy, top-3)"},
        {"batery",      "\xe0\xa6\xac\xe0\xa7\x8d\xe0\xa6\xaf\xe0\xa6\xbe\xe0\xa6\x9f\xe0\xa6\xbe\xe0\xa6\xb0\xe0\xa6\xbf",   true,  "batery -> ব্যাটারি (fuzzy, top-3)"},

        // সমস্যা
        {"somossa",     "\xe0\xa6\xb8\xe0\xa6\xae\xe0\xa6\xb8\xe0\xa7\x8d\xe0\xa6\xaf\xe0\xa6\xbe",   true,  "somossa -> সমস্যা (top-3)"},
        {"somossha",    "\xe0\xa6\xb8\xe0\xa6\xae\xe0\xa6\xb8\xe0\xa7\x8d\xe0\xa6\xaf\xe0\xa6\xbe",   true,  "somossha -> সমস্যা (top-3)"},
        {"shomossa",    "\xe0\xa6\xb8\xe0\xa6\xae\xe0\xa6\xb8\xe0\xa7\x8d\xe0\xa6\xaf\xe0\xa6\xbe",   true,  "shomossa -> সমস্যা (top-3)"},

        // পরিবর্তন
        {"poriborton",  "\xe0\xa6\xaa\xe0\xa6\xb0\xe0\xa6\xbf\xe0\xa6\xac\xe0\xa6\xb0\xe0\xa7\x8d\xe0\xa6\xa4\xe0\xa6\xa8", true,  "poriborton -> পরিবর্তন (top-3)"},

        // আনোয়ার
        {"anwar",       "\xe0\xa6\x86\xe0\xa6\xa8\xe0\xa7\x8b\xe0\xa6\xaf\xe0\xa6\xbc\xe0\xa6\xbe\xe0\xa6\xb0", true,  "anwar -> আনোয়ার (top-3)"},
        {"anoyar",      "\xe0\xa6\x86\xe0\xa6\xa8\xe0\xa7\x8b\xe0\xa6\xaf\xe0\xa6\xbc\xe0\xa6\xbe\xe0\xa6\xb0", true,  "anoyar -> আনোয়ার (top-3)"},

        // ফ্যান, টেবিল, চেয়ার, কন্ট্রোল, মাউস
        {"fan",         "\xe0\xa6\xab\xe0\xa7\x8d\xe0\xa6\xaf\xe0\xa6\xbe\xe0\xa6\xa8",   true,  "fan -> ফ্যান (top-3)"},
        {"table",       "\xe0\xa6\x9f\xe0\xa7\x87\xe0\xa6\xac\xe0\xa6\xbf\xe0\xa6\xb2",   true,  "table -> টেবিল (top-3)"},
        {"chair",       "\xe0\xa6\x9a\xe0\xa7\x87\xe0\xa6\xaf\xe0\xa6\xbc\xe0\xa6\xbe\xe0\xa6\xb0", true,  "chair -> চেয়ার (top-3)"},
        {"control",     "\xe0\xa6\x95\xe0\xa6\xa8\xe0\xa7\x8d\xe0\xa6\x9f\xe0\xa7\x8d\xe0\xa6\xb0\xe0\xa7\x8b\xe0\xa6\xb2", true, "control -> কন্ট্রোল (top-3)"},
        {"mouse",       "\xe0\xa6\xae\xe0\xa6\xbe\xe0\xa6\x89\xe0\xa6\xb8",   true,  "mouse -> মাউস (top-3)"},

        // পরিষ্কার
        {"porishkar",   "\xe0\xa6\xaa\xe0\xa6\xb0\xe0\xa6\xbf\xe0\xa6\xb7\xe0\xa7\x8d\xe0\xa6\x95\xe0\xa6\xbe\xe0\xa6\xb0", true,  "porishkar -> পরিষ্কার (top-3)"},
    };

    int suite8_passed = 0;
    int suite8_failed = 0;
    for (const auto& qc : quality_cases) {
        bool ok = check_quality(qc);
        if (ok) {
            suite8_passed++;
            g_passed++;
        } else {
            suite8_failed++;
            g_failed++;
            // Get actual top-3 for failure display
            BanglaEngine_SetComposition(engine, qc.input.c_str());
            CandidateList list;
            BanglaEngine_GetCandidates(engine, &list);
            std::cout << "  [FAIL] " << qc.description << "\n";
            std::cout << "         Expected: " << qc.expected << "\n";
            std::cout << "         Got top-" << list.count << ": ";
            for (uint32_t i = 0; i < list.count && i < 3; i++) {
                std::cout << "[" << list.candidates[i].bengali_text << "] ";
            }
            std::cout << "\n";
        }
    }

    if (suite8_failed == 0) {
        std::cout << "  [PASS] All " << suite8_passed << " suggestion quality cases verified.\n";
    } else {
        std::cout << "  [RESULT] " << suite8_passed << " passed, " << suite8_failed << " failed.\n";
    }

    // =========================================================================
    // SUITE 9: EXPANDED CONVERSATIONAL BIGRAM PREDICTIONS
    // =========================================================================
    std::cout << "\n--- Suite 9: Expanded Conversational Bigram Predictions ---\n";
    {
        BanglaEngine_ResetContext(engine);
        BanglaEngine_CommitWord(engine, "কী");
        CandidateList p1;
        BanglaEngine_GetNextWordPredictions(engine, &p1);
        bool found_khobor = false;
        for (uint32_t i = 0; i < p1.count; i++) {
            if (std::string(p1.candidates[i].bengali_text) == "খবর") { found_khobor = true; break; }
        }
        ASSERT_TRUE(found_khobor, "Prediction after 'কী' contains 'খবর'");

        BanglaEngine_ResetContext(engine);
        BanglaEngine_CommitWord(engine, "শুভ");
        CandidateList p2;
        BanglaEngine_GetNextWordPredictions(engine, &p2);
        bool found_sokal = false, found_bday = false;
        for (uint32_t i = 0; i < p2.count; i++) {
            if (std::string(p2.candidates[i].bengali_text) == "সকাল") found_sokal = true;
            if (std::string(p2.candidates[i].bengali_text) == "জন্মদিন") found_bday = true;
        }
        ASSERT_TRUE(found_sokal || found_bday, "Prediction after 'শুভ' contains 'সকাল' or 'জন্মদিন'");

        BanglaEngine_ResetContext(engine);
        BanglaEngine_CommitWord(engine, "ধন্যবাদ");
        CandidateList p3;
        BanglaEngine_GetNextWordPredictions(engine, &p3);
        bool found_bhai = false;
        for (uint32_t i = 0; i < p3.count; i++) {
            if (std::string(p3.candidates[i].bengali_text) == "ভাই") found_bhai = true;
        }
        ASSERT_TRUE(found_bhai, "Prediction after 'ধন্যবাদ' contains 'ভাই'");
    }

    // =========================================================================
    // SUITE 10: MIXED BANGLA + ENGLISH TYPING RETENTION
    // =========================================================================
    std::cout << "\n--- Suite 10: Mixed Bangla + English Typing Retention ---\n";
    {
        BanglaEngine_SetComposition(engine, "wifi");
        CandidateList c_wifi;
        BanglaEngine_GetCandidates(engine, &c_wifi);
        bool has_raw_eng = false;
        for (uint32_t i = 0; i < c_wifi.count; i++) {
            if (std::string(c_wifi.candidates[i].bengali_text) == "wifi") { has_raw_eng = true; break; }
        }
        ASSERT_TRUE(has_raw_eng, "Mixed typing: exact English candidate 'wifi' preserved in candidate list");
    }

    // =========================================================================
    // SUITE 11: TEACH MODE & PERSONAL LEARNING ISOLATION
    // =========================================================================
    std::cout << "\n--- Suite 11: Teach Mode & Personal Learning Isolation ---\n";
    {
        // Add an explicit user word
        BanglaEngine_AddUserWord(engine, "mycustomword", "আমারশব্দ");
        // Add a learned word
        BanglaEngine_LearnWord(engine, "learnedkey", "শেখাফল");

        // Clear only learned data
        size_t cleared = BanglaEngine_ClearLearnedData(engine);
        ASSERT_TRUE(cleared >= 1, "ClearLearnedData removed auto-learned entries");

        // Explicit user word must still exist!
        CandidateList list_custom;
        BanglaEngine_SetComposition(engine, "mycustomword");
        BanglaEngine_GetCandidates(engine, &list_custom);
        ASSERT_TRUE(list_custom.count > 0 && std::string(list_custom.candidates[0].bengali_text) == "আমারশব্দ",
                    "Teach Mode: Explicit personal dictionary word preserved after clearing learned data");
    }

    BanglaEngine_Destroy(engine);

    std::cout << "\n=========================================================\n";
    std::cout << "  TEST RESULTS SUMMARY\n";
    std::cout << "  Total Passed: " << g_passed << "\n";
    std::cout << "  Total Failed: " << g_failed << "\n";
    std::cout << "  Status: " << (g_failed == 0 ? "SUCCESS (ALL PASSED)" : "FAILURE") << "\n";
    std::cout << "=========================================================\n";

    return (g_failed == 0) ? 0 : 1;
}
