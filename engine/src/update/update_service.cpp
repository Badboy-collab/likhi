#include "update_service.h"

#include <windows.h>
#include <winhttp.h>
#include <wincrypt.h>
#include <shlobj.h>
#include <shellapi.h>

#include <chrono>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace likhi {

namespace {

// JSON helper: extracts the string value for a given key: "key": "value"
bool ExtractJsonString(const std::string& json, const std::string& key, std::string& out_value, size_t start_pos = 0, size_t* found_pos = nullptr) {
    std::string pattern = "\"" + key + "\"";
    size_t pos = json.find(pattern, start_pos);
    if (pos == std::string::npos) return false;

    size_t colon = json.find(':', pos + pattern.size());
    if (colon == std::string::npos) return false;

    // Find opening quote
    size_t quote_start = json.find('"', colon + 1);
    if (quote_start == std::string::npos) return false;

    // Find matching closing quote, respecting backslash escapes
    std::string val;
    size_t i = quote_start + 1;
    while (i < json.size()) {
        char c = json[i];
        if (c == '"') {
            out_value = val;
            if (found_pos) *found_pos = i + 1;
            return true;
        }
        if (c == '\\' && i + 1 < json.size()) {
            char next = json[i + 1];
            switch (next) {
                case '"':  val += '"'; break;
                case '\\': val += '\\'; break;
                case '/':  val += '/'; break;
                case 'n':  val += '\n'; break;
                case 'r':  val += '\r'; break;
                case 't':  val += '\t'; break;
                default:   val += next; break;
            }
            i += 2;
            continue;
        }
        val += c;
        ++i;
    }
    return false;
}

// JSON helper: extracts numeric value for a given key
bool ExtractJsonNumber(const std::string& json, const std::string& key, uint64_t& out_value, size_t start_pos = 0) {
    std::string pattern = "\"" + key + "\"";
    size_t pos = json.find(pattern, start_pos);
    if (pos == std::string::npos) return false;

    size_t colon = json.find(':', pos + pattern.size());
    if (colon == std::string::npos) return false;

    size_t num_start = json.find_first_of("0123456789", colon + 1);
    if (num_start == std::string::npos) return false;

    size_t num_end = json.find_first_not_of("0123456789", num_start);
    std::string num_str = json.substr(num_start, num_end == std::string::npos ? std::string::npos : num_end - num_start);
    try {
        out_value = std::stoull(num_str);
        return true;
    } catch (...) {
        return false;
    }
}

// JSON helper: extracts boolean value for a given key
bool ExtractJsonBool(const std::string& json, const std::string& key, bool& out_value, size_t start_pos = 0) {
    std::string pattern = "\"" + key + "\"";
    size_t pos = json.find(pattern, start_pos);
    if (pos == std::string::npos) return false;

    size_t colon = json.find(':', pos + pattern.size());
    if (colon == std::string::npos) return false;

    size_t val_pos = json.find_first_not_of(" \t\r\n", colon + 1);
    if (val_pos == std::string::npos) return false;

    if (json.compare(val_pos, 4, "true") == 0) {
        out_value = true;
        return true;
    }
    if (json.compare(val_pos, 5, "false") == 0) {
        out_value = false;
        return true;
    }
    return false;
}

uint64_t CurrentEpochSeconds() {
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());
}

std::wstring Utf8ToWide(const std::string& utf8) {
    if (utf8.empty()) return std::wstring();
    int len = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(), nullptr, 0);
    std::wstring wide(len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(), &wide[0], len);
    return wide;
}

