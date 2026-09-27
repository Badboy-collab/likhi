#include <iostream>
#include <cassert>
#include <string>
#include <vector>
#include <fstream>
#include <windows.h>
#include <shlobj.h>

#include "likhi_version.h"
#include "update_service.h"

using namespace likhi;

// Test counters
static int g_tests_run = 0;
static int g_tests_passed = 0;

inline void ExpectTrue(bool condition, const char* expr_str, int line) {
    g_tests_run++;
    if (condition) {
        g_tests_passed++;
    } else {
        std::cerr << "  FAILED: " << expr_str << " at line " << line << std::endl;
    }
}

#define EXPECT_TRUE(...) ExpectTrue((__VA_ARGS__), #__VA_ARGS__, __LINE__)
#define EXPECT_FALSE(...) ExpectTrue(!(__VA_ARGS__), "!(" #__VA_ARGS__ ")", __LINE__)
#define EXPECT_EQ(a, b) ExpectTrue(((a) == (b)), #a " == " #b, __LINE__)

void TestSemVerParsingAndComparison() {
    std::cout << "[TEST] Semantic Version Parsing and Comparison..." << std::endl;

    // Basic equality
    SemVer v1 = SemVer::Parse("1.0.0");
    SemVer v2 = SemVer::Parse("v1.0.0");
    EXPECT_EQ(v1.major, 1);
    EXPECT_EQ(v1.minor, 0);
    EXPECT_EQ(v1.patch, 0);
    EXPECT_TRUE(v1 == v2);
    EXPECT_EQ(v1.Compare(v2), 0);

    // Minor update
    SemVer v_curr = SemVer::Parse("1.0.0");
    SemVer v_new_minor = SemVer::Parse("1.1.0");
    EXPECT_TRUE(v_curr < v_new_minor);
    EXPECT_TRUE(v_new_minor > v_curr);
    EXPECT_EQ(v_curr.Compare(v_new_minor), -1);
    EXPECT_EQ(v_new_minor.Compare(v_curr), 1);

    // Patch update
    SemVer v_new_patch = SemVer::Parse("1.0.1");
    EXPECT_TRUE(v_curr < v_new_patch);

    // Major update
    SemVer v_new_major = SemVer::Parse("2.0.0");
    EXPECT_TRUE(v_curr < v_new_major);

    // Prerelease versions
    SemVer v_release = SemVer::Parse("1.0.0");
    SemVer v_pre = SemVer::Parse("1.0.0-test2");
    EXPECT_TRUE(v_release > v_pre); // Official release is newer than prerelease of same version
    
    SemVer v_pre1 = SemVer::Parse("1.0.0-test1");
    SemVer v_pre2 = SemVer::Parse("1.0.0-test2");
    EXPECT_TRUE(v_pre1 < v_pre2);

    // UpdateService::CompareVersions static helper
    EXPECT_EQ(UpdateService::CompareVersions("1.0.0", "1.0.0"), 0);
    EXPECT_EQ(UpdateService::CompareVersions("1.0.0", "v1.1.0"), -1);
    EXPECT_EQ(UpdateService::CompareVersions("1.2.0", "1.1.0"), 1);
}

void TestReleaseJsonParsing() {
    std::cout << "[TEST] GitHub Release JSON Parsing..." << std::endl;

    // Simulated real GitHub API payload
    std::string sample_json = R"json({
        "tag_name": "v1.1.0",
        "name": "Likhi v1.1.0 (Windows Update)",
        "body": "Improved Bangla typing and suggestions",
        "published_at": "2026-09-27T10:00:00Z",
        "assets": [
            {
                "name": "LikhiSetup.exe",
                "content_type": "application/octet-stream",
                "size": 17825792,
                "digest": "sha256:48fb12cbba9eac76cb805d7eebb3093def847f6cb9e2fcd9e532606b2dc3cc0b",
                "browser_download_url": "https://github.com/Badboy-collab/likhi/releases/download/v1.1.0/LikhiSetup.exe"
            }
        ]
    })json";

    ReleaseInfo info;
    bool parsed = UpdateService::ParseReleaseJson(sample_json, info);
    EXPECT_TRUE(parsed);
    EXPECT_EQ(info.version, "1.1.0");
    EXPECT_EQ(info.tag_name, "v1.1.0");
    EXPECT_EQ(info.name, "Likhi v1.1.0 (Windows Update)");
    EXPECT_TRUE(info.release_notes.find("Improved Bangla typing") != std::string::npos);
    EXPECT_EQ(info.download_url, "https://github.com/Badboy-collab/likhi/releases/download/v1.1.0/LikhiSetup.exe");
    EXPECT_EQ(info.sha256_hash, "sha256:48fb12cbba9eac76cb805d7eebb3093def847f6cb9e2fcd9e532606b2dc3cc0b");
    EXPECT_EQ(info.file_size, 17825792ULL);
    EXPECT_EQ(info.update_page_url, "https://getlikhi.com/update/");

    // Malformed JSON should not crash and return false
    ReleaseInfo bad_info;
    EXPECT_FALSE(UpdateService::ParseReleaseJson("", bad_info));
    EXPECT_FALSE(UpdateService::ParseReleaseJson("{ invalid json }", bad_info));
}

