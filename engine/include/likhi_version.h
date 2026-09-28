#pragma once

#include <string>
#include <vector>
#include <sstream>
#include <cstdint>

namespace likhi {

// ============================================================================
// Authoritative Application Version Constants
// ============================================================================
constexpr int kVersionMajor = 1;
constexpr int kVersionMinor = 0;
constexpr int kVersionPatch = 1;

inline const char* const kVersionString = "1.0.1";
inline const wchar_t* const kVersionWString = L"1.0.1";

inline const char* const kAppName = "Likhi";
inline const wchar_t* const kAppNameW = L"Likhi";
inline const wchar_t* const kAppNameBn = L"লিখি";

// Official URLs
inline const char* const kOfficialWebsiteUrl = "https://getlikhi.com/";
inline const char* const kOfficialUpdateUrl = "https://getlikhi.com/update/";
inline const char* const kOfficialDownloadUrl = "https://getlikhi.com/download/";
inline const char* const kGitHubRepoUrl = "https://github.com/Badboy-collab/likhi";
inline const char* const kGitHubApiReleasesUrl = "https://api.github.com/repos/Badboy-collab/likhi/releases/latest";

// ============================================================================
// Semantic Versioning Helper
// ============================================================================
struct SemVer {
    int major = 0;
    int minor = 0;
    int patch = 0;
    std::string prerelease;

    static SemVer Parse(const std::string& v_str) {
        SemVer sv;
        if (v_str.empty()) return sv;

        // Skip leading 'v' or 'V'
        size_t start = 0;
        if (v_str[0] == 'v' || v_str[0] == 'V') start = 1;

        std::string core = v_str.substr(start);
        size_t dash = core.find('-');
        if (dash != std::string::npos) {
            sv.prerelease = core.substr(dash + 1);
            core = core.substr(0, dash);
        }

        std::stringstream ss(core);
        std::string part;
        if (std::getline(ss, part, '.')) {
            try { sv.major = std::stoi(part); } catch (...) { sv.major = 0; }
        }
        if (std::getline(ss, part, '.')) {
            try { sv.minor = std::stoi(part); } catch (...) { sv.minor = 0; }
        }
        if (std::getline(ss, part, '.')) {
            try { sv.patch = std::stoi(part); } catch (...) { sv.patch = 0; }
        }
        return sv;
    }

    // Returns:
    //  -1 if this < other
    //   0 if this == other
    //  +1 if this > other
    int Compare(const SemVer& other) const {
        if (major != other.major) return major < other.major ? -1 : 1;
        if (minor != other.minor) return minor < other.minor ? -1 : 1;
        if (patch != other.patch) return patch < other.patch ? -1 : 1;

        // A version without prerelease tag is strictly greater than one with prerelease tag
        // e.g. 1.0.0 > 1.0.0-test2
        if (prerelease.empty() && !other.prerelease.empty()) return 1;
        if (!prerelease.empty() && other.prerelease.empty()) return -1;
        if (!prerelease.empty() && !other.prerelease.empty()) {
            if (prerelease != other.prerelease) {
                return prerelease < other.prerelease ? -1 : 1;
            }
        }
        return 0;
    }

    bool operator<(const SemVer& other) const { return Compare(other) < 0; }
    bool operator<=(const SemVer& other) const { return Compare(other) <= 0; }
    bool operator>(const SemVer& other) const { return Compare(other) > 0; }
    bool operator>=(const SemVer& other) const { return Compare(other) >= 0; }
    bool operator==(const SemVer& other) const { return Compare(other) == 0; }
    bool operator!=(const SemVer& other) const { return Compare(other) != 0; }
};

} // namespace likhi
