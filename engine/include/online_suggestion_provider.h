#ifndef ONLINE_SUGGESTION_PROVIDER_H
#define ONLINE_SUGGESTION_PROVIDER_H

#include <string>
#include <vector>
#include <cstdint>

namespace bangla {

// Pure virtual interface for online suggestion providers.
// Allows mock implementations for deterministic unit testing
// and live implementations (e.g. WinHTTP / Google Input Tools).
class IOnlineSuggestionProvider {
public:
    virtual ~IOnlineSuggestionProvider() = default;

    // Queries suggestions for a single roman word token.
    //
    // Privacy & Performance Invariants:
    // - Only single-word roman tokens are allowed (never full sentences, passwords, or numbers).
    // - Must strictly respect timeout_ms. If timeout or offline, returns false immediately.
    // - Never blocks local typing or UI.
    virtual bool QuerySuggestions(const std::string& roman_word,
                                  std::vector<std::string>& out_candidates,
                                  uint32_t timeout_ms = 150) = 0;

    // Returns true if the provider is currently online and active.
    virtual bool IsAvailable() const = 0;

    // Returns a human-readable name of this provider.
    virtual const char* GetProviderName() const = 0;
};

} // namespace bangla

#endif // ONLINE_SUGGESTION_PROVIDER_H
