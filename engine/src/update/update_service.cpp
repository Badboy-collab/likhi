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
#include <ctime>

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

void UpdateService::LogDiagnostic(const std::string& message) {
    std::wstring dir = GetAppDataDirectory();
    if (dir.empty()) return;
    std::wstring log_path = dir + L"\\update.log";

    // Rotate if larger than 64KB
    WIN32_FILE_ATTRIBUTE_DATA fad = {0};
    if (GetFileAttributesExW(log_path.c_str(), GetFileExInfoStandard, &fad)) {
        if (fad.nFileSizeLow > 64 * 1024) {
            DeleteFileW(log_path.c_str());
        }
    }

    std::ofstream out(log_path.c_str(), std::ios::app);
    if (out.is_open()) {
        SYSTEMTIME st;
        GetLocalTime(&st);
        char buf[64];
        snprintf(buf, sizeof(buf), "[%04d-%02d-%02d %02d:%02d:%02d] ",
                 st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
        out << buf << message << "\n";
    }
}

std::wstring UpdateService::GetLastCheckTimeString() {
    std::wstring cache_file = GetUpdateCachePath();
    if (cache_file.empty()) return L"এখনও পরীক্ষা করা হয়নি";

    std::ifstream in(cache_file.c_str());
    if (!in.is_open()) return L"এখনও পরীক্ষা করা হয়নি";

    std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    in.close();

    uint64_t last_check = 0;
    if (!ExtractJsonNumber(content, "last_check_epoch", last_check) || last_check == 0) {
        return L"এখনও পরীক্ষা করা হয়নি";
    }

    time_t check_time = static_cast<time_t>(last_check);
    tm lt = {0};
    localtime_s(&lt, &check_time);

    time_t now_time = time(nullptr);
    tm now_lt = {0};
    localtime_s(&now_lt, &now_time);

    auto to_bengali_digits = [](const std::wstring& str) -> std::wstring {
        std::wstring res;
        for (wchar_t c : str) {
            if (c >= L'0' && c <= L'9') {
                res += static_cast<wchar_t>(L'০' + (c - L'0'));
            } else {
                res += c;
            }
        }
        return res;
    };

    wchar_t time_buf[32] = {0};
    swprintf(time_buf, 32, L"%02d:%02d", lt.tm_hour, lt.tm_min);
    std::wstring b_time = to_bengali_digits(time_buf);

    if (lt.tm_year == now_lt.tm_year && lt.tm_yday == now_lt.tm_yday) {
        return L"আজ " + b_time;
    } else if (lt.tm_year == now_lt.tm_year && lt.tm_yday == now_lt.tm_yday - 1) {
        return L"গতকাল " + b_time;
    } else {
        wchar_t date_buf[64] = {0};
        swprintf(date_buf, 64, L"%02d/%02d/%04d %02d:%02d",
                 lt.tm_mday, lt.tm_mon + 1, lt.tm_year + 1900, lt.tm_hour, lt.tm_min);
        return to_bengali_digits(date_buf);
    }
}

bool UpdateService::ParseReleaseJson(const std::string& json_str, ReleaseInfo& out_info) {
    if (json_str.empty()) return false;

    // Find the relevant json segment. If json_str is an array [...], isolate the first object { ... }
    std::string target_json = json_str;
    size_t start_brace = json_str.find('{');
    if (start_brace == std::string::npos) return false;

    // 1. tag_name (e.g. "v1.1.0" or "v1.0.0-test2")
    if (!ExtractJsonString(target_json, "tag_name", out_info.tag_name, start_brace)) {
        return false;
    }

    // Extract core version from tag_name (strip leading 'v' / 'V')
    out_info.version = out_info.tag_name;
    if (!out_info.version.empty() && (out_info.version[0] == 'v' || out_info.version[0] == 'V')) {
        out_info.version = out_info.version.substr(1);
    }

    // 2. name (Release Title)
    ExtractJsonString(target_json, "name", out_info.name, start_brace);
    if (out_info.name.empty()) out_info.name = "Likhi " + out_info.tag_name;

    // 3. body (Release notes)
    ExtractJsonString(target_json, "body", out_info.release_notes, start_brace);

    // 4. published_at
    ExtractJsonString(target_json, "published_at", out_info.published_at, start_brace);

    // 5. Assets inspection: look for LikhiSetup.exe or any .exe asset inside the first release
    out_info.update_page_url = kOfficialUpdateUrl;

    size_t assets_pos = target_json.find("\"assets\"", start_brace);
    if (assets_pos != std::string::npos) {
        // Limit search to the assets block of this release
        size_t assets_end = target_json.find(']', assets_pos);
        if (assets_end == std::string::npos) assets_end = target_json.size();
        std::string assets_json = target_json.substr(assets_pos, assets_end - assets_pos);

        size_t search_pos = 0;
        std::string asset_name, dl_url, digest;
        uint64_t size_bytes = 0;
        
        while (ExtractJsonString(assets_json, "name", asset_name, search_pos, &search_pos)) {
            if (asset_name.size() > 4 && asset_name.substr(asset_name.size() - 4) == ".exe") {
                ExtractJsonString(assets_json, "browser_download_url", dl_url, search_pos);
                ExtractJsonString(assets_json, "digest", digest, search_pos);
                ExtractJsonNumber(assets_json, "size", size_bytes, search_pos);

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

UpdateCheckResult UpdateService::CheckForUpdate(ReleaseInfo& out_info, bool force_bypass_cooldown) {
    if (!force_bypass_cooldown && !ShouldCheckOnStartup()) {
        LogDiagnostic("Startup check skipped: cooldown active");
        return UpdateCheckResult::kUpToDate;
    }

    LogDiagnostic("Starting update check (force=" + std::string(force_bypass_cooldown ? "true" : "false") + ")");

    // Open WinHTTP session with automatic proxy detection
    HINTERNET h_session = WinHttpOpen(L"Likhi-Update-Checker/1.0 (Windows NT; x64)",
                                      WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                                      WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!h_session) {
        h_session = WinHttpOpen(L"Likhi-Update-Checker/1.0 (Windows NT; x64)",
                                WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    }
    if (!h_session) {
        LogDiagnostic("WinHttpOpen failed (err=" + std::to_string(GetLastError()) + ")");
        return UpdateCheckResult::kNetworkOffline;
    }

    // Explicitly configure TLS 1.2 and TLS 1.3
    DWORD protocols = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2;
#if defined(WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_3)
    protocols |= WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_3;
#endif
    WinHttpSetOption(h_session, WINHTTP_OPTION_SECURE_PROTOCOLS, &protocols, sizeof(protocols));

    // Timeouts: 4000ms each
    DWORD timeout_ms = 4000;
    WinHttpSetOption(h_session, WINHTTP_OPTION_CONNECT_TIMEOUT, &timeout_ms, sizeof(timeout_ms));
    WinHttpSetOption(h_session, WINHTTP_OPTION_SEND_TIMEOUT, &timeout_ms, sizeof(timeout_ms));
    WinHttpSetOption(h_session, WINHTTP_OPTION_RECEIVE_TIMEOUT, &timeout_ms, sizeof(timeout_ms));

    HINTERNET h_connect = WinHttpConnect(h_session, L"api.github.com", INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!h_connect) {
        DWORD err = GetLastError();
        LogDiagnostic("WinHttpConnect to api.github.com failed (err=" + std::to_string(err) + ")");
        WinHttpCloseHandle(h_session);
        return UpdateCheckResult::kNetworkOffline;
    }

    auto fetch_url = [&](const wchar_t* path, std::string& out_body, DWORD& out_status) -> bool {
        HINTERNET h_request = WinHttpOpenRequest(h_connect, L"GET", path,
                                                NULL, WINHTTP_NO_REFERER,
                                                WINHTTP_DEFAULT_ACCEPT_TYPES,
                                                WINHTTP_FLAG_SECURE);
        if (!h_request) return false;

        DWORD redirect_policy = WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS;
        WinHttpSetOption(h_request, WINHTTP_OPTION_REDIRECT_POLICY, &redirect_policy, sizeof(redirect_policy));

        const wchar_t* headers = L"Accept: application/vnd.github.v3+json\r\nUser-Agent: Likhi-Update-Checker/1.0\r\n";
        BOOL sent = WinHttpSendRequest(h_request, headers, (DWORD)-1L, WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
        if (!sent || !WinHttpReceiveResponse(h_request, NULL)) {
            WinHttpCloseHandle(h_request);
            return false;
        }

        DWORD status_code = 0;
        DWORD status_size = sizeof(status_code);
        WinHttpQueryHeaders(h_request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                            WINHTTP_HEADER_NAME_BY_INDEX, &status_code, &status_size, WINHTTP_NO_HEADER_INDEX);
        out_status = status_code;

        if (status_code == 200) {
            DWORD bytes_avail = 0;
            while (WinHttpQueryDataAvailable(h_request, &bytes_avail) && bytes_avail > 0) {
                std::vector<char> buf(bytes_avail + 1, 0);
                DWORD bytes_read = 0;
                if (WinHttpReadData(h_request, buf.data(), bytes_avail, &bytes_read) && bytes_read > 0) {
                    out_body.append(buf.data(), bytes_read);
                } else {
                    break;
                }
            }
        }

        WinHttpCloseHandle(h_request);
        return true;
    };

    std::string response_data;
    DWORD status_code = 0;

    LogDiagnostic("Querying GitHub endpoint 1: /repos/Badboy-collab/likhi/releases/latest");
    bool ok = fetch_url(L"/repos/Badboy-collab/likhi/releases/latest", response_data, status_code);
    LogDiagnostic("Endpoint 1 result: ok=" + std::to_string(ok) + ", status=" + std::to_string(status_code) + ", bytes=" + std::to_string(response_data.size()));

    // If /releases/latest returned 404 (due to pre-releases or untagged latest), fall back to /releases?per_page=5
    if (!ok || status_code == 404 || status_code != 200) {
        LogDiagnostic("Falling back to GitHub endpoint 2: /repos/Badboy-collab/likhi/releases?per_page=5");
        response_data.clear();
        status_code = 0;
        ok = fetch_url(L"/repos/Badboy-collab/likhi/releases?per_page=5", response_data, status_code);
        LogDiagnostic("Endpoint 2 result: ok=" + std::to_string(ok) + ", status=" + std::to_string(status_code) + ", bytes=" + std::to_string(response_data.size()));
    }

    WinHttpCloseHandle(h_connect);
    WinHttpCloseHandle(h_session);

    if (!ok) {
        LogDiagnostic("Network request failed completely (offline / DNS / connection error)");
        return UpdateCheckResult::kNetworkOffline;
    }

    if (status_code != 200) {
        LogDiagnostic("HTTP status code is not 200 (status=" + std::to_string(status_code) + ")");
        return UpdateCheckResult::kError;
    }

    if (!ParseReleaseJson(response_data, out_info)) {
        LogDiagnostic("ParseReleaseJson failed on payload");
        return UpdateCheckResult::kError;
    }

    LogDiagnostic("Parsed remote release: version=" + out_info.version + ", tag=" + out_info.tag_name + ", url=" + out_info.download_url);

    RecordCheckTimestamp();

    int cmp = CompareVersions(GetCurrentVersion(), out_info.version);
    LogDiagnostic("Compare: current=" + GetCurrentVersion() + " vs remote=" + out_info.version + " -> cmp=" + std::to_string(cmp));

    if (cmp < 0) {
        if (!force_bypass_cooldown && IsVersionDismissed(out_info.version)) {
            LogDiagnostic("Update " + out_info.version + " is dismissed; returning up-to-date");
            return UpdateCheckResult::kUpToDate;
        }
        LogDiagnostic("New update available!");
        return UpdateCheckResult::kUpdateAvailable;
    }

    LogDiagnostic("Application is up to date");
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