// Helper to parse a single JSON release object
bool ParseSingleReleaseObject(const std::string& obj_str, ReleaseInfo& out_info) {
    if (obj_str.empty()) return false;

    if (!ExtractJsonString(obj_str, "tag_name", out_info.tag_name)) {
        return false;
    }

    out_info.version = out_info.tag_name;
    if (!out_info.version.empty() && (out_info.version[0] == 'v' || out_info.version[0] == 'V')) {
        out_info.version = out_info.version.substr(1);
    }

    bool gh_prerelease = false;
    if (ExtractJsonBool(obj_str, "prerelease", gh_prerelease)) {
        out_info.is_prerelease = gh_prerelease;
    }
    bool gh_draft = false;
    if (ExtractJsonBool(obj_str, "draft", gh_draft)) {
        out_info.is_draft = gh_draft;
    }

    SemVer sv = SemVer::Parse(out_info.version);
    if (!sv.prerelease.empty()) {
        out_info.is_prerelease = true; // e.g. tag is v1.0.0-test2, -beta, -rc
    }

    ExtractJsonString(obj_str, "name", out_info.name);
    if (out_info.name.empty()) out_info.name = "Likhi " + out_info.tag_name;

    ExtractJsonString(obj_str, "body", out_info.release_notes);
    ExtractJsonString(obj_str, "published_at", out_info.published_at);
    out_info.update_page_url = kOfficialUpdateUrl;

    // Assets inspection
    size_t assets_pos = obj_str.find("\"assets\"");
    if (assets_pos != std::string::npos) {
        size_t search_pos = assets_pos;
        std::string asset_name, dl_url, digest;
        uint64_t size_bytes = 0;
        
        while (ExtractJsonString(obj_str, "name", asset_name, search_pos, &search_pos)) {
            if (asset_name.size() > 4 && asset_name.substr(asset_name.size() - 4) == ".exe") {
                ExtractJsonString(obj_str, "browser_download_url", dl_url, search_pos);
                ExtractJsonString(obj_str, "digest", digest, search_pos);
                ExtractJsonNumber(obj_str, "size", size_bytes, search_pos);

                out_info.download_url = dl_url;
                out_info.sha256_hash = digest;
                out_info.file_size = size_bytes;

                if (asset_name == "LikhiSetup.exe" || asset_name.find("Likhi_Setup") != std::string::npos) {
                    break;
                }
            }
        }
    }

    if (out_info.download_url.empty()) {
        out_info.download_url = kOfficialUpdateUrl;
    }

    return true;
}

// WinHTTP GET helper with status code and body extraction
bool FetchUrl(HINTERNET h_connect, const wchar_t* path, DWORD& out_status_code, std::string& out_body) {
    HINTERNET h_request = WinHttpOpenRequest(h_connect, L"GET",
                                            path,
                                            NULL, WINHTTP_NO_REFERER,
                                            WINHTTP_DEFAULT_ACCEPT_TYPES,
                                            WINHTTP_FLAG_SECURE);
    if (!h_request) return false;

    const wchar_t* headers = L"Accept: application/vnd.github.v3+json\r\n";
    BOOL sent = WinHttpSendRequest(h_request, headers, (DWORD)-1L, WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
    if (!sent || !WinHttpReceiveResponse(h_request, NULL)) {
        WinHttpCloseHandle(h_request);
        return false;
    }

    DWORD status_code = 0;
    DWORD status_size = sizeof(status_code);
    WinHttpQueryHeaders(h_request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                        WINHTTP_HEADER_NAME_BY_INDEX, &status_code, &status_size, WINHTTP_NO_HEADER_INDEX);
    out_status_code = status_code;

    std::string response_data;
    DWORD bytes_available = 0;
    while (WinHttpQueryDataAvailable(h_request, &bytes_available) && bytes_available > 0) {
        std::vector<char> buffer(bytes_available + 1, 0);
        DWORD bytes_read = 0;
        if (WinHttpReadData(h_request, buffer.data(), bytes_available, &bytes_read) && bytes_read > 0) {
            response_data.append(buffer.data(), bytes_read);
        } else {
            break;
        }
    }

    WinHttpCloseHandle(h_request);
    out_body = response_data;
    return true;
}

} // namespace

std::string UpdateService::GetCurrentVersion() {
    return kVersionString;
}

std::wstring UpdateService::GetCurrentVersionW() {
    return kVersionWString;
}

