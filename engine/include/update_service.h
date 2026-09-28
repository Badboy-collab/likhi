#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include "likhi_version.h"

namespace likhi {

struct ReleaseInfo {
    std::string version;             // e.g. "1.1.0"
    std::string tag_name;            // e.g. "v1.1.0"
    std::string name;                // e.g. "Likhi v1.1.0 (Windows)"
    std::string release_notes;       // e.g. "• Improved Bangla typing\n• Bug fixes"
    std::string download_url;        // Asset download URL (e.g. LikhiSetup.exe)
    std::string update_page_url;     // Official GetLikhi.com update portal
    std::string sha256_hash;         // SHA-256 digest if available
    uint64_t file_size = 0;          // Asset byte size
    std::string published_at;        // ISO 8601 timestamp
    bool is_prerelease = false;      // True if GitHub marked prerelease or SemVer has prerelease suffix
    bool is_draft = false;           // True if marked draft
};

enum class UpdateCheckResult {
    kUpToDate,
    kUpdateAvailable,
    kNetworkOffline,
    kError
};

class UpdateService {
public:
    // Cooldown duration for automated startup checks (24 hours in seconds)
    static constexpr uint64_t kStartupCheckIntervalSeconds = 24 * 60 * 60;
    
    // Snooze duration if user chooses "Later" (7 days in seconds)
    static constexpr uint64_t kDismissSnoozeSeconds = 7 * 24 * 60 * 60;

    // Get current version
    static std::string GetCurrentVersion();
    static std::wstring GetCurrentVersionW();

    // Check for updates online (calls GitHub API via WinHTTP)
    // If force_bypass_cooldown is true, ignores 24-hour cache limit (used for manual Check Updates button)
    static UpdateCheckResult CheckForUpdate(ReleaseInfo& out_info, bool force_bypass_cooldown = false);

    // Determines if an automated startup check should run based on last check timestamp
    static bool ShouldCheckOnStartup();

    // Updates cache timestamp and dismissed version
    static void RecordCheckTimestamp();
    static std::wstring GetLastCheckTimeString();
    static void DismissVersion(const std::string& version);
    static bool IsVersionDismissed(const std::string& version);

    // Version comparison: returns -1 if current < remote, 0 if equal, 1 if current > remote
    static int CompareVersions(const std::string& current, const std::string& remote);

    // JSON Parser for GitHub Releases API payload (robust, zero-dependency)
    // Handles both single release object and array of releases.
    // When allow_prereleases is false, drafts and prereleases are strictly excluded.
    static bool ParseReleaseJson(const std::string& json_str, ReleaseInfo& out_info, bool allow_prereleases = false);

    // Opens the official GetLikhi.com update page in the default system browser
    static bool LaunchOfficialUpdateFlow(const std::string& custom_url = "");

    // Verifies SHA-256 hash of a file against expected hex string using Win32 CryptoAPI
    static bool VerifySha256(const std::wstring& file_path, const std::string& expected_hex);

    // Helpers
    static std::wstring GetAppDataDirectory();
    static std::wstring GetUpdateCachePath();
};

} // namespace likhi
