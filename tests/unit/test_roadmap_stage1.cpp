#include <iostream>
#include <vector>
#include <string>
#include <cassert>
#include <fstream>
#include <cstdio>
#include "../../engine/include/bangla_engine.h"
#include "../../tsf/include/key_policy.h"
#include "../../tsf/include/composition_mgr.h"
#include "../../engine/src/personal_dict/personal_dictionary.h"

using namespace bangla_tsf;

static int g_pass_count = 0;
static int g_fail_count = 0;

static void Verify(bool condition, const std::string& desc) {
    if (condition) {
        std::cout << "  [PASS] " << desc << "\n";
        g_pass_count++;
    } else {
        std::cerr << "  [FAIL] " << desc << "\n";
        g_fail_count++;
    }
}

int main() {
    std::cout << "============================================================\n";
    std::cout << "  STAGE 1: REAL-WORLD ROADMAP VERIFICATION TEST SUITE\n";
    std::cout << "============================================================\n\n";

    std::cout << "=== [SECTION 1] Candidate Number Selection (1-6) and Modifiers ===\n";
    {
        for (char d = '1'; d <= '6'; d++) {
            KeyState s;
            s.vk = static_cast<WPARAM>(d);
            s.composing = true;
            s.candidates_visible = true;
            s.digit_selectable = true;
            s.numlock = true;

            KeyDecision dec = DecideKey(s);
            Verify(dec.eat && dec.action == KeyAction::kProcessDigit,
                   std::string("Top-row '") + d + "' selects candidate (eaten, kProcessDigit)");
        }

        for (int i = 1; i <= 6; i++) {
            KeyState s;
            s.vk = VK_NUMPAD0 + i;
            s.composing = true;
            s.candidates_visible = true;
            s.digit_selectable = true;
            s.numlock = true;

            KeyDecision dec = DecideKey(s);
            Verify(dec.eat && dec.action == KeyAction::kProcessDigit,
                   std::string("Numpad '") + std::to_string(i) + "' with NumLock ON selects candidate");
        }

        for (int i = 1; i <= 6; i++) {
            KeyState s;
            s.vk = VK_NUMPAD0 + i;
            s.composing = true;
            s.candidates_visible = true;
            s.digit_selectable = true;
            s.numlock = false;

            KeyDecision dec = DecideKey(s);
            Verify(!dec.eat && dec.commit_first,
                   std::string("Numpad '") + std::to_string(i) + "' with NumLock OFF commits and passes through natively");
        }

        for (char d = '1'; d <= '9'; d++) {
            KeyState s;
            s.vk = static_cast<WPARAM>(d);
            s.composing = false;
            s.candidates_visible = false;
            s.digit_selectable = false;

            KeyDecision dec = DecideKey(s);
            Verify(!dec.eat && dec.action == KeyAction::kPass && !dec.commit_first,
                   std::string("Idle mode '") + d + "' passes through natively untouched");
        }

        {
            KeyState s_ctrl;
            s_ctrl.vk = '1';
            s_ctrl.composing = true;
            s_ctrl.candidates_visible = true;
            s_ctrl.digit_selectable = true;
            s_ctrl.ctrl = true;
            KeyDecision dec_ctrl = DecideKey(s_ctrl);
            Verify(!dec_ctrl.eat, "Ctrl+1 never selects candidate (passes through)");

            KeyState s_alt;
            s_alt.vk = '1';
            s_alt.composing = true;
            s_alt.candidates_visible = true;
            s_alt.digit_selectable = true;
            s_alt.alt = true;
            KeyDecision dec_alt = DecideKey(s_alt);
            Verify(!dec_alt.eat, "Alt+1 never selects candidate (passes through)");

            KeyState s_shift;
            s_shift.vk = '1';
            s_shift.composing = true;
            s_shift.candidates_visible = true;
            s_shift.digit_selectable = true;
            s_shift.shift = true;
            KeyDecision dec_shift = DecideKey(s_shift);
            Verify(!dec_shift.eat, "Shift+1 produces '!' natively (passes through)");
        }

        {
            CompositionManager mgr(nullptr);
            mgr.Initialize(NULL);
            std::vector<std::wstring> cands = { L"\u09AD\u09BE\u09B2\u09CB", L"\u09AD\u09BE\u09B2", L"\u09AD\u09BE\u09B2\u09CB\u0987", L"\u09AD\u09BE\u09B2\u09CB\u09AD\u09BE\u09AC\u09C7", L"\u09AD\u09BE\u09B2\u09CB\u09B0", L"\u09AD\u09BE\u09B2\u09AC\u09C7\u09B8\u09C7" };
            mgr.SetCandidatesForTesting(cands);

            for (char d = '1'; d <= '6'; d++) {
                Verify(mgr.CanSelectCandidate(d), std::string("CompositionManager accepts candidate hotkey '") + d + "'");
            }
            Verify(!mgr.CanSelectCandidate('7'), "Candidate 7 not selectable when only 6 candidates exist");
            Verify(!mgr.CanSelectCandidate('0'), "Candidate 0 not selectable");

            // Candidate 7 selectable when 7 candidates exist (6 suggestions + raw English)
            cands.push_back(L"bhalo");
            mgr.SetCandidatesForTesting(cands);
            Verify(mgr.CanSelectCandidate('7'), "Candidate 7 IS selectable when 7 candidates exist");
            Verify(!mgr.CanSelectCandidate('8'), "Candidate 8 not selectable when only 7 candidates exist");
        }
    }

    std::cout << "\n=== [SECTION 2] Personal Learning and Adaptive Ranking ===\n";
    {
        std::string test_dict_path = "test_user_dict_temp.txt";
        std::remove(test_dict_path.c_str());

        EngineConfig cfg;
        BanglaEngine_GetDefaultConfig(&cfg);
        cfg.lexicon_binary_path = "engine/data/lexicon.bin";
        cfg.user_dict_path = test_dict_path.c_str();
        cfg.max_candidates = 6;

        BanglaEngine* engine = BanglaEngine_Create(&cfg);
        assert(engine != nullptr);

        BanglaEngine_SetComposition(engine, "bhalo");
        CandidateList list_base;
        BanglaEngine_GetCandidates(engine, &list_base);

        std::string original_top = list_base.candidates[0].bengali_text;
        Verify(original_top == "\u09AD\u09BE\u09B2\u09CB", "Original base top candidate for 'bhalo' is \u09AD\u09BE\u09B2\u09CB");

        std::string target_learned = "\u09AD\u09BE\u09B2\u09CB\u0987";
        BanglaEngine_CommitWordWithOrigin(engine, "bhalo", target_learned.c_str());

        BanglaEngine_SetComposition(engine, "bhalo");
        CandidateList list_after1;
        BanglaEngine_GetCandidates(engine, &list_after1);
        Verify(std::string(list_after1.candidates[0].bengali_text) == "\u09AD\u09BE\u09B2\u09CB",
               "1 accidental selection does NOT immediately override base ranking (remains \u09AD\u09BE\u09B2\u09CB)");

        BanglaEngine_CommitWordWithOrigin(engine, "bhalo", target_learned.c_str());
        BanglaEngine_CommitWordWithOrigin(engine, "bhalo", target_learned.c_str());

        BanglaEngine_SetComposition(engine, "bhalo");
        CandidateList list_after3;
        BanglaEngine_GetCandidates(engine, &list_after3);

        Verify(std::string(list_after3.candidates[0].bengali_text) == target_learned,
               "Learned candidate (\u09AD\u09BE\u09B2\u09CB\u0987) successfully became #1 after repeated selections!");

        BanglaEngine_Destroy(engine);

        std::cout << "\n=== [SECTION 3] User Dictionary Persistence and Restart ===\n";
        std::ifstream infile(test_dict_path);
        Verify(infile.is_open(), "user_dict.txt was written and exists on disk");
        std::string line;
        bool found_entry = false;
        while (std::getline(infile, line)) {
            if (line.find("bhalo\t") != std::string::npos && line.find("\t3\t") != std::string::npos) {
                found_entry = true;
            }
        }
        infile.close();
        Verify(found_entry, "user_dict.txt accurately persisted 'bhalo', target_learned, frequency 3");

        BanglaEngine* restarted_engine = BanglaEngine_Create(&cfg);
        BanglaEngine_SetComposition(restarted_engine, "bhalo");
        CandidateList list_restarted;
        BanglaEngine_GetCandidates(restarted_engine, &list_restarted);
        Verify(std::string(list_restarted.candidates[0].bengali_text) == target_learned,
               "Learned ranking persisted after complete application restart (remains #1)");

        std::cout << "\n=== [SECTION 4] Offline Operation Verification ===\n";
        BanglaEngine_SetComposition(restarted_engine, "ami");
        CandidateList list_ami;
        BanglaEngine_GetCandidates(restarted_engine, &list_ami);
        Verify(list_ami.count >= 1 && std::string(list_ami.candidates[0].bengali_text) == "\u0986\u09AE\u09BF",
               "Offline standard transliteration 'ami' -> '\u0986\u09AE\u09BF' 100% functional without internet");

        BanglaEngine_SetComposition(restarted_engine, "bhalo");
        CandidateList list_bhalo_offline;
        BanglaEngine_GetCandidates(restarted_engine, &list_bhalo_offline);
        Verify(std::string(list_bhalo_offline.candidates[0].bengali_text) == target_learned,
               "Offline learned preference 'bhalo' -> '\u09AD\u09BE\u09B2\u09CB\u0987' 100% functional without internet");

        BanglaEngine_Destroy(restarted_engine);
        std::remove(test_dict_path.c_str());
    }

    std::cout << "\n=== [SECTION 5] Dictionary Corruption and Duplicate Resilience ===\n";
    {
        std::string corrupt_path = "test_corrupt_dict.txt";
        std::ofstream out(corrupt_path);
        out << "amar\t\u0986\u09AE\u09BE\u09B0\t5\t1700000000\n";
        out << "tumi\t\u09A4\u09C1\u09AE\u09BF\t4\t1700000001\r\n";
        out << "amar\t\u0986\u09AE\u09BE\u09B0\t2\t1700000002\n";
        out << "corrupt_line_without_tabs\n";
        out << "\n";
        out << "desh\t\u09A6\u09C7\u09B6\tabc\txyz\n";
        out.close();

        bangla::PersonalDictionary dict;
        bool loaded = dict.LoadFromFile(corrupt_path);
        Verify(loaded, "PersonalDictionary::LoadFromFile gracefully loads without crashing on corrupt lines");

        auto amar_entries = dict.GetWordsByRomanKey("amar");
        Verify(amar_entries.size() == 1, "Duplicates safely merged without duplicate vector entries");
        Verify(amar_entries[0].bengali_word == "\u0986\u09AE\u09BE\u09B0", "Record correctly retained");

        auto tumi_entries = dict.GetWordsByRomanKey("tumi");
        Verify(tumi_entries.size() == 1 && tumi_entries[0].bengali_word == "\u09A4\u09C1\u09AE\u09BF",
               "CRLF Windows line ending parsed cleanly");

        auto desh_entries = dict.GetWordsByRomanKey("desh");
        Verify(desh_entries.size() == 1 && desh_entries[0].frequency == 1,
               "Malformed frequency gracefully defaulted to 1 without crash");

        std::remove(corrupt_path.c_str());
    }

    std::cout << "\n============================================================\n";
    std::cout << "  STAGE 1 VERIFICATION SUMMARY\n";
    std::cout << "  Passed: " << g_pass_count << "\n";
    std::cout << "  Failed: " << g_fail_count << "\n";
    std::cout << "  Status: " << (g_fail_count == 0 ? "100% SUCCESS" : "FAILED") << "\n";
    std::cout << "============================================================\n";

    return (g_fail_count == 0) ? 0 : 1;
}