std::wstring UpdateService::GetAppDataDirectory() {
    wchar_t path[MAX_PATH] = {0};
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, 0, path))) {
        std::wstring dir = std::wstring(path) + L"\\PC-Bangla-Typing-App";
        CreateDirectoryW(dir.c_str(), NULL);
        return dir;
    }
    return L"";
}

std::wstring UpdateService::GetUpdateCachePath() {
    std::wstring dir = GetAppDataDirectory();
    if (dir.empty()) return L"";
    return dir + L"\\update_cache.json";
}

bool UpdateService::ShouldCheckOnStartup() {
    std::wstring cache_file = GetUpdateCachePath();
    if (cache_file.empty()) return true;

    std::ifstream in(cache_file.c_str());
    if (!in.is_open()) return true;

    std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    in.close();

    uint64_t last_check = 0;
    if (ExtractJsonNumber(content, "last_check_epoch", last_check)) {
        uint64_t now = CurrentEpochSeconds();
        if (now >= last_check && (now - last_check) < kStartupCheckIntervalSeconds) {
            return false; // Still within 24h cooldown
        }
    }
    return true;
}

void UpdateService::RecordCheckTimestamp() {
    std::wstring cache_file = GetUpdateCachePath();
    if (cache_file.empty()) return;

    uint64_t now = CurrentEpochSeconds();
    std::string dismissed_ver;
    uint64_t dismissed_time = 0;

    // Preserve existing dismissed settings
    std::ifstream in(cache_file.c_str());
    if (in.is_open()) {
        std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        in.close();
        ExtractJsonString(content, "dismissed_version", dismissed_ver);
        ExtractJsonNumber(content, "dismissed_epoch", dismissed_time);
    }

    std::ofstream out(cache_file.c_str());
    if (out.is_open()) {
        out << "{\n";
        out << "  \"last_check_epoch\": " << now << ",\n";
        out << "  \"dismissed_version\": \"" << dismissed_ver << "\",\n";
        out << "  \"dismissed_epoch\": " << dismissed_time << "\n";
        out << "}\n";
    }
}

void UpdateService::DismissVersion(const std::string& version) {
    std::wstring cache_file = GetUpdateCachePath();
    if (cache_file.empty()) return;

    uint64_t now = CurrentEpochSeconds();
    uint64_t last_check = 0;

    std::ifstream in(cache_file.c_str());
    if (in.is_open()) {
        std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        in.close();
        ExtractJsonNumber(content, "last_check_epoch", last_check);
    }

    std::ofstream out(cache_file.c_str());
    if (out.is_open()) {
        out << "{\n";
        out << "  \"last_check_epoch\": " << (last_check ? last_check : now) << ",\n";
        out << "  \"dismissed_version\": \"" << version << "\",\n";
        out << "  \"dismissed_epoch\": " << now << "\n";
        out << "}\n";
    }
}

bool UpdateService::IsVersionDismissed(const std::string& version) {
    std::wstring cache_file = GetUpdateCachePath();
    if (cache_file.empty()) return false;

    std::ifstream in(cache_file.c_str());
    if (!in.is_open()) return false;

    std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    in.close();

    std::string dismissed_ver;
    uint64_t dismissed_time = 0;
    if (ExtractJsonString(content, "dismissed_version", dismissed_ver) &&
        ExtractJsonNumber(content, "dismissed_epoch", dismissed_time)) {
        if (dismissed_ver == version) {
            uint64_t now = CurrentEpochSeconds();
            if (now >= dismissed_time && (now - dismissed_time) < kDismissSnoozeSeconds) {
                return true; // Still within snooze period
            }
        }
    }
    return false;
}

int UpdateService::CompareVersions(const std::string& current, const std::string& remote) {
    SemVer v_curr = SemVer::Parse(current);
    SemVer v_rem = SemVer::Parse(remote);
    return v_curr.Compare(v_rem);
}

