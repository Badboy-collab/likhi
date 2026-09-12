#include "../include/cloud_translit.h"
#include <winhttp.h>
#include <algorithm>
#include <cstdio>

namespace bangla_tsf {

// ============================================================
// Google Input Tools cloud response parser
// ============================================================
namespace {

inline bool IsWs(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }

// Appends a single Unicode code point as UTF-8 to out.
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

// Reads one JSON string starting at *p (which must point at the opening '"').
// Advances *p past the closing quote. Returns false on malformed input.
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
                // digits occupy *p+1 .. *p+4; surrogate pair continues at
                // *p+5 '\' *p+6 'u' digits *p+7 .. *p+10.
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
                        *p += 5; // lone high surrogate — encode as-is
                    }
                } else {
                    *p += 5; // regular BMP code point
                }
                AppendUtf8(out, static_cast<unsigned>(cp));
                break;
            }
            default:
                out += e; ++(*p); break;
        }
    }
    return false; // unterminated string
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

} // namespace

bool CloudTranslit_ParseGoogleResponse(const std::string& body,
                                       std::vector<std::string>& out) {
    out.clear();
    if (body.empty()) return false;

    // Anchor: "SUCCESS"
    size_t pos = body.find("\"SUCCESS\"");
    if (pos == std::string::npos) return false;

    // results array '[' -> first word array '[' -> candidates array '['
    pos = body.find('[', pos + 8);
    if (pos == std::string::npos) return false;
    pos = body.find('[', pos + 1);
    if (pos == std::string::npos) return false;
    pos = body.find('[', pos + 1);
    if (pos == std::string::npos) return false;
    ++pos;

    while (pos < body.size() && body[pos] != ']') {
        if (IsWs(body[pos]) || body[pos] == ',') { ++pos; continue; }
        if (body[pos] != '"') break; // unexpected token — stop
        std::string cand;
        if (!ReadJsonString(body, &pos, cand) || cand.empty()) break;
        out.push_back(cand);
    }
    return !out.empty();
}

// ============================================================
// CloudTranslit
// ============================================================

CloudTranslit::CloudTranslit() = default;

CloudTranslit::~CloudTranslit() {
    Stop();
}

bool CloudTranslit::Start() {
    if (worker_.joinable()) return true;
    stop_ = false;
    worker_ = std::thread(&CloudTranslit::WorkerMain, this);
    return true;
}

void CloudTranslit::Stop() {
    if (!worker_.joinable()) return;
    {
        std::lock_guard<std::mutex> lk(mu_);
        stop_ = true;
        pending_word_.clear();
    }
    cv_.notify_all();
    worker_.join();
    CloseHttp();
}

void CloudTranslit::Request(const std::wstring& word, uint32_t generation) {
    if (word.empty()) return;
    {
        std::lock_guard<std::mutex> lk(mu_);
        pending_word_ = word;
        pending_gen_ = generation;
    }
    cv_.notify_all();
}

void CloudTranslit::Cancel() {
    std::lock_guard<std::mutex> lk(mu_);
    pending_word_.clear();
}

void CloudTranslit::WorkerMain() {
    std::unique_lock<std::mutex> lk(mu_);
    while (!stop_) {
        cv_.wait(lk, [&] { return stop_ || !pending_word_.empty(); });
        if (stop_) break;

        // Debounce: keep waiting while the word keeps changing. Only after the
        // word has been stable for kDebounceMs do we send it. Because we only
        // consume pending_word_ after a full quiet window, rapid typing never
        // creates a request per keystroke — intermediate snapshots are skipped.
        while (!stop_) {
            std::wstring word = pending_word_;
            uint32_t gen = pending_gen_;
            lk.unlock();

            std::unique_lock<std::mutex> quiet(mu_);
            bool changed = cv_.wait_for(quiet, std::chrono::milliseconds(kDebounceMs),
                                        [&] { return stop_ || pending_word_ != word ||
                                                     pending_gen_ != gen; });
            if (stop_) { lk = std::move(quiet); break; }
            if (changed) {
                // A newer request arrived during the quiet window → re-debounce.
                lk = std::move(quiet);
                continue;
            }
            // Stable for kDebounceMs → consume and query.
            pending_word_.clear();
            quiet.unlock();

            if (!DoHttpRequest(word, gen)) {
                // Offline/slow path: silent, caller keeps local engine.
            }
            lk.lock();
            break;
        }
    }
}

