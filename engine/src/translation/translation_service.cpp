#include "../../engine/include/translation_service.h"
#include <algorithm>
#include <sstream>
#include <cctype>

namespace likhi {

namespace {

inline bool IsWs(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

void AppendUtf8(std::string& out, unsigned cp) {
    if (cp < 0x80) {
        out += static_cast<char>(cp);
    } else if (cp < 0x800) {
        out += static_cast<char>(0xC0 | (cp >> 6));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else if (cp < 0x10000) {
        out += static_cast<char>(0xE0 | (cp >> 12));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else {
        out += static_cast<char>(0xF0 | (cp >> 18));
        out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    }
}

int HexDigit(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

bool ReadJsonString(const std::string& s, size_t* p, std::string& out) {
    if (*p >= s.size() || s[*p] != '"') return false;
    ++(*p);
    out.clear();
    while (*p < s.size()) {
        char c = s[*p];
        if (c == '"') { ++(*p); return true; }
        if (c != '\\') { out += c; ++(*p); continue; }
        // Escape sequence
        ++(*p);
        if (*p >= s.size()) return false;
        char e = s[*p];
        switch (e) {
            case '"': out += '"'; ++(*p); break;
            case '\\': out += '\\'; ++(*p); break;
            case '/': out += '/'; ++(*p); break;
            case 'n': out += '\n'; ++(*p); break;
            case 'r': out += '\r'; ++(*p); break;
            case 't': out += '\t'; ++(*p); break;
            case 'b': out += '\b'; ++(*p); break;
            case 'f': out += '\f'; ++(*p); break;
            case 'u': {
                if (*p + 4 >= s.size()) return false;
                int cp = 0;
                for (int i = 1; i <= 4; ++i) {
                    int d = HexDigit(s[*p + i]);
                    if (d < 0) return false;
                    cp = (cp << 4) | d;
                }
                if (cp >= 0xD800 && cp <= 0xDBFF &&
                    *p + 11 < s.size() && s[*p + 5] == '\\' && s[*p + 6] == 'u') {
                    int lo = 0;
                    for (int i = 7; i <= 10; ++i) {
                        int d = HexDigit(s[*p + i]);
                        if (d < 0) { lo = -1; break; }
                        lo = (lo << 4) | d;
                    }
                    if (lo >= 0xDC00 && lo <= 0xDFFF) {
                        cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                        *p += 11;
                    } else {
                        *p += 5;
                    }
                } else {
                    *p += 5;
                }
                AppendUtf8(out, static_cast<unsigned>(cp));
                break;
            }
            default:
                out += e; ++(*p); break;
        }
    }
    return false;
}

std::string UrlEncode(const std::string& s) {
    static const char* hex = "0123456789ABCDEF";
    std::string out;
    for (unsigned char c : s) {
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') {
            out += static_cast<char>(c);
        } else {
            out += '%';
            out += hex[c >> 4];
            out += hex[c & 0x0F];
        }
    }
    return out;
}

std::string WideToUtf8(const std::wstring& w) {
    if (w.empty()) return std::string();
    int size = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string out(size, '\0');
    if (size > 0) {
        WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), &out[0], size, nullptr, nullptr);
    }
    return out;
}

std::wstring Utf8ToWide(const std::string& s) {
    if (s.empty()) return std::wstring();
    int size = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    std::wstring out(size, L'\0');
    if (size > 0) {
        MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &out[0], size);
    }
    return out;
}

void PolishEnglishSentence(std::string& text) {
    // Trim leading/trailing whitespace
    while (!text.empty() && IsWs(text.front())) text.erase(text.begin());
    while (!text.empty() && IsWs(text.back())) text.pop_back();

    if (text.empty()) return;

    // Capitalize first character
    if (text[0] >= 'a' && text[0] <= 'z') {
        text[0] = static_cast<char>(text[0] - 'a' + 'A');
    }

    // Capitalize standalone " i " or " i'" -> " I " or " I'"
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == 'i') {
            bool left_boundary = (i == 0 || text[i - 1] == ' ' || text[i - 1] == '"' || text[i - 1] == '(');
            bool right_boundary = (i + 1 == text.size() || text[i + 1] == ' ' || text[i + 1] == '\'' ||
                                   text[i + 1] == ',' || text[i + 1] == '.' || text[i + 1] == '?' ||
                                   text[i + 1] == '!' || text[i + 1] == '"' || text[i + 1] == ')');
            if (left_boundary && right_boundary) {
                text[i] = 'I';
            }
        }
    }
}

} // namespace

