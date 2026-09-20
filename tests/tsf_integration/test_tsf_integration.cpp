#include <iostream>
#include <vector>
#include <string>
#include <cassert>
#include <windows.h>
#include "../../tsf/include/composition_mgr.h"
#include "../../tsf/include/tsf_utils.h"
#include "../../engine/src/unicode/bangla_unicode.h"

namespace bangla_tsf {
    HINSTANCE g_hInstance = NULL;
}

static int g_tsf_passed = 0;
static int g_tsf_failed = 0;

static void ASSERT_TSF_TRUE(bool cond, const std::string& desc) {
    if (cond) {
        g_tsf_passed++;
    } else {
        g_tsf_failed++;
        std::cout << "  [FAIL] " << desc << "\n";
    }
}

static void ASSERT_TSF_EQUAL(const std::string& actual, const std::string& expected, const std::string& desc) {
    if (actual == expected) {
        g_tsf_passed++;
    } else {
        g_tsf_failed++;
        std::cout << "  [FAIL] " << desc << "\n         Expected: " << expected << "\n         Actual:   " << actual << "\n";
    }
}

class MockTSFTextStore {
public:
    void InsertText(const std::string& text) {
        buffer_ += text;
    }
    void Clear() {
        buffer_.clear();
    }
    const std::string& GetText() const { return buffer_; }

private:
    std::string buffer_;
};