bool UpdateService::ParseReleaseJson(const std::string& json_str, ReleaseInfo& out_info, bool allow_prereleases) {
    if (json_str.empty()) return false;

    std::vector<std::string> release_objects;
    size_t first_non_ws = json_str.find_first_not_of(" \t\r\n");
    if (first_non_ws == std::string::npos) return false;

    if (json_str[first_non_ws] == '[') {
        int depth = 0;
        size_t start_obj = std::string::npos;
        bool in_string = false;
        for (size_t i = first_non_ws + 1; i < json_str.size(); ++i) {
            char c = json_str[i];
            if (c == '"' && (i == 0 || json_str[i - 1] != '\\')) {
                in_string = !in_string;
                continue;
            }
            if (in_string) continue;

            if (c == '{') {
                if (depth == 0) start_obj = i;
                depth++;
            } else if (c == '}') {
                depth--;
                if (depth == 0 && start_obj != std::string::npos) {
                    release_objects.push_back(json_str.substr(start_obj, i - start_obj + 1));
                    start_obj = std::string::npos;
                }
            } else if (c == ']' && depth == 0) {
                break;
            }
        }
    } else {
        release_objects.push_back(json_str);
    }

    ReleaseInfo best_candidate;
    SemVer best_version;
    bool found_candidate = false;

    for (const auto& obj_str : release_objects) {
        ReleaseInfo candidate;
        if (!ParseSingleReleaseObject(obj_str, candidate)) {
            continue;
        }

        // Never consider draft releases
        if (candidate.is_draft) {
            continue;
        }

        // Production Release Policy: Normal users receive STABLE releases only.
        if (!allow_prereleases && candidate.is_prerelease) {
            continue;
        }

        SemVer cand_ver = SemVer::Parse(candidate.version);
        if (!allow_prereleases && !cand_ver.prerelease.empty()) {
            continue;
        }

        if (!found_candidate) {
            best_candidate = candidate;
            best_version = cand_ver;
            found_candidate = true;
        } else {
            // Pick highest eligible semantic version
            if (cand_ver.Compare(best_version) > 0) {
                best_candidate = candidate;
                best_version = cand_ver;
            }
        }
    }

    if (found_candidate) {
        out_info = best_candidate;
        return true;
    }

    return false;
}

UpdateCheckResult UpdateService::CheckForUpdate(ReleaseInfo& out_info, bool force_bypass_cooldown) {
    if (!force_bypass_cooldown && !ShouldCheckOnStartup()) {
        return UpdateCheckResult::kUpToDate;
    }

    // Open WinHTTP session with custom User-Agent
    HINTERNET h_session = WinHttpOpen(L"Likhi-Update-Checker/1.0 (Windows NT; x64)",
                                      WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                      WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!h_session) {
        return UpdateCheckResult::kNetworkOffline;
    }

    // Short timeouts so we never hang or delay startup: 3000ms each
    DWORD timeout_ms = 3000;
    WinHttpSetOption(h_session, WINHTTP_OPTION_CONNECT_TIMEOUT, &timeout_ms, sizeof(timeout_ms));
    WinHttpSetOption(h_session, WINHTTP_OPTION_SEND_TIMEOUT, &timeout_ms, sizeof(timeout_ms));
    WinHttpSetOption(h_session, WINHTTP_OPTION_RECEIVE_TIMEOUT, &timeout_ms, sizeof(timeout_ms));

    HINTERNET h_connect = WinHttpConnect(h_session, L"api.github.com", INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!h_connect) {
        WinHttpCloseHandle(h_session);
        return UpdateCheckResult::kNetworkOffline;
    }

    DWORD status_code = 0;
    std::string response_data;
    bool req_ok = FetchUrl(h_connect, L"/repos/Badboy-collab/likhi/releases/latest", status_code, response_data);

    if (!req_ok) {
        WinHttpCloseHandle(h_connect);
        WinHttpCloseHandle(h_session);
        return UpdateCheckResult::kNetworkOffline;
    }

    // If /releases/latest returned 404 (e.g. only prereleases exist or none published), fallback to /releases?per_page=5
    if (status_code == 404) {
        req_ok = FetchUrl(h_connect, L"/repos/Badboy-collab/likhi/releases?per_page=5", status_code, response_data);
        if (!req_ok) {
            WinHttpCloseHandle(h_connect);
            WinHttpCloseHandle(h_session);
            return UpdateCheckResult::kNetworkOffline;
        }
    }

    WinHttpCloseHandle(h_connect);
    WinHttpCloseHandle(h_session);

    if (status_code != 200) {
        if (status_code == 404) {
            // No releases exist at all in repository
            RecordCheckTimestamp();
            return UpdateCheckResult::kUpToDate;
        }
        return UpdateCheckResult::kError;
    }

    // Production Release Policy: strictly ignore drafts and prereleases for production installations
    if (!ParseReleaseJson(response_data, out_info, false)) {
        // Releases exist on remote (e.g. v1.0.0-test2), but NONE are stable releases.
        // By documented policy, normal production users stay up-to-date with no errors.
        RecordCheckTimestamp();
        return UpdateCheckResult::kUpToDate;
    }

    RecordCheckTimestamp();

    int cmp = CompareVersions(GetCurrentVersion(), out_info.version);
    if (cmp < 0) {
        // Newer stable version exists!
        if (!force_bypass_cooldown && IsVersionDismissed(out_info.version)) {
            return UpdateCheckResult::kUpToDate; // User chose "Later" recently
        }
        return UpdateCheckResult::kUpdateAvailable;
    }

    return UpdateCheckResult::kUpToDate;
}