bool ParseTranslationResponse(const std::string& json_body, std::string& out_english) {
    out_english.clear();
    if (json_body.empty()) return false;

    // Locate the first string literal inside array: e.g. ["hello world"] or [["hello world"]]
    size_t pos = 0;
    while (pos < json_body.size() && (IsWs(json_body[pos]) || json_body[pos] == '[')) {
        ++pos;
    }

    if (pos >= json_body.size() || json_body[pos] != '"') {
        return false;
    }

    std::string parsed;
    if (!ReadJsonString(json_body, &pos, parsed)) {
        return false;
    }

    PolishEnglishSentence(parsed);
    out_english = parsed;
    return !out_english.empty();
}

TranslationService::TranslationService()
    : stop_(false), consent_enabled_(false), pending_gen_(0),
      h_session_(nullptr), h_connect_(nullptr) {}

TranslationService::~TranslationService() {
    Stop();
}

bool TranslationService::Start() {
    if (worker_thread_.joinable()) return true;
    stop_ = false;
    worker_thread_ = std::thread(&TranslationService::WorkerLoop, this);
    return true;
}

void TranslationService::Stop() {
    {
        std::lock_guard<std::mutex> lk(queue_mutex_);
        stop_ = true;
        pending_text_.clear();
    }
    cv_.notify_all();
    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }
    CloseHttp();
}

void TranslationService::RequestTranslation(const std::wstring& bengali_text, uint32_t generation) {
    if (bengali_text.empty()) return;

    // Check cache first; if cached, invoke callback immediately
    {
        std::lock_guard<std::mutex> lk(cache_mutex_);
        auto it = cache_.find(bengali_text);
        if (it != cache_.end()) {
            if (callback_) {
                callback_(bengali_text, it->second, generation);
            }
            return;
        }
    }

    // Queue for async translation
    {
        std::lock_guard<std::mutex> lk(queue_mutex_);
        pending_text_ = bengali_text;
        pending_gen_ = generation;
    }
    cv_.notify_all();
}

void TranslationService::Cancel() {
    std::lock_guard<std::mutex> lk(queue_mutex_);
    pending_text_.clear();
}

bool TranslationService::LookupCache(const std::wstring& bengali_text, std::wstring& out_english) {
    std::lock_guard<std::mutex> lk(cache_mutex_);
    auto it = cache_.find(bengali_text);
    if (it != cache_.end()) {
        out_english = it->second;
        return true;
    }
    return false;
}

void TranslationService::ClearCache() {
    std::lock_guard<std::mutex> lk(cache_mutex_);
    cache_.clear();
}

size_t TranslationService::CacheSize() {
    std::lock_guard<std::mutex> lk(cache_mutex_);
    return cache_.size();
}

bool TranslationService::EnsureHttp() {
    if (h_session_ && h_connect_) return true;
    CloseHttp();

    h_session_ = WinHttpOpen(L"Likhi/1.0 (Windows NT; Bangla Translation)",
                             WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                             WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!h_session_) return false;

    DWORD ms = 4000;
    WinHttpSetOption(h_session_, WINHTTP_OPTION_CONNECT_TIMEOUT, &ms, sizeof(ms));
    WinHttpSetOption(h_session_, WINHTTP_OPTION_SEND_TIMEOUT, &ms, sizeof(ms));
    WinHttpSetOption(h_session_, WINHTTP_OPTION_RECEIVE_TIMEOUT, &ms, sizeof(ms));

    h_connect_ = WinHttpConnect(h_session_, L"clients5.google.com",
                                INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!h_connect_) {
        CloseHttp();
        return false;
    }
    return true;
}

void TranslationService::CloseHttp() {
    if (h_connect_) {
        WinHttpCloseHandle(h_connect_);
        h_connect_ = nullptr;
    }
    if (h_session_) {
        WinHttpCloseHandle(h_session_);
        h_session_ = nullptr;
    }
}