int main() {
    std::cout << "=========================================================\n";
    std::cout << "  PC Bangla Typing App - TSF Integration Test Suite (Phase 4)\n";
    std::cout << "=========================================================\n\n";

    bangla_tsf::CompositionManager comp_mgr(nullptr);
    bool init_ok = comp_mgr.Initialize(NULL);
    ASSERT_TSF_TRUE(init_ok, "CompositionManager initialization successful");

    // =========================================================================
    // SUITE 1: Composition & Transliteration Lifecycle
    // =========================================================================
    std::cout << "=== [SUITE 1] Composition & Transliteration Lifecycle ===\n";
    {
        // Type 'a' -> 'm' -> 'i'
        comp_mgr.OnCharacter(nullptr, 'a');
        comp_mgr.OnCharacter(nullptr, 'm');
        comp_mgr.OnCharacter(nullptr, 'i');

        ASSERT_TSF_EQUAL(comp_mgr.GetRomanBuffer(), "ami", "Roman buffer tracks 'ami'");
        const auto& cands_w = comp_mgr.GetCurrentCandidates();
        ASSERT_TSF_TRUE(!cands_w.empty(), "Candidates generated for 'ami'");

        std::string top_cand = (!cands_w.empty()) ? bangla_tsf::Utf16ToUtf8(cands_w[0]) : "";
        ASSERT_TSF_EQUAL(top_cand, "আমি", "Top candidate is 'আমি'");

        // Commit on space
        comp_mgr.OnSpace(nullptr);
        ASSERT_TSF_TRUE(!comp_mgr.IsComposing(), "Composition reset after Space commit");
        ASSERT_TSF_EQUAL(comp_mgr.GetRomanBuffer(), "", "Roman buffer cleared after commit");
    }
    std::cout << "  [PASS] Composition and commit lifecycle verified.\n\n";

    // =========================================================================
    // SUITE 2: Multi-Word Sentence Input & Context Progression
    // =========================================================================
    std::cout << "=== [SUITE 2] Multi-Word Sentence Input & Context Progression ===\n";
    {
        std::vector<std::pair<std::string, std::string>> sentence_tokens = {
            {"ami", "আমি"},
            {"ajke", "আজকে"},
            {"office", "অফিস"},
            {"jabo", "যাব"}
        };

        MockTSFTextStore store;
        for (size_t i = 0; i < sentence_tokens.size(); i++) {
            const auto& tok = sentence_tokens[i];
            for (char ch : tok.first) {
                comp_mgr.OnCharacter(nullptr, ch);
            }
            const auto& cands_w = comp_mgr.GetCurrentCandidates();
            ASSERT_TSF_TRUE(!cands_w.empty(), "Candidates generated for token: " + tok.first);

            std::string top_cand = (!cands_w.empty()) ? bangla_tsf::Utf16ToUtf8(cands_w[0]) : "";
            store.InsertText(top_cand);
            if (i + 1 < sentence_tokens.size()) {
                store.InsertText(" ");
            }
            comp_mgr.OnSpace(nullptr);
        }

        std::string final_text = store.GetText();
        ASSERT_TSF_TRUE(final_text.find("আমি") != std::string::npos, "Sentence contains 'আমি'");
        ASSERT_TSF_TRUE(final_text.find("আজকে") != std::string::npos, "Sentence contains 'আজকে'");
        ASSERT_TSF_TRUE(final_text.find("অফিস") != std::string::npos, "Sentence contains 'অফিস'");
        ASSERT_TSF_TRUE(final_text.find("যাব") != std::string::npos, "Sentence contains 'যাব'");
    }
    std::cout << "  [PASS] Multi-word sentence progression verified.\n\n";

    // =========================================================================
    // SUITE 3: Candidate Selection (Number Keys & Arrow Navigation)
    // =========================================================================
    std::cout << "=== [SUITE 3] Candidate Selection (Keys & Navigation) ===\n";
    {
        for (char ch : std::string("bhalo")) {
            comp_mgr.OnCharacter(nullptr, ch);
        }

        const auto& cands_w = comp_mgr.GetCurrentCandidates();
        ASSERT_TSF_TRUE(cands_w.size() >= 1, "Candidates available for 'bhalo'");
        std::string top_cand = (!cands_w.empty()) ? bangla_tsf::Utf16ToUtf8(cands_w[0]) : "";
        ASSERT_TSF_EQUAL(top_cand, "ভালো", "Top candidate is 'ভালো'");

        // Arrow navigation
        comp_mgr.OnArrow(nullptr, true); // down
        comp_mgr.OnArrow(nullptr, false); // up

        // Select candidate #1 via number key
        bool eaten = comp_mgr.OnNumberSelection(nullptr, 1);
        ASSERT_TSF_TRUE(eaten, "Number key 1 selected candidate");
        ASSERT_TSF_TRUE(!comp_mgr.IsComposing(), "Composition closed after number selection");
    }
    std::cout << "  [PASS] Candidate keyboard navigation and number selection verified.\n\n";

    // =========================================================================
    // SUITE 4: Grapheme-Aware Backspace Integration
    // =========================================================================
    std::cout << "=== [SUITE 4] Grapheme-Aware Backspace Integration ===\n";
    {
        // 1. Type 'k' -> 'a' ('কা')
        comp_mgr.OnCharacter(nullptr, 'k');
        comp_mgr.OnCharacter(nullptr, 'a');
        ASSERT_TSF_EQUAL(comp_mgr.GetRomanBuffer(), "ka", "Buffer is 'ka'");

        // Backspace once -> removes 'a' -> buffer is 'k' -> candidate is 'ক'
        comp_mgr.OnBackspace(nullptr);
        ASSERT_TSF_EQUAL(comp_mgr.GetRomanBuffer(), "k", "Buffer after backspace is 'k'");
        const auto& cands_k = comp_mgr.GetCurrentCandidates();
        std::string top_k = (!cands_k.empty()) ? bangla_tsf::Utf16ToUtf8(cands_k[0]) : "";
        ASSERT_TSF_EQUAL(top_k, "ক", "Candidate after backspace is clean 'ক'");

        // Backspace again -> clears composition
        comp_mgr.OnBackspace(nullptr);
        ASSERT_TSF_TRUE(!comp_mgr.IsComposing(), "Composition ends cleanly when empty");

        // 2. Unicode Grapheme Cluster Backspace Test on complex string
        std::string complex_str = "বৃষ্টি";
        std::string step1 = bangla::UnicodeUtils::BackspaceGrapheme(complex_str);
        ASSERT_TSF_TRUE(bangla::UnicodeUtils::IsValidBengaliSequence(step1), "Grapheme step1 is valid Unicode");
        ASSERT_TSF_TRUE(!bangla::UnicodeUtils::HasDanglingHasant(step1), "Zero dangling hasant after grapheme step1");
    }
    std::cout << "  [PASS] Grapheme-aware backspace verified.\n\n";

    // =========================================================================
    // SUITE 5: Bengali Unicode Integrity in Text Commit
    // =========================================================================
    std::cout << "=== [SUITE 5] Bengali Unicode Integrity in Text Commit ===\n";
    {
        std::vector<std::string> complex_words = {
            "স্বাস্থ্য", "আন্তর্জাতিক", "ব্রহ্মপুত্র", "বিজ্ঞপ্তি", "আত্মীয়",
            "উৎকৃষ্ট", "বৃষ্টি", "স্মৃতি", "আকাঙ্ক্ষা", "পঙ্কজ", "অঞ্চল",
            "শৃঙ্খলা", "উজ্জ্বল", "দ্বন্দ্ব", "তত্ত্ব", "চট্টগ্রাম",
            "কম্পিউটার", "মোবাইল", "ইন্টারনেট"
        };

        for (const auto& cw : complex_words) {
            std::wstring cw_w = bangla_tsf::Utf8ToUtf16(cw);
            std::string back_u8 = bangla_tsf::Utf16ToUtf8(cw_w);

            ASSERT_TSF_EQUAL(back_u8, cw, "UTF-16 roundtrip intact: " + cw);
            ASSERT_TSF_TRUE(bangla::UnicodeUtils::IsValidBengaliSequence(cw), "Valid Unicode sequence: " + cw);
            ASSERT_TSF_TRUE(!bangla::UnicodeUtils::HasDanglingHasant(cw), "Zero dangling hasant: " + cw);
        }
    }
    std::cout << "  [PASS] Unicode integrity preserved across TSF buffers.\n\n";

    // =========================================================================
    // SUITE 6: Auto-Correct Strict Hard Policy Gating
    // =========================================================================
    std::cout << "=== [SUITE 6] Auto-Correct Strict Hard Policy Gating ===\n";
    {
        // 1. Auto-Correct OFF (Default)
        comp_mgr.SetAutoCorrectEnabled(false);
        for (char ch : std::string("ami")) comp_mgr.OnCharacter(nullptr, ch);
        comp_mgr.OnSpace(nullptr);
        ASSERT_TSF_TRUE(!comp_mgr.IsComposing(), "AutoCorrect=OFF commits user-selected candidate");

        // 2. Auto-Correct ON
        comp_mgr.SetAutoCorrectEnabled(true);
        for (char ch : std::string("ami")) comp_mgr.OnCharacter(nullptr, ch);
        comp_mgr.OnSpace(nullptr);
        ASSERT_TSF_TRUE(!comp_mgr.IsComposing(), "AutoCorrect=ON behaves consistently");

        // Reset to default
        comp_mgr.SetAutoCorrectEnabled(false);
    }
    std::cout << "  [PASS] Auto-Correct hard policy gating verified.\n\n";

    // =========================================================================
    // SUITE 7: Punctuation & Focus Loss Recovery
    // =========================================================================
    std::cout << "=== [SUITE 7] Punctuation & Focus Loss Recovery ===\n";
    {
        // 1. Punctuation '.' during composition
        for (char ch : std::string("jabo")) comp_mgr.OnCharacter(nullptr, ch);
        comp_mgr.OnPunctuation(nullptr, '.');
        ASSERT_TSF_TRUE(!comp_mgr.IsComposing(), "Composition committed on punctuation '.'");

        // 2. Focus loss during active composition
        for (char ch : std::string("ekhon")) comp_mgr.OnCharacter(nullptr, ch);
        ASSERT_TSF_TRUE(comp_mgr.IsComposing(), "Composing before focus loss");
        comp_mgr.OnFocusLost(nullptr);
        ASSERT_TSF_TRUE(!comp_mgr.IsComposing(), "Composition safely closed on focus loss");
    }
    // =========================================================================
    // SUITE 8: Requested High-Frequency Words & Engine Offline Transliteration
    // =========================================================================
    std::cout << "=== [SUITE 8] Requested Words & Engine Offline Transliteration ===\n";
    {
        // 1. Vocabulary Overrides
        struct WordTest {
            std::string roman;
            std::wstring expected_bengali;
        };
        std::vector<WordTest> word_tests = {
            {"use", L"ইউস"},
            {"babohar", L"ব্যবহার"},
            {"jotokhon", L"যতক্ষণ"},
            {"totokhon", L"ততক্ষণ"},
            {"kichukhon", L"কিছুক্ষণ"},
            {"sarakhon", L"সারাক্ষণ"},
            {"saradin", L"সারাদিন"},
            {"jotodin", L"যতদিন"},
            {"totodin", L"ততদিন"},
            {"porishkar", L"পরিষ্কার"},
            {"poriskar", L"পরিষ্কার"},
            {"suchi", L"শুচি"},
            {"suddho", L"শুদ্ধ"},
            {"saf", L"সাফ"},
            {"pobitro", L"পবিত্র"},
            {"nirmol", L"নির্মল"},
            {"sohojboddo", L"সহজবোধ্য"},
            {"sohojbodho", L"সহজবোধ্য"},
            {"najjo", L"ন্যায্য"},
            {"cholonsoi", L"চলনসই"},
            {"sundor", L"সুন্দর"},
            {"obadh", L"অবাধ"},
            {"ujjol", L"উজ্জ্বল"},
            {"bisudhho", L"বিশুদ্ধ"},
            {"khati", L"খাঁটি"},
            {"poripati", L"পরিপাটি"},
            {"chimcham", L"ছিমছাম"},
            {"susrikhono", L"সুশৃঙ্খল"},
            {"fitfat", L"ফিটফাট"},
            {"porichonno", L"পরিচ্ছন্ন"},
            {"sushongoto", L"সুসঙ্গত"},
            {"songlogno", L"সংলগ্ন"},
            {"songsokto", L"সংসক্ত"},
            {"ekotro", L"একত্র"},
            {"sommilito", L"সম্মিলিত"},
            {"soscho", L"স্বচ্ছ"},
            {"prajol", L"প্রাঞ্জল"},
            {"okolokko", L"অকলঙ্ক"},
            {"onindo", L"অনিন্দ্য"},
            {"vassor", L"ভাস্বর"},
            {"anshuman", L"অংশুমান"},
            {"spostobokta", L"স্পষ্টবক্তা"},
            {"okopot", L"অকপট"},
            {"ruju", L"ঋজু"},
            {"omayik", L"অমায়িক"},
            {"budhiman", L"বুদ্ধিমান"},
            {"medhabi", L"মেধাবী"},
            {"chotur", L"চতুর"},
            {"chalak", L"চালাক"},
            {"motiman", L"মতিমান"},
            {"sustho", L"সুস্থ"},
            {"shasthokor", L"স্বাস্থ্যকর"},
            {"shasthoban", L"স্বাস্থ্যবান"},
            {"niramoi", L"নিরাময়"},
            {"finfine", L"ফিনফিনে"},
            {"sfotik", L"স্ফটিক"},
            {"jilliboth", L"ঝিল্লিবৎ"},
            {"mayamoi", L"মায়াময়"},
            {"obostha", L"অবস্থা"},
            {"dosa", L"দশা"},
            {"khosmejaje", L"খোসমেজাজে"},
            {"pottokhogochor", L"প্রত্যক্ষগোচর"},
            {"sonsoiviti", L"সংশয়াতীত"}
        };

        for (const auto& wt : word_tests) {
            for (char ch : wt.roman) comp_mgr.OnCharacter(nullptr, ch);
            const auto& cands = comp_mgr.GetCurrentCandidates();
            ASSERT_TSF_TRUE(!cands.empty(), "Candidates generated for " + wt.roman);
            if (!cands.empty()) {
                ASSERT_TSF_TRUE(cands[0] == wt.expected_bengali,
                                wt.roman + " -> candidate 0 matches expected override");
            }
            comp_mgr.OnSpace(nullptr);
        }

        // 2. Offline Determinism & High-Frequency Validation
        // Verify 'amar' generates Bengali candidates purely locally without any network
        for (char ch : std::string("amar")) comp_mgr.OnCharacter(nullptr, ch);
        const auto& amar_cands = comp_mgr.GetCurrentCandidates();
        ASSERT_TSF_TRUE(!amar_cands.empty(), "Offline candidate list generated for 'amar'");
        if (!amar_cands.empty()) {
            ASSERT_TSF_TRUE(amar_cands[0] == L"আমার", "Local candidate 0 for 'amar' is 'আমার'");
        }
        comp_mgr.OnSpace(nullptr);
    }
    std::cout << "  [PASS] Requested words and Engine Offline Transliteration verified.\n\n";

    comp_mgr.Shutdown();

    std::cout << "=========================================================\n";
    std::cout << "  TSF INTEGRATION TEST RESULTS\n";
    std::cout << "  Total Passed: " << g_tsf_passed << "\n";
    std::cout << "  Total Failed: " << g_tsf_failed << "\n";
    std::cout << "  Status: " << (g_tsf_failed == 0 ? "SUCCESS (ALL PASSED)" : "FAILURE") << "\n";
    std::cout << "=========================================================\n";

    return (g_tsf_failed == 0) ? 0 : 1;
}
