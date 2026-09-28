#ifndef HYBRID_BRAIN_H
#define HYBRID_BRAIN_H

#include "../../include/bangla_engine.h"
#include "../../include/online_suggestion_provider.h"
#include "../unicode/bangla_unicode.h"

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <chrono>

namespace bangla {

struct HybridCandidate {
    std::string bengali_text;
    std::string roman_origin;
    float score = 0.0f;
    uint32_t flags = 0;
    bool from_local = false;
    bool from_online = false;
    bool agreement_bonus = false;
};

// 4-Tier Knowledge Separation & Hybrid Intelligence Coordinator
class HybridBrain {
public:
    HybridBrain(BanglaEngine* local_engine,
                std::shared_ptr<IOnlineSuggestionProvider> online_provider = nullptr);
    ~HybridBrain();

    // Enable or disable online suggestions enhancement
    void SetOnlineEnabled(bool enabled);
    bool IsOnlineEnabled() const;

    // Set online suggestion provider
    void SetOnlineProvider(std::shared_ptr<IOnlineSuggestionProvider> provider);

    // Primary hybrid query: combines local brain with optional online suggestions
    // 1. Queries local engine (0ms, primary)
    // 2. Checks transient online cache (Tier 4) or optionally queries online provider
    // 3. Validates and canonicalizes all candidates
    // 4. Applies Agreement Bonus (+0.15f) for local & online consensus
    // 5. Deduplicates and orders by final score
    std::vector<HybridCandidate> GetHybridSuggestions(const std::string& roman_token,
                                                      size_t max_candidates = 5);

    // Feedback Loop: User selects or commits a candidate.
    // If the candidate came from online (Tier 4), it is now explicitly confirmed
    // by the user and committed to the local engine / personal learning (Tier 2),
    // ensuring "Learn when online, remain useful when offline".
    void OnUserSelect(const std::string& roman_token, const std::string& selected_bengali);

    // Hybrid Validator: verifies candidate contains valid Bengali Unicode,
    // canonicalizes it, and filters out malformed strings.
    static bool ValidateCandidate(const std::string& candidate, std::string& out_canonical);

    // Privacy Guard: ensures no passwords, numbers, or full sentences are sent online
    static bool IsPrivacySafe(const std::string& token);

    // Tier 4 Online Cache stats
    size_t GetCacheSize() const;
    void ClearCache();

private:
    BanglaEngine* local_engine_ = nullptr;
    std::shared_ptr<IOnlineSuggestionProvider> online_provider_;
    bool online_enabled_ = true;

    // Tier 4: Transient Online Suggestion Cache (isolated from local dictionary)
    struct CacheEntry {
        std::vector<std::string> candidates;
        std::chrono::steady_clock::time_point timestamp;
    };
    mutable std::mutex cache_mu_;
    std::unordered_map<std::string, CacheEntry> online_cache_;
    static constexpr size_t kMaxCacheEntries = 256;
};

} // namespace bangla

#endif // HYBRID_BRAIN_H
