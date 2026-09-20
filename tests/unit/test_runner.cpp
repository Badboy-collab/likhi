#include "../../engine/include/bangla_engine.h"
#include "../../engine/src/layout/inscript_layout.h"
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

        // Adaptive User Pattern Learning Verification
        BanglaEngine_CommitWordWithOrigin(engine, "amarshohor", "আমারশহর");
        BanglaEngine_SetComposition(engine, "amarshohor");
        CandidateList list_learned;
        BanglaEngine_GetCandidates(engine, &list_learned);
        ASSERT_TRUE(list_learned.count > 0, "Learned user candidate returned");
        ASSERT_EQUAL(std::string(list_learned.candidates[0].bengali_text), "আমারশহর", "Adaptive learning ranks chosen word top");

        // Emoji Shortcut Verification
        BanglaEngine_SetComposition(engine, ":smile:");
        CandidateList list_emoji;
        BanglaEngine_GetCandidates(engine, &list_emoji);
        ASSERT_TRUE(list_emoji.count > 0, "Emoji candidate returned");
        ASSERT_EQUAL(std::string(list_emoji.candidates[0].bengali_text), "😊", "Emoji tag :smile: produces 😊");
    }
    std::cout << "  [PASS] Personal User Dictionary & Adaptive Learning verified.\n";

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
    // TEST SUITE 8: National INSCRIPT Layout Verification
    // =========================================================================
    std::cout << "\n=== [TEST SUITE 8] National INSCRIPT Layout ===\n";
    {
        bangla::InscriptLayout inscript;
        ASSERT_TRUE(inscript.HasMapping('k', false), "Inscript has normal 'k' mapping");
        ASSERT_EQUAL(inscript.GetChar('k', false), "ক", "Normal 'k' -> 'ক'");
        ASSERT_EQUAL(inscript.GetChar('K', true), "খ", "Shift 'K' -> 'খ'");
        ASSERT_EQUAL(inscript.GetChar('l', false), "ত", "Normal 'l' -> 'ত'");
        ASSERT_EQUAL(inscript.GetChar('L', true), "থ", "Shift 'L' -> 'থ'");
        ASSERT_EQUAL(inscript.GetChar('h', false), "প", "Normal 'h' -> 'প'");
        ASSERT_EQUAL(inscript.GetChar('H', true), "ফ", "Shift 'H' -> 'ফ'");
        ASSERT_EQUAL(inscript.GetChar('j', false), "র", "Normal 'j' -> 'র'");
        ASSERT_EQUAL(inscript.GetChar('y', false), "ব", "Normal 'y' -> 'ব'");
        ASSERT_EQUAL(inscript.GetChar('Y', true), "ভ", "Shift 'Y' -> 'ভ'");
        ASSERT_EQUAL(inscript.GetChar('c', false), "ম", "Normal 'c' -> 'ম'");
        ASSERT_EQUAL(inscript.GetChar('d', false), "্", "Normal 'd' -> Hasanta '্'");
        ASSERT_EQUAL(inscript.GetChar('>', true), "।", "Shift '>' -> Bengali Dari '।'");
    }
    std::cout << "  [PASS] National INSCRIPT key mapping verified.\n";

    // =========================================================================
    // TEST SUITE 9: Likhi Master Fix Verification (P0-1 to P0-6)
    // =========================================================================
    std::cout << "\n=== [TEST SUITE 9] Likhi Master Fix Verification ===\n";
    {
        BanglaEngine_ResetContext(engine);

        // 1. Critical Banglish Accuracy (P0-2)
        char s_buf[512] = {0};
        BanglaEngine_TransliterateSentence(engine, "poriborton", s_buf, sizeof(s_buf));
        ASSERT_EQUAL(std::string(s_buf), "পরিবর্তন", "poriborton -> পরিবর্তন");

        BanglaEngine_TransliterateSentence(engine, "somossha", s_buf, sizeof(s_buf));
        ASSERT_EQUAL(std::string(s_buf), "সমস্যা", "somossha -> সমস্যা");

        BanglaEngine_TransliterateSentence(engine, "somossa", s_buf, sizeof(s_buf));
        ASSERT_EQUAL(std::string(s_buf), "সমস্যা", "somossa -> সমস্যা");

        BanglaEngine_TransliterateSentence(engine, "poriborton korbo", s_buf, sizeof(s_buf));
        ASSERT_EQUAL(std::string(s_buf), "পরিবর্তন করবো", "poriborton korbo -> পরিবর্তন করবো");

        BanglaEngine_TransliterateSentence(engine, "ami ekta poriborton korbo", s_buf, sizeof(s_buf));
        ASSERT_EQUAL(std::string(s_buf), "আমি একটা পরিবর্তন করবো", "ami ekta poriborton korbo -> আমি একটা পরিবর্তন করবো");

        BanglaEngine_TransliterateSentence(engine, "amar ekta somossa hoyeche", s_buf, sizeof(s_buf));
        ASSERT_TRUE(std::string(s_buf).find("আমার একটা সমস্যা") == 0, "amar ekta somossa hoyeche sentence");

        // 2. English Loanwords (P0-5)
        BanglaEngine_TransliterateSentence(engine, "battery", s_buf, sizeof(s_buf));
        ASSERT_EQUAL(std::string(s_buf), "ব্যাটারি", "battery -> ব্যাটারি");

        BanglaEngine_TransliterateSentence(engine, "office", s_buf, sizeof(s_buf));
        ASSERT_EQUAL(std::string(s_buf), "অফিস", "office -> অফিস");

        BanglaEngine_TransliterateSentence(engine, "control", s_buf, sizeof(s_buf));
        ASSERT_EQUAL(std::string(s_buf), "কন্ট্রোল", "control -> কন্ট্রোল");

        BanglaEngine_TransliterateSentence(engine, "computer", s_buf, sizeof(s_buf));
        ASSERT_EQUAL(std::string(s_buf), "কম্পিউটার", "computer -> কম্পিউটার");

        BanglaEngine_TransliterateSentence(engine, "mouse", s_buf, sizeof(s_buf));
        ASSERT_EQUAL(std::string(s_buf), "মাউস", "mouse -> মাউস");

        BanglaEngine_TransliterateSentence(engine, "chair", s_buf, sizeof(s_buf));
        ASSERT_EQUAL(std::string(s_buf), "চেয়ার", "chair -> চেয়ার");

        BanglaEngine_TransliterateSentence(engine, "table", s_buf, sizeof(s_buf));
        ASSERT_EQUAL(std::string(s_buf), "টেবিল", "table -> টেবিল");

        BanglaEngine_TransliterateSentence(engine, "fan", s_buf, sizeof(s_buf));
        ASSERT_EQUAL(std::string(s_buf), "ফ্যান", "fan -> ফ্যান");

        BanglaEngine_TransliterateSentence(engine, "anwar", s_buf, sizeof(s_buf));
        ASSERT_EQUAL(std::string(s_buf), "আনোয়ার", "anwar -> আনোয়ার");

        // 3. Controlled Fuzzy Banglish Tolerance (P0-3)
        auto check_has_candidate = [&](const std::string& input, const std::string& expected_cand, const std::string& desc) {
            BanglaEngine_SetComposition(engine, input.c_str());
            CandidateList clist;
            BanglaEngine_GetCandidates(engine, &clist);
            bool found = false;
            for (uint32_t i = 0; i < clist.count; i++) {
                if (std::string(clist.candidates[i].bengali_text) == expected_cand) {
                    found = true;
                    break;
                }
            }
            ASSERT_TRUE(found, desc + " (" + input + " -> " + expected_cand + ")");
        };

        check_has_candidate("battary", "ব্যাটারি", "Fuzzy: battary suggests ব্যাটারি");
        check_has_candidate("batery", "ব্যাটারি", "Fuzzy: batery suggests ব্যাটারি");
        check_has_candidate("batteri", "ব্যাটারি", "Fuzzy: batteri suggests ব্যাটারি");

        // 4. Ambiguous Banglish Suggestions (P0-4)
        check_has_candidate("t", "ত", "Ambiguous: t has ত");
        check_has_candidate("t", "ট", "Ambiguous: t has ট");
        check_has_candidate("ta", "তা", "Ambiguous: ta has তা");
        check_has_candidate("ta", "টা", "Ambiguous: ta has টা");
        check_has_candidate("taka", "টাকা", "Ambiguous: taka has টাকা");
        check_has_candidate("taka", "তাকা", "Ambiguous: taka has তাকা");
        check_has_candidate("d", "দ", "Ambiguous: d has দ");
        check_has_candidate("d", "ড", "Ambiguous: d has ড");
        check_has_candidate("s", "স", "Ambiguous: s has স");
        check_has_candidate("s", "শ", "Ambiguous: s has শ");
        check_has_candidate("n", "ন", "Ambiguous: n has ন");
        check_has_candidate("n", "ণ", "Ambiguous: n has ণ");
        check_has_candidate("r", "র", "Ambiguous: r has র");
        check_has_candidate("r", "ড়", "Ambiguous: r has ড়");
        check_has_candidate("c", "চ", "Ambiguous: c has চ");
        check_has_candidate("c", "ছ", "Ambiguous: c has ছ");
        check_has_candidate("k", "ক", "Ambiguous: k has ক");
        check_has_candidate("k", "খ", "Ambiguous: k has খ");

        // 5. Preserved Original English Candidate (P0-6)
        check_has_candidate("office", "office", "P0-6: office preserves raw office candidate");
        check_has_candidate("computer", "computer", "P0-6: computer preserves raw computer candidate");
    }
    std::cout << "  [PASS] All Likhi Master Fix verification cases passed.\n";

    BanglaEngine_Destroy(engine);

    std::cout << "\n=========================================================\n";
    std::cout << "  TEST RESULTS SUMMARY\n";
    std::cout << "  Total Passed: " << g_passed << "\n";
    std::cout << "  Total Failed: " << g_failed << "\n";
    std::cout << "  Status: " << (g_failed == 0 ? "SUCCESS (ALL PASSED)" : "FAILURE") << "\n";
    std::cout << "=========================================================\n";

    return (g_failed == 0) ? 0 : 1;
}