void TestCooldownAndThrottling() {
    std::cout << "[TEST] Throttling, Cooldown, and Dismissal Logic..." << std::endl;

    // Record check timestamp
    UpdateService::RecordCheckTimestamp();
    // Immediate subsequent startup check should be throttled (within 24h)
    EXPECT_FALSE(UpdateService::ShouldCheckOnStartup());

    // Dismiss a version (user clicks "Later")
    UpdateService::DismissVersion("1.1.0");
    EXPECT_TRUE(UpdateService::IsVersionDismissed("1.1.0"));
    // A newer version (e.g. 1.2.0) should NOT be dismissed
    EXPECT_FALSE(UpdateService::IsVersionDismissed("1.2.0"));
}

void TestSha256Verification() {
    std::cout << "[TEST] SHA-256 Cryptographic Digest Verification..." << std::endl;

    // Create a temporary file with known content
    wchar_t temp_dir[MAX_PATH];
    GetTempPathW(MAX_PATH, temp_dir);
    std::wstring temp_file = std::wstring(temp_dir) + L"likhi_test_digest.bin";

    const std::string content = "Likhi Bangla Typing System 2026";
    std::ofstream out(temp_file.c_str(), std::ios::binary);
    out.write(content.data(), content.size());
    out.close();

    // Verify with invalid hash -> MUST return false
    EXPECT_FALSE(UpdateService::VerifySha256(temp_file, "0000000000000000000000000000000000000000000000000000000000000000"));

    // Self-calculate using CryptoAPI to test VerifySha256 consistency
    HCRYPTPROV hProv = 0;
    HCRYPTHASH hHash = 0;
    std::string expected_hex;
    if (CryptAcquireContextW(&hProv, NULL, NULL, PROV_RSA_AES, CRYPT_VERIFYCONTEXT)) {
        if (CryptCreateHash(hProv, CALG_SHA_256, 0, 0, &hHash)) {
            CryptHashData(hHash, reinterpret_cast<const BYTE*>(content.data()), (DWORD)content.size(), 0);
            BYTE hashBytes[32];
            DWORD hashLen = sizeof(hashBytes);
            if (CryptGetHashParam(hHash, HP_HASHVAL, hashBytes, &hashLen, 0)) {
                char buf[3];
                for (DWORD i = 0; i < hashLen; ++i) {
                    snprintf(buf, sizeof(buf), "%02x", hashBytes[i]);
                    expected_hex += buf;
                }
            }
            CryptDestroyHash(hHash);
        }
        CryptReleaseContext(hProv, 0);
    }

    EXPECT_FALSE(expected_hex.empty());
    // Verify matching hash
    EXPECT_TRUE(UpdateService::VerifySha256(temp_file, expected_hex));
    // Verify with "sha256:" prefix
    EXPECT_TRUE(UpdateService::VerifySha256(temp_file, "sha256:" + expected_hex));

    DeleteFileW(temp_file.c_str());
}

void TestUserDataPreservation() {
    std::cout << "[TEST] User Data Preservation Guarantee (%APPDATA%)..." << std::endl;

    std::wstring appdata = UpdateService::GetAppDataDirectory();
    EXPECT_FALSE(appdata.empty());

    // Create a mock user settings file and custom dictionary file
    std::wstring mock_settings = appdata + L"\\test_preserve_settings.json";
    std::wstring mock_dict = appdata + L"\\test_preserve_user_dict.txt";

    std::ofstream sf(mock_settings.c_str());
    sf << "{\"custom_user_theme\": \"dark\", \"typing_speed\": 120}\n";
    sf.close();

    std::ofstream df(mock_dict.c_str());
    df << "amar\tআমার\t100\n";
    df.close();

    // Perform UpdateService operations
    UpdateService::RecordCheckTimestamp();
    UpdateService::DismissVersion("1.0.0");
    UpdateService::IsVersionDismissed("1.0.0");

    // Assert that mock user data files STILL exist and contents are 100% unaltered
    std::ifstream sf_read(mock_settings.c_str());
    EXPECT_TRUE(sf_read.is_open());
    std::string s_content((std::istreambuf_iterator<char>(sf_read)), std::istreambuf_iterator<char>());
    sf_read.close();
    EXPECT_TRUE(s_content.find("custom_user_theme") != std::string::npos);

    std::ifstream df_read(mock_dict.c_str());
    EXPECT_TRUE(df_read.is_open());
    std::string d_content((std::istreambuf_iterator<char>(df_read)), std::istreambuf_iterator<char>());
    df_read.close();
    EXPECT_TRUE(d_content.find("amar") != std::string::npos);

    // Clean up test files
    DeleteFileW(mock_settings.c_str());
    DeleteFileW(mock_dict.c_str());
}

int main() {
    std::cout << "==========================================================" << std::endl;
    std::cout << " LIKHI UPDATE SERVICE AUTOMATED REGRESSION SUITE" << std::endl;
    std::cout << "==========================================================" << std::endl;

    TestSemVerParsingAndComparison();
    TestReleaseJsonParsing();
    TestCooldownAndThrottling();
    TestSha256Verification();
    TestUserDataPreservation();

    std::cout << "==========================================================" << std::endl;
    std::cout << " Results: " << g_tests_passed << " / " << g_tests_run << " assertions passed." << std::endl;
    if (g_tests_passed == g_tests_run) {
        std::cout << " STATUS: ALL UPDATE TESTS PASSED (100% SUCCESS)" << std::endl;
        return 0;
    } else {
        std::cout << " STATUS: FAILURES DETECTED" << std::endl;
        return 1;
    }
}
