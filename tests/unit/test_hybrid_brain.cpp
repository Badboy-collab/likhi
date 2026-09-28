#include "bangla_engine.h"
#include "../../engine/src/hybrid/hybrid_brain.h"
#include <iostream>
#include <vector>
#include <string>
#include <cassert>
#include <thread>
#include <chrono>

#ifdef _WIN32
#include <windows.h>
#endif

void SetupConsole() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
}

// Mock Online Suggestion Provider for testing
class MockOnlineProvider : public bangla::IOnlineSuggestionProvider {
public:
    bool is_available = true;
    bool should_timeout = false;
    std::vector<std::string> mock_suggestions;

    bool QuerySuggestions(const std::string& /*roman_word*/,
                          std::vector<std::string>& out_candidates,
                          uint32_t /*timeout_ms*/) override {
        if (!is_available) return false;
        if (should_timeout) {
            // Simulate timeout
            return false;
        }
        out_candidates = mock_suggestions;
        return !mock_suggestions.empty();
    }

    bool IsAvailable() const override {
        return is_available;
    }

    const char* GetProviderName() const override {
        return "MockOnlineProvider";
    }
};

int main() {
    SetupConsole();
    std::cout << "======================================================================\n";
    std::cout << " LIKHI HYBRID SUGGESTION INTELLIGENCE TEST SUITE\n";
    std::cout << "======================================================================\n\n";

    EngineConfig cfg;
    BanglaEngine_GetDefaultConfig(&cfg);
    cfg.lexicon_binary_path = "engine/data/lexicon.bin";
    BanglaEngine* engine = BanglaEngine_Create(&cfg);
    if (!engine) {
        std::cerr << "[CRITICAL] Failed to create BanglaEngine!\n";
        return 1;
    }

    auto mock_provider = std::make_shared<MockOnlineProvider>();
    bangla::HybridBrain hybrid_brain(engine, mock_provider);

    int passed = 0;
    int failed = 0;

    auto CHECK = [&](bool condition, const std::string& desc) {
        if (condition) {
            std::cout << "  [PASS] " << desc << "\n";
            passed++;
        } else {
            std::cout << "  [FAIL] " << desc << "\n";
            failed++;
        }
    };

    // --- Scenario 1: Local Brain Primary (Zero Online, 100% Offline) ---
    std::cout << "Scenario 1: Local Brain Primary\n";
    hybrid_brain.SetOnlineEnabled(false);
    auto cands1 = hybrid_brain.GetHybridSuggestions("dhaka", 5);
    CHECK(!cands1.empty(), "Local engine returns candidates without online");
    CHECK(cands1[0].bengali_text == "ঢাকা", "Rank 1 is 'ঢাকা'");
    CHECK(cands1[0].from_local == true, "Candidate flagged as from_local");
    CHECK(cands1[0].from_online == false, "Candidate not flagged as from_online");
    CHECK(cands1[0].agreement_bonus == false, "No agreement bonus when online is disabled");

    // --- Scenario 2: Graceful Offline Fallback & Timeouts ---
    std::cout << "\nScenario 2: Graceful Offline Fallback & Timeouts\n";
    hybrid_brain.SetOnlineEnabled(true);
    mock_provider->is_available = false; // network down
    auto cands2 = hybrid_brain.GetHybridSuggestions("dhaka", 5);
    CHECK(!cands2.empty(), "Offline fallback: returns local candidates when network is down");
    CHECK(cands2[0].bengali_text == "ঢাকা", "Offline fallback maintains top candidate 'ঢাকা'");

    mock_provider->is_available = true;
    mock_provider->should_timeout = true; // network timeout
    auto cands2_timeout = hybrid_brain.GetHybridSuggestions("dhaka", 5);
    CHECK(!cands2_timeout.empty(), "Timeout fallback: returns local candidates on timeout");
    CHECK(cands2_timeout[0].bengali_text == "ঢাকা", "Timeout fallback maintains top candidate");

    // --- Scenario 3: Agreement Bonus (+0.15f Consensus Boost) ---
    std::cout << "\nScenario 3: Agreement Bonus Consensus Boost\n";
    mock_provider->should_timeout = false;
    mock_provider->mock_suggestions = {"ঢাকা", "ঢাকায়"}; // Agree on ঢাকা
    hybrid_brain.ClearCache();

    // Baseline score without agreement
    hybrid_brain.SetOnlineEnabled(false);
    auto base_cands = hybrid_brain.GetHybridSuggestions("dhaka", 5);
    float base_score = base_cands[0].score;

    hybrid_brain.SetOnlineEnabled(true);
    auto cands3 = hybrid_brain.GetHybridSuggestions("dhaka", 5);
    CHECK(cands3[0].bengali_text == "ঢাকা", "Top candidate is 'ঢাকা'");
    CHECK(cands3[0].agreement_bonus == true, "Agreement bonus flag is TRUE");
    CHECK(cands3[0].score >= base_score, "Candidate score boosted by agreement");
    CHECK(cands3[0].from_local && cands3[0].from_online, "Consensus from both local and online");

    // --- Scenario 4: Hybrid Validation & Canonicalization ---
    std::cout << "\nScenario 4: Hybrid Validation & Canonicalization\n";
    std::string canon_out;
    CHECK(bangla::HybridBrain::ValidateCandidate("বাংলা", canon_out) == true, "Valid Bengali text accepted");
    CHECK(bangla::HybridBrain::ValidateCandidate("", canon_out) == false, "Empty string rejected");
    std::string control_junk = "টেস্ট\x01\x02";
    CHECK(bangla::HybridBrain::ValidateCandidate(control_junk, canon_out) == false, "Control characters rejected");

    // --- Scenario 5: Privacy Guard Invariants ---
    std::cout << "\nScenario 5: Privacy Guard Invariants\n";
    CHECK(bangla::HybridBrain::IsPrivacySafe("desh") == true, "Plain roman word is safe");
    CHECK(bangla::HybridBrain::IsPrivacySafe("Google-er") == true, "Hyphenated token is safe");
    CHECK(bangla::HybridBrain::IsPrivacySafe("password123") == false, "Alphanumeric password rejected");
    CHECK(bangla::HybridBrain::IsPrivacySafe("user@example.com") == false, "Email address rejected");
    CHECK(bangla::HybridBrain::IsPrivacySafe("amar sonar bangla") == false, "Full sentence rejected");
    CHECK(bangla::HybridBrain::IsPrivacySafe("1234567890123456") == false, "Card number rejected");

    // --- Scenario 6: 4-Tier Knowledge Separation & Learning Loop ---
    std::cout << "\nScenario 6: 4-Tier Knowledge Separation & Learning Loop\n";
    mock_provider->mock_suggestions = {"নতুনশব্দ"}; // Online-only discovery candidate
    hybrid_brain.ClearCache();
    auto cands6 = hybrid_brain.GetHybridSuggestions("notunshobdo", 5);
    CHECK(hybrid_brain.GetCacheSize() == 1, "Tier 4 cache populated with 1 entry");

    // User explicitly selects the online candidate
    hybrid_brain.OnUserSelect("notunshobdo", "নতুনশব্দ");

    // Next time in offline mode, the learned word is present!
    hybrid_brain.SetOnlineEnabled(false);
    auto cands6_offline = hybrid_brain.GetHybridSuggestions("notunshobdo", 5);
    bool found_offline = false;
    for (const auto& c : cands6_offline) {
        if (c.bengali_text == "নতুনশব্দ") { found_offline = true; break; }
    }
    CHECK(found_offline, "User selection feedback loop: online word successfully learned into offline brain!");

    BanglaEngine_Destroy(engine);

    std::cout << "\n======================================================================\n";
    std::cout << " Hybrid Intelligence Results: " << passed << " passed, " << failed << " failed.\n";
    std::cout << "======================================================================\n";

    return (failed == 0) ? 0 : 1;
}