bool UpdateService::LaunchOfficialUpdateFlow(const std::string& custom_url) {
    std::string url = custom_url.empty() ? kOfficialUpdateUrl : custom_url;
    std::wstring w_url = Utf8ToWide(url);
    HINSTANCE res = ShellExecuteW(NULL, L"open", w_url.c_str(), NULL, NULL, SW_SHOWNORMAL);
    return reinterpret_cast<INT_PTR>(res) > 32;
}

bool UpdateService::VerifySha256(const std::wstring& file_path, const std::string& expected_hex) {
    if (file_path.empty() || expected_hex.empty()) return false;

    // Normalize expected hex: strip "sha256:" prefix if present and convert to lowercase
    std::string clean_expected = expected_hex;
    if (clean_expected.find("sha256:") == 0) {
        clean_expected = clean_expected.substr(7);
    }
    std::transform(clean_expected.begin(), clean_expected.end(), clean_expected.begin(), ::tolower);

    HANDLE hFile = CreateFileW(file_path.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL,
                               OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return false;

    HCRYPTPROV hProv = 0;
    HCRYPTHASH hHash = 0;
    bool success = false;

    if (CryptAcquireContextW(&hProv, NULL, NULL, PROV_RSA_AES, CRYPT_VERIFYCONTEXT)) {
        if (CryptCreateHash(hProv, CALG_SHA_256, 0, 0, &hHash)) {
            BYTE buffer[8192];
            DWORD bytesRead = 0;
            BOOL readSuccess = TRUE;

            while ((readSuccess = ReadFile(hFile, buffer, sizeof(buffer), &bytesRead, NULL)) && bytesRead > 0) {
                if (!CryptHashData(hHash, buffer, bytesRead, 0)) {
                    readSuccess = FALSE;
                    break;
                }
            }

            if (readSuccess) {
                BYTE hashBytes[32];
                DWORD hashLen = sizeof(hashBytes);
                if (CryptGetHashParam(hHash, HP_HASHVAL, hashBytes, &hashLen, 0)) {
                    std::ostringstream ss;
                    for (DWORD i = 0; i < hashLen; ++i) {
                        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(hashBytes[i]);
                    }
                    std::string calculated_hex = ss.str();
                    std::transform(calculated_hex.begin(), calculated_hex.end(), calculated_hex.begin(), ::tolower);
                    success = (calculated_hex == clean_expected);
                }
            }
            CryptDestroyHash(hHash);
        }
        CryptReleaseContext(hProv, 0);
    }

    CloseHandle(hFile);
    return success;
}

} // namespace likhi