TranslationResult TranslationService::TranslateDirect(const std::wstring& bengali_text, uint32_t timeout_ms) {
    TranslationResult res;
    res.source_bengali = bengali_text;
    res.success = false;

    if (bengali_text.empty()) {
        res.error = "Empty input";
        return res;
    }

    // Check cache
    {
        std::lock_guard<std::mutex> lk(cache_mutex_);
        auto it = cache_.find(bengali_text);
        if (it != cache_.end()) {
            res.translated_english = it->second;
            res.success = true;
            return res;
        }
    }

    // Consent check: if user has disabled translation or not consented, do not query
    if (!consent_enabled_.load()) {
        res.error = "Translation consent disabled by user";
        return res;
    }

    HINTERNET h_sess = WinHttpOpen(L"Likhi-Translator/1.0",
                                   WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                   WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!h_sess) {
        res.error = "WinHttpOpen failed";
        return res;
    }

    WinHttpSetOption(h_sess, WINHTTP_OPTION_CONNECT_TIMEOUT, &timeout_ms, sizeof(timeout_ms));
    WinHttpSetOption(h_sess, WINHTTP_OPTION_SEND_TIMEOUT, &timeout_ms, sizeof(timeout_ms));
    WinHttpSetOption(h_sess, WINHTTP_OPTION_RECEIVE_TIMEOUT, &timeout_ms, sizeof(timeout_ms));

    HINTERNET h_conn = WinHttpConnect(h_sess, L"clients5.google.com",
                                      INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!h_conn) {
        WinHttpCloseHandle(h_sess);
        res.error = "WinHttpConnect failed";
        return res;
    }

    std::string utf8_text = WideToUtf8(bengali_text);
    std::string path = "/translate_a/t?client=dict-chrome-ex&sl=bn&tl=en&q=" + UrlEncode(utf8_text);
    std::wstring path_w = Utf8ToWide(path);

    HINTERNET h_req = WinHttpOpenRequest(h_conn, L"GET", path_w.c_str(),
                                         nullptr, WINHTTP_NO_REFERER,
                                         WINHTTP_DEFAULT_ACCEPT_TYPES,
                                         WINHTTP_FLAG_SECURE);
    if (!h_req) {
        WinHttpCloseHandle(h_conn);
        WinHttpCloseHandle(h_sess);
        res.error = "WinHttpOpenRequest failed";
        return res;
    }

    const wchar_t* headers = L"User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64)\r\n";
    BOOL sent = WinHttpSendRequest(h_req, headers, (DWORD)-1L,
                                   WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
    if (!sent || !WinHttpReceiveResponse(h_req, nullptr)) {
        WinHttpCloseHandle(h_req);
        WinHttpCloseHandle(h_conn);
        WinHttpCloseHandle(h_sess);
        res.error = "WinHttp send/receive failed";
        return res;
    }

    DWORD status_code = 0;
    DWORD status_size = sizeof(status_code);
    WinHttpQueryHeaders(h_req, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                        WINHTTP_HEADER_NAME_BY_INDEX, &status_code, &status_size, WINHTTP_NO_HEADER_INDEX);

    if (status_code != 200) {
        WinHttpCloseHandle(h_req);
        WinHttpCloseHandle(h_conn);
        WinHttpCloseHandle(h_sess);
        res.error = "HTTP error code: " + std::to_string(status_code);
        return res;
    }

    std::string response_body;
    DWORD bytes_avail = 0;
    while (WinHttpQueryDataAvailable(h_req, &bytes_avail) && bytes_avail > 0) {
        std::vector<char> buffer(bytes_avail + 1, 0);
        DWORD bytes_read = 0;
        if (WinHttpReadData(h_req, buffer.data(), bytes_avail, &bytes_read) && bytes_read > 0) {
            response_body.append(buffer.data(), bytes_read);
        }
    }

    WinHttpCloseHandle(h_req);
    WinHttpCloseHandle(h_conn);
    WinHttpCloseHandle(h_sess);

    std::string out_english;
    if (ParseTranslationResponse(response_body, out_english)) {
        res.translated_english = Utf8ToWide(out_english);
        res.success = true;

        // Store in cache
        std::lock_guard<std::mutex> lk(cache_mutex_);
        if (cache_.size() >= 500) {
            cache_.erase(cache_.begin());
        }
        cache_[bengali_text] = res.translated_english;
    } else {
        res.error = "Failed to parse translation response: " + response_body;
    }

    return res;
}

void TranslationService::WorkerLoop() {
    std::unique_lock<std::mutex> lk(queue_mutex_);

    while (!stop_) {
        cv_.wait(lk, [&] { return stop_ || !pending_text_.empty(); });
        if (stop_) break;

        // Debounce window
        while (!stop_) {
            std::wstring text = pending_text_;
            uint32_t gen = pending_gen_;
            lk.unlock();

            std::unique_lock<std::mutex> quiet(queue_mutex_);
            bool changed = cv_.wait_for(quiet, std::chrono::milliseconds(kDebounceMs),
                                        [&] { return stop_ || pending_text_ != text || pending_gen_ != gen; });
            if (stop_) {
                lk = std::move(quiet);
                break;
            }
            if (changed) {
                lk = std::move(quiet);
                continue;
            }

            // Debounce passed: extract item
            pending_text_.clear();
            quiet.unlock();

            // Check cache before network
            std::wstring cached_trans;
            if (LookupCache(text, cached_trans)) {
                if (callback_ && !stop_) {
                    callback_(text, cached_trans, gen);
                }
            } else if (consent_enabled_.load()) {
                TranslationResult result = TranslateDirect(text);
                if (result.success && callback_ && !stop_) {
                    callback_(text, result.translated_english, gen);
                }
            }

            lk.lock();
            break;
        }
    }
}

} // namespace likhi
