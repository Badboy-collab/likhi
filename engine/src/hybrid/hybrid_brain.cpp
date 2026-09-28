#include "hybrid_brain.h"
#include <algorithm>
#include <cctype>

namespace bangla {

HybridBrain::HybridBrain(BanglaEngine* local_engine,
                         std::shared_ptr<IOnlineSuggestionProvider> online_provider)
    : local_engine_(local_engine), online_provider_(online_provider) {}

HybridBrain::~HybridBrain() = default;

void HybridBrain::SetOnlineEnabled(bool enabled) {
    online_enabled_ = enabled;
}

bool HybridBrain::IsOnlineEnabled() const {
    return online_enabled_;
}

void HybridBrain::SetOnlineProvider(std::shared_ptr<IOnlineSuggestionProvider> provider) {
    online_provider_ = provider;
}

size_t HybridBrain::GetCacheSize() const {
    std::lock_guard<std::mutex> lock(cache_mu_);
    return online_cache_.size();
}

void HybridBrain::ClearCache() {
    std::lock_guard<std::mutex> lock(cache_mu_);
    online_cache_.clear();
}

bool HybridBrain::IsPrivacySafe(const std::string& token) {
    if (token.empty() || token.size() > 32) return false;
    for (char c : token) {
        // Only allow ASCII letters and hyphen for morphology
        if (!std::isalpha(static_cast<unsigned char>(c)) && c != '-') {
            return false; // Rejects numbers, symbols, spaces, passwords, emails
        }
    }
    return true;
}

bool HybridBrain::ValidateCandidate(const std::string& candidate, std::string& out_canonical) {
    if (candidate.empty()) return false;

    // Check for control characters
    for (unsigned char c : candidate) {
        if (c < 0x20 && c != '\t') return false;
    }

    // Canonicalize Bengali Unicode (U+09F0 -> U+09B0, decomposed nuktas, etc.)
    out_canonical = UnicodeUtils::CanonicalizeBengali(candidate);
    return !out_canonical.empty();
}

std::vector<HybridCandidate> HybridBrain::GetHybridSuggestions(const std::string& roman_token,
                                                              size_t max_candidates) {
    std::vector<HybridCandidate> results;
    if (roman_token.empty() || !local_engine_) return results;

    // Step 1: Local Likhi Brain (Primary, 0ms, 100% offline-first)
    BanglaEngine_SetComposition(local_engine_, roman_token.c_str());
    CandidateList local_list;
    BanglaEngine_GetCandidates(local_engine_, &local_list);

    // Map to track canonical text -> HybridCandidate
    std::vector<HybridCandidate> combined;
    std::unordered_map<std::string, size_t> text_to_index;

    for (uint32_t i = 0; i < local_list.count; ++i) {
        std::string canon;
        if (!ValidateCandidate(local_list.candidates[i].bengali_text, canon)) continue;

        HybridCandidate hc;
        hc.bengali_text = canon;
        hc.roman_origin = local_list.candidates[i].roman_origin;
        hc.score = local_list.candidates[i].score;
        hc.flags = local_list.candidates[i].category_flags;
        hc.from_local = true;
        hc.from_online = false;
        hc.agreement_bonus = false;

        text_to_index[canon] = combined.size();
        combined.push_back(hc);
    }

    // Step 2: Optional Online Suggestion Enhancement (Tier 4 Cache / Query)
    std::vector<std::string> online_candidates;
    bool had_online = false;

    if (online_enabled_ && IsPrivacySafe(roman_token)) {
        // Check Tier 4 Cache first
        {
            std::lock_guard<std::mutex> lock(cache_mu_);
            auto it = online_cache_.find(roman_token);
            if (it != online_cache_.end()) {
                online_candidates = it->second.candidates;
                had_online = true;
            }
        }

        // If not in cache and provider is active, query provider
        if (!had_online && online_provider_ && online_provider_->IsAvailable()) {
            std::vector<std::string> fetched;
            if (online_provider_->QuerySuggestions(roman_token, fetched, 150)) {
                online_candidates = fetched;
                had_online = true;

                // Store in Tier 4 Cache (LRU bounded)
                std::lock_guard<std::mutex> lock(cache_mu_);
                if (online_cache_.size() >= kMaxCacheEntries) {
                    online_cache_.erase(online_cache_.begin());
                }
                online_cache_[roman_token] = {fetched, std::chrono::steady_clock::now()};
            }
        }
    }

    // Step 3: Hybrid Consensus & Agreement Bonus (+0.15f)
    if (had_online) {
        float online_base_score = 0.88f;
        for (const auto& raw_online : online_candidates) {
            std::string canon;
            if (!ValidateCandidate(raw_online, canon)) continue;

            auto it = text_to_index.find(canon);
            if (it != text_to_index.end()) {
                // AGREEMENT BONUS: Candidate agreed upon by BOTH Local Brain & Online Provider!
                combined[it->second].from_online = true;
                combined[it->second].agreement_bonus = true;
                combined[it->second].score += 0.15f; // Agreement Boost
                if (combined[it->second].score > 1.0f) combined[it->second].score = 1.0f;
            } else {
                // Online discovery candidate (Tier 4)
                HybridCandidate hc;
                hc.bengali_text = canon;
                hc.roman_origin = roman_token;
                hc.score = online_base_score;
                hc.flags = 0;
                hc.from_local = false;
                hc.from_online = true;
                hc.agreement_bonus = false;

                text_to_index[canon] = combined.size();
                combined.push_back(hc);
                online_base_score = std::max(0.15f, online_base_score - 0.05f);
            }
        }
    }

    // Step 4: Sort by score descending (preserving primary flags)
    std::stable_sort(combined.begin(), combined.end(),
                     [](const HybridCandidate& a, const HybridCandidate& b) {
                         if ((a.flags & CANDIDATE_FLAG_PRIMARY) != (b.flags & CANDIDATE_FLAG_PRIMARY)) {
                             return (a.flags & CANDIDATE_FLAG_PRIMARY) > (b.flags & CANDIDATE_FLAG_PRIMARY);
                         }
                         return a.score > b.score;
                     });

    // Step 5: Trim to max_candidates
    if (combined.size() > max_candidates) {
        combined.resize(max_candidates);
    }

    return combined;
}

void HybridBrain::OnUserSelect(const std::string& roman_token, const std::string& selected_bengali) {
    if (selected_bengali.empty() || !local_engine_) return;

    // Feedback Loop:
    // Once user confirms/commits the candidate, it transitions into
    // the local brain. The local engine records the word as confirmed,
    // so this word is available offline next time!
    BanglaEngine_CommitWord(local_engine_, selected_bengali.c_str());
    BanglaEngine_AddUserWord(local_engine_, roman_token.c_str(), selected_bengali.c_str());
    BanglaEngine_LearnWord(local_engine_, roman_token.c_str(), selected_bengali.c_str());
}

} // namespace bangla