bool CloudTranslit::EnsureHttp() {
    auto now = std::chrono::steady_clock::now();
    if (h_session_ && h_connect_) return true;
    if (now < offline_until_) return false; // still cooling down after a failure

    CloseHttp();
    h_session_ = WinHttpOpen(L"Likhi/1.0 (Bangla TSF)",
                             WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                             WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!h_session_) {
        offline_until_ = now + std::chrono::seconds(10);
        return false;
    }
    DWORD ms = 4000; // aggressive timeouts: offline must fall back fast
    WinHttpSetOption(h_session_, WINHTTP_OPTION_CONNECT_TIMEOUT, &ms, sizeof(ms));
    WinHttpSetOption(h_session_, WINHTTP_OPTION_SEND_TIMEOUT, &ms, sizeof(ms));
    WinHttpSetOption(h_session_, WINHTTP_OPTION_RECEIVE_TIMEOUT, &ms, sizeof(ms));

    h_connect_ = WinHttpConnect(h_session_, L"inputtools.google.com",
                                INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!h_connect_) {
        CloseHttp();
        offline_until_ = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        return false;
    }
    return true;
}

void CloudTranslit::CloseHttp() {
    if (h_connect_) { WinHttpCloseHandle(h_connect_); h_connect_ = nullptr; }
    if (h_session_) { WinHttpCloseHandle(h_session_); h_session_ = nullptr; }
}

bool CloudTranslit::DoHttpRequest(const std::wstring& word, uint32_t generation) {
    if (word.empty() || stop_) return false;
    if (!EnsureHttp()) return false;

    // Same parameters as the extracted JS engine:
    //   ?text=<enc>&itc=bn-t-i0-und&num=5&cp=0&cs=1&ie=utf-8&oe=utf-8&app=demopage
    std::string word8;
    for (wchar_t wc : word) {
        if (wc <= 0x7F) word8 += static_cast<char>(wc);
        else { /* roman buffer is ASCII; skip anything else */ }
    }
    std::string path = "/request?text=" + UrlEncode(word8) +
                       "&itc=bn-t-i0-und&num=5&cp=0&cs=1&ie=utf-8&oe=utf-8&app=likhi";

    int wlen = MultiByteToWideChar(CP_UTF8, 0, path.data(), (int)path.size(), nullptr, 0);
    std::wstring pathw(wlen, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, path.data(), (int)path.size(), &pathw[0], wlen);

    HINTERNET req = WinHttpOpenRequest(h_connect_, L"GET", pathw.c_str(), nullptr,
                                       WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                       WINHTTP_FLAG_SECURE);
    if (!req) {
        CloseHttp();
        return false;
    }

    BOOL ok = WinHttpSendRequest(req, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                 WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
    if (ok) ok = WinHttpReceiveResponse(req, nullptr);

    std::string body;
    while (ok) {
        DWORD avail = 0;
        if (!WinHttpQueryDataAvailable(req, &avail) || avail == 0) break;
        char buf[8192];
        DWORD read = 0;
        DWORD want = (std::min)(avail, (DWORD)sizeof(buf));
        if (!WinHttpReadData(req, buf, want, &read) || read == 0) { ok = FALSE; break; }
        body.append(buf, read);
    }
    WinHttpCloseHandle(req);

    if (!ok) {
        // Connection died (offline etc.): drop handles so the next attempt
        // re-negotiates, and cool down for a few seconds.
        CloseHttp();
        offline_until_ = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        return false;
    }

    std::vector<std::string> candidates8;
    if (!CloudTranslit_ParseGoogleResponse(body, candidates8)) {
        return false; // no suggestions (or unexpected shape) — fallback local
    }
    // Success: reset any cooldown.
    offline_until_ = {};

    std::vector<std::wstring> candidates;
    candidates.reserve(candidates8.size());
    for (const std::string& c8 : candidates8) {
        int clen = MultiByteToWideChar(CP_UTF8, 0, c8.data(), (int)c8.size(), nullptr, 0);
        std::wstring cw(clen, L'\0');
        if (clen > 0) MultiByteToWideChar(CP_UTF8, 0, c8.data(), (int)c8.size(), &cw[0], clen);
        if (!cw.empty()) candidates.push_back(cw);
    }
    if (candidates.empty()) return false;

    if (result_cb_ && !stop_) {
        result_cb_(word, generation, candidates);
    }
    return true;
}

} // namespace bangla_tsf
