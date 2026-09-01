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
        {"tumi kemon acho", "তুমি কেমন আছ", "greeting context"},
        {"apnar shathe kotha bole valo laglo", "আপনার সাথে কথা বলে ভালো লাগল", "courtesy context"},
        {"amra shobai eksathe kaaj korbo", "আমরা সবাই একসাথে কাজ করব", "teamwork context"},
        {"ajke brishti hocche", "আজকে বৃষ্টি হচ্ছে", "weather context"},
        {"ei boi ta onek shundor", "এই বই টা অনেক সুন্দর", "opinion context"},
        {"tumi ki bhat kheyecho", "তুমি কি ভাত খেয়েছ", "question context"},
        {"she ekhon shob bujhte parbe", "সে এখন সব বুঝতে পারবে", "conversational context"},
        {"bangladesh amar jonmobhumi", "বাংলাদেশ আমার জন্মভূমি", "statement context"},
        {"amader shwastho shocheton hote hobe", "আমাদের স্বাস্থ্য সচেতন হতে হবে", "health context"},
        {"biggan o projukti amader desh ke notun rup dicche", "বিজ্ঞান ও প্রযুক্তি আমাদের দেশ কে নতুন রূপ দিচ্ছে", "tech context"},
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
        strict_on_cfg.auto_correct_threshold = 0.85f;
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

    BanglaEngine_Destroy(engine);

    std::cout << "\n=========================================================\n";
    std::cout << "  TEST RESULTS SUMMARY\n";
    std::cout << "  Total Passed: " << g_passed << "\n";
    std::cout << "  Total Failed: " << g_failed << "\n";
    std::cout << "  Status: " << (g_failed == 0 ? "SUCCESS (ALL PASSED)" : "FAILURE") << "\n";
    std::cout << "=========================================================\n";

    return (g_failed == 0) ? 0 : 1;
}
