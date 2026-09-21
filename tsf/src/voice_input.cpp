// Voice input implementation — see tsf/include/voice_input.h for the design
// notes (online provider, key handling, audio path).
#include "../include/voice_input.h"

#include <mmsystem.h>
#include <winhttp.h>

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <sstream>

namespace bangla_tsf {

namespace {

// ---------------------------------------------------------------- JSON bits

bool IsWs(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

// Reads a JSON string starting at the opening quote. Tolerant on purpose: the
// payloads here are small and fixed, and a malformed answer must never throw.
bool ReadJsonString(const std::string& s, size_t* pos, std::string& out) {
    if (*pos >= s.size() || s[*pos] != '"') return false;
    ++(*pos);
    out.clear();
    while (*pos < s.size()) {
        char c = s[*pos];
        if (c == '\\') {
            if (*pos + 1 >= s.size()) return false;
            char e = s[*pos + 1];
            *pos += 2;
            switch (e) {
                case 'n': out += '\n'; break;
                case 't': out += '\t'; break;
                case 'r': out += '\r'; break;
                case 'b': out += '\b'; break;
                case 'f': out += '\f'; break;
                case 'u': {
                    if (*pos + 4 > s.size()) return false;
                    unsigned cp = 0;
                    for (int i = 0; i < 4; ++i) {
                        char h = s[*pos + i];
                        unsigned d = (h >= '0' && h <= '9') ? (h - '0')
                                     : (h >= 'a' && h <= 'f') ? (h - 'a' + 10)
                                     : (h >= 'A' && h <= 'F') ? (h - 'A' + 10)
                                                              : 0;
                        cp = (cp << 4) | d;
                    }
                    *pos += 4;
                    // Encode as UTF-8 (BMP only — enough for Bengali).
                    if (cp < 0x80) {
                        out += static_cast<char>(cp);
                    } else if (cp < 0x800) {
                        out += static_cast<char>(0xC0 | (cp >> 6));
                        out += static_cast<char>(0x80 | (cp & 0x3F));
                    } else {
                        out += static_cast<char>(0xE0 | (cp >> 12));
                        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
                        out += static_cast<char>(0x80 | (cp & 0x3F));
                    }
                    break;
                }
                default: out += e; break;  // \" \\ \/ …
            }
            continue;
        }
        if (c == '"') {
            ++(*pos);
            return true;
        }
        out += c;
        ++(*pos);
    }
    return false;
}

// name":"value"  — returns false when the key is absent.
bool JsonFindRawString(const std::string& body, const std::string& key, std::string& out) {
    std::string needle = "\"" + key + "\"";
    size_t pos = body.find(needle);
    if (pos == std::string::npos) return false;
    pos = body.find(':', pos + needle.size());
    if (pos == std::string::npos) return false;
    ++pos;
    while (pos < body.size() && IsWs(body[pos])) ++pos;
    return ReadJsonString(body, &pos, out);
}

void PutLE16(std::vector<uint8_t>& v, uint16_t x) {
    v.push_back(static_cast<uint8_t>(x & 0xFF));
    v.push_back(static_cast<uint8_t>((x >> 8) & 0xFF));
}

void PutLE32(std::vector<uint8_t>& v, uint32_t x) {
    v.push_back(static_cast<uint8_t>(x & 0xFF));
    v.push_back(static_cast<uint8_t>((x >> 8) & 0xFF));
    v.push_back(static_cast<uint8_t>((x >> 16) & 0xFF));
    v.push_back(static_cast<uint8_t>((x >> 24) & 0xFF));
}

std::wstring Utf8ToWide(const std::string& s) {
    if (s.empty()) return std::wstring();
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    std::wstring w(n, L'\0');
    if (n > 0) MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), &w[0], n);
    return w;
}

// ------------------------------------------------------------- WinHTTP POST

bool HttpPost(const std::wstring& host, const std::wstring& path,
              const std::wstring& extra_headers, const std::string& body,
              std::string& response, std::wstring* error_out) {
    HINTERNET session = WinHttpOpen(L"Likhi/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                    WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session) {
        if (error_out) *error_out = L"WinHTTP init failed";
        return false;
    }
    DWORD ms = 30000;  // speech uploads are slower than a text lookup
    WinHttpSetOption(session, WINHTTP_OPTION_CONNECT_TIMEOUT, &ms, sizeof(ms));
    WinHttpSetOption(session, WINHTTP_OPTION_SEND_TIMEOUT, &ms, sizeof(ms));
    WinHttpSetOption(session, WINHTTP_OPTION_RECEIVE_TIMEOUT, &ms, sizeof(ms));

    HINTERNET connect = WinHttpConnect(session, host.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!connect) {
        WinHttpCloseHandle(session);
        if (error_out) *error_out = L"cannot reach the speech service";
        return false;
    }

    HINTERNET req = WinHttpOpenRequest(connect, L"POST", path.c_str(), nullptr,
                                       WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                       WINHTTP_FLAG_SECURE);
    if (!req) {
        WinHttpCloseHandle(connect);
        WinHttpCloseHandle(session);
        if (error_out) *error_out = L"request creation failed";
        return false;
    }

    BOOL ok = WinHttpSendRequest(
        req, extra_headers.empty() ? WINHTTP_NO_ADDITIONAL_HEADERS : extra_headers.c_str(),
        extra_headers.empty() ? 0 : static_cast<DWORD>(-1L),  // -1 => length from the string
        const_cast<char*>(body.data()), static_cast<DWORD>(body.size()),
        static_cast<DWORD>(body.size()), 0);
    if (ok) ok = WinHttpReceiveResponse(req, nullptr);

    response.clear();
    while (ok) {
        DWORD avail = 0;
        if (!WinHttpQueryDataAvailable(req, &avail) || avail == 0) break;
        char buf[8192];
        DWORD read = 0;
        DWORD want = (std::min)(avail, static_cast<DWORD>(sizeof(buf)));
        if (!WinHttpReadData(req, buf, want, &read) || read == 0) break;
        response.append(buf, read);
    }

    DWORD status = 0;
    DWORD len = sizeof(status);
    WinHttpQueryHeaders(req, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                        WINHTTP_HEADER_NAME_BY_INDEX, &status, &len, WINHTTP_NO_HEADER_INDEX);

    WinHttpCloseHandle(req);
    WinHttpCloseHandle(connect);
    WinHttpCloseHandle(session);

    if (!ok) {
        if (error_out) *error_out = L"network error while uploading the recording";
        return false;
    }
    if (status != 200) {
        if (error_out) {
            *error_out = L"speech service returned HTTP " + std::to_wstring(status);
            // Surface the provider's message (e.g. "invalid api key") when present.
            std::string msg;
            if (JsonFindRawString(response, "message", msg) && !msg.empty()) {
                *error_out += L": " + Utf8ToWide(msg);
            }
        }
        return false;
    }
    return true;
}

}  // namespace

// =====================================================================
// pure helpers
// =====================================================================

VoiceConfig VoiceConfig_ParseJson(const std::string& text) {
    VoiceConfig cfg;
    std::string v;
    if (JsonFindRawString(text, "provider", v) && !v.empty()) cfg.provider = v;
    if (JsonFindRawString(text, "api_key", v)) cfg.api_key = v;
    if (JsonFindRawString(text, "model", v) && !v.empty()) cfg.model = v;
    if (JsonFindRawString(text, "language", v) && !v.empty()) cfg.language = v;
    // "apiKey" is accepted too: hand-written files tend to use camelCase.
    if (cfg.api_key.empty() && JsonFindRawString(text, "apiKey", v)) cfg.api_key = v;
    std::transform(cfg.provider.begin(), cfg.provider.end(), cfg.provider.begin(),
                   [](unsigned char c) { return static_cast<char>(::tolower(c)); });
    return cfg;
}

std::vector<uint8_t> Voice_BuildWav(const std::vector<int16_t>& pcm, uint32_t sample_rate) {
    const uint32_t data_bytes = static_cast<uint32_t>(pcm.size() * sizeof(int16_t));
    std::vector<uint8_t> w;
    w.reserve(44 + data_bytes);

    const char* riff = "RIFF";
    w.insert(w.end(), riff, riff + 4);
    PutLE32(w, 36 + data_bytes);
    const char* wave = "WAVE";
    w.insert(w.end(), wave, wave + 4);
    const char* fmt = "fmt ";
    w.insert(w.end(), fmt, fmt + 4);
    PutLE32(w, 16);                                   // PCM chunk size
    PutLE16(w, 1);                                    // WAVE_FORMAT_PCM
    PutLE16(w, 1);                                    // mono
    PutLE32(w, sample_rate);
    PutLE32(w, sample_rate * 2);                      // byte rate (mono, 16-bit)
    PutLE16(w, 2);                                    // block align
    PutLE16(w, 16);                                   // bits per sample
    const char* data = "data";
    w.insert(w.end(), data, data + 4);
    PutLE32(w, data_bytes);
    for (int16_t s : pcm) {
        w.push_back(static_cast<uint8_t>(s & 0xFF));
        w.push_back(static_cast<uint8_t>((s >> 8) & 0xFF));
    }
    return w;
}

std::string Voice_Base64Encode(const uint8_t* data, size_t len) {
    static const char* tbl = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve(((len + 2) / 3) * 4);
    size_t i = 0;
    while (i + 2 < len) {
        uint32_t n = (data[i] << 16) | (data[i + 1] << 8) | data[i + 2];
        out += tbl[(n >> 18) & 63];
        out += tbl[(n >> 12) & 63];
        out += tbl[(n >> 6) & 63];
        out += tbl[n & 63];
        i += 3;
    }
    if (i + 1 == len) {
        uint32_t n = data[i] << 16;
        out += tbl[(n >> 18) & 63];
        out += tbl[(n >> 12) & 63];
        out += "==";
    } else if (i + 2 == len) {
        uint32_t n = (data[i] << 16) | (data[i + 1] << 8);
        out += tbl[(n >> 18) & 63];
        out += tbl[(n >> 12) & 63];
        out += tbl[(n >> 6) & 63];
        out += '=';
    }
    return out;
}

bool Voice_ParseTranscript(const std::string& provider, const std::string& body,
                           std::string& out_utf8) {
    out_utf8.clear();
    if (body.empty()) return false;

    std::string text;
    if (provider == "google") {
        // {"results":[{"alternatives":[{"transcript":"...","confidence":0.9}]}]}
        if (!JsonFindRawString(body, "transcript", text)) return false;
    } else {
        // OpenAI: {"text":"..."}   (also used by gpt-4o-transcribe)
        if (!JsonFindRawString(body, "text", text)) return false;
    }
    if (text.empty()) return false;
    out_utf8 = text;
    return true;
}

// =====================================================================
// config on disk
// =====================================================================

static std::wstring VoiceConfigPath(const std::wstring& appdata_dir) {
    std::wstring dir = appdata_dir;
    if (!dir.empty() && dir.back() != L'\\') dir += L'\\';
    return dir + L"PC-Bangla-Typing-App\\voice.json";
}

VoiceConfig VoiceConfig_Load(const std::wstring& appdata_dir) {
    std::ifstream in(VoiceConfigPath(appdata_dir).c_str(), std::ios::binary);
    if (!in.is_open()) return VoiceConfig();  // no file -> disabled (empty key)
    std::ostringstream ss;
    ss << in.rdbuf();
    return VoiceConfig_ParseJson(ss.str());
}

bool VoiceConfig_Save(const std::wstring& appdata_dir, const VoiceConfig& cfg) {
    std::wstring path = VoiceConfigPath(appdata_dir);
    std::wstring dir = path.substr(0, path.find_last_of(L'\\'));
    CreateDirectoryW(dir.c_str(), nullptr);
    std::ofstream out(path.c_str(), std::ios::binary | std::ios::trunc);
    if (!out.is_open()) return false;
    out << "{\n"
        << "  \"provider\": \"" << cfg.provider << "\",\n"
        << "  \"api_key\": \"" << cfg.api_key << "\",\n"
        << "  \"model\": \"" << cfg.model << "\",\n"
        << "  \"language\": \"" << cfg.language << "\"\n"
        << "}\n";
    return out.good();
}

// =====================================================================
// recorder
// =====================================================================

VoiceRecorder::~VoiceRecorder() {
    if (hwave_) Stop();
}

bool VoiceRecorder::Start() {
    std::lock_guard<std::mutex> lk(mu_);
    if (recording_) return true;
    if (hwave_) {
        waveInClose(hwave_);
        hwave_ = nullptr;
    }
    pcm_.clear();
    error_.clear();
    stopping_ = false;
    full_ = false;

    WAVEFORMATEX fmt = {};
    fmt.wFormatTag = WAVE_FORMAT_PCM;
    fmt.nChannels = 1;
    fmt.nSamplesPerSec = kSampleRate;
    fmt.wBitsPerSample = 16;
    fmt.nBlockAlign = static_cast<WORD>(fmt.nChannels * fmt.wBitsPerSample / 8);
    fmt.nAvgBytesPerSec = fmt.nSamplesPerSec * fmt.nBlockAlign;
    fmt.cbSize = 0;

    MMRESULT r = waveInOpen(&hwave_, WAVE_MAPPER, &fmt,
                            reinterpret_cast<DWORD_PTR>(&VoiceRecorder::WaveInProc),
                            reinterpret_cast<DWORD_PTR>(this), CALLBACK_FUNCTION);
    if (r != MMSYSERR_NOERROR || !hwave_) {
        hwave_ = nullptr;
        error_ = L"microphone could not be opened (is one connected?)";
        return false;
    }

    for (int i = 0; i < kBuffers; ++i) {
        storage_[i].assign(kBufferSamples, 0);
        WAVEHDR& h = hdrs_[i];
        ZeroMemory(&h, sizeof(h));
        h.lpData = reinterpret_cast<LPSTR>(storage_[i].data());
        h.dwBufferLength = static_cast<DWORD>(kBufferSamples * sizeof(int16_t));
        if (waveInPrepareHeader(hwave_, &h, sizeof(h)) != MMSYSERR_NOERROR ||
            waveInAddBuffer(hwave_, &h, sizeof(h)) != MMSYSERR_NOERROR) {
            error_ = L"microphone buffer setup failed";
            for (int j = 0; j <= i; ++j) waveInUnprepareHeader(hwave_, &hdrs_[j], sizeof(WAVEHDR));
            waveInClose(hwave_);
            hwave_ = nullptr;
            return false;
        }
    }

    if (waveInStart(hwave_) != MMSYSERR_NOERROR) {
        error_ = L"recording could not start";
        for (int i = 0; i < kBuffers; ++i) waveInUnprepareHeader(hwave_, &hdrs_[i], sizeof(WAVEHDR));
        waveInClose(hwave_);
        hwave_ = nullptr;
        return false;
    }

    recording_ = true;
    return true;
}

void CALLBACK VoiceRecorder::WaveInProc(HWAVEIN, UINT msg, DWORD_PTR instance,
                                       DWORD_PTR param1, DWORD_PTR) {
    if (msg != WIM_DATA) return;
    VoiceRecorder* self = reinterpret_cast<VoiceRecorder*>(instance);
    if (!self) return;
    self->OnBuffer(reinterpret_cast<WAVEHDR*>(param1));
}

void VoiceRecorder::OnBuffer(WAVEHDR* hdr) {
    std::lock_guard<std::mutex> lk(mu_);
    if (!hdr) return;

    if (hdr->dwBytesRecorded > 0 && !full_) {
        size_t n = hdr->dwBytesRecorded / sizeof(int16_t);
        if (pcm_.size() + n > kMaxSamples) {
            n = (pcm_.size() < kMaxSamples) ? (kMaxSamples - pcm_.size()) : 0;
            full_ = true;
        }
        if (n > 0) {
            const int16_t* src = reinterpret_cast<const int16_t*>(hdr->lpData);
            pcm_.insert(pcm_.end(), src, src + n);
        }
    }

    if (!stopping_ && hwave_ && !full_) {
        waveInAddBuffer(hwave_, hdr, sizeof(WAVEHDR));
    }
}

std::vector<int16_t> VoiceRecorder::Stop() {
    HWAVEIN h = nullptr;
    {
        std::lock_guard<std::mutex> lk(mu_);
        if (!hwave_) {
            recording_ = false;
            return pcm_;
        }
        stopping_ = true;
        h = hwave_;
    }

    waveInStop(h);
    waveInReset(h);  // drains pending buffers (their WIM_DATA callbacks run now)
    for (int i = 0; i < kBuffers; ++i) waveInUnprepareHeader(h, &hdrs_[i], sizeof(WAVEHDR));
    waveInClose(h);

    std::lock_guard<std::mutex> lk(mu_);
    hwave_ = nullptr;
    recording_ = false;
    stopping_ = false;
    return pcm_;
}

bool VoiceRecorder::IsRecording() const {
    std::lock_guard<std::mutex> lk(mu_);
    return recording_;
}

size_t VoiceRecorder::SampleCount() const {
    std::lock_guard<std::mutex> lk(mu_);
    return pcm_.size();
}

std::wstring VoiceRecorder::LastError() const {
    std::lock_guard<std::mutex> lk(mu_);
    return error_;
}

static std::string VoiceTrim(const std::string& s) {
    size_t b = 0, e = s.size();
    while (b < e && (s[b] == ' ' || s[b] == '\t' || s[b] == '\r' || s[b] == '\n')) ++b;
    while (e > b && (s[e - 1] == ' ' || s[e - 1] == '\t' || s[e - 1] == '\r' || s[e - 1] == '\n')) --e;
    return s.substr(b, e - b);
}

// Same as JsonFindRawString but starts looking at `from` — the Gemini answer
// contains several objects and we only want the text inside candidates[].
static bool JsonFindRawStringAfter(const std::string& body, const std::string& key,
                                   size_t from, std::string& out) {
    out.clear();
    std::string needle = "\"" + key + "\"";
    size_t pos = body.find(needle, from);
    if (pos == std::string::npos) return false;
    pos = body.find(':', pos + needle.size());
    if (pos == std::string::npos) return false;
    ++pos;
    while (pos < body.size() && (body[pos] == ' ' || body[pos] == '\t' ||
                                 body[pos] == '\r' || body[pos] == '\n')) {
        ++pos;
    }
    if (pos >= body.size() || body[pos] != '"') return false;
    ++pos;
    std::string s;
    while (pos < body.size()) {
        char c = body[pos++];
        if (c == '\\' && pos < body.size()) {
            char esc = body[pos++];
            switch (esc) {
                case 'n': s += '\n'; break;
                case 't': s += '\t'; break;
                case 'r': s += '\r'; break;
                case 'b': s += '\b'; break;
                case 'f': s += '\f'; break;
                case 'u': {
                    if (pos + 4 > body.size()) return false;
                    unsigned cp = 0;
                    for (int i = 0; i < 4; ++i) {
                        char h = body[pos + i];
                        cp <<= 4;
                        if (h >= '0' && h <= '9') cp |= static_cast<unsigned>(h - '0');
                        else if (h >= 'a' && h <= 'f') cp |= static_cast<unsigned>(h - 'a' + 10);
                        else if (h >= 'A' && h <= 'F') cp |= static_cast<unsigned>(h - 'A' + 10);
                    }
                    pos += 4;
                    if (cp < 0x80) {
                        s += static_cast<char>(cp);
                    } else if (cp < 0x800) {
                        s += static_cast<char>(0xC0 | (cp >> 6));
                        s += static_cast<char>(0x80 | (cp & 0x3F));
                    } else {
                        s += static_cast<char>(0xE0 | (cp >> 12));
                        s += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
                        s += static_cast<char>(0x80 | (cp & 0x3F));
                    }
                    break;
                }
                default: s += esc; break;
            }
        } else if (c == '"') {
            out = s;
            return true;
        } else {
            s += c;
        }
    }
    return false;  // unterminated string
}

std::string Voice_ExtractFinalText(const std::string& text) {
    const std::string open_tag = "<FINAL_TEXT>";
    const std::string close_tag = "</FINAL_TEXT>";
    size_t b = text.find(open_tag);
    if (b != std::string::npos) {
        size_t e = text.find(close_tag, b + open_tag.size());
        if (e != std::string::npos) {
            return VoiceTrim(text.substr(b + open_tag.size(), e - b - open_tag.size()));
        }
    }
    return VoiceTrim(text);
}

bool Voice_ParseGeminiResponse(const std::string& body, std::string& out_utf8) {
    out_utf8.clear();
    if (body.empty()) return false;
    size_t from = body.find("\"candidates\"");
    if (from == std::string::npos) from = 0;
    std::string text;
    if (!JsonFindRawStringAfter(body, "text", from, text)) return false;
    text = Voice_ExtractFinalText(text);
    if (text.empty()) return false;
    out_utf8 = text;
    return true;
}

// Gemini (Google AI Studio): the audio is sent inline as base64 and the answer
// is read back from the <FINAL_TEXT> tag — the same contract the reference
// engine uses (bangla_extracted_engine/gemini_voice_stt.js).
std::string Voice_TranscribeGemini(const std::vector<int16_t>& pcm, const VoiceConfig& cfg,
                                   std::wstring* error_out) {
    if (cfg.model.empty()) {
        if (error_out) *error_out = L"no model configured";
        return std::string();
    }
    std::vector<uint8_t> wav = Voice_BuildWav(pcm, VoiceRecorder::kSampleRate);
    std::string b64 = Voice_Base64Encode(wav.data(), wav.size());
    if (b64.empty()) {
        if (error_out) *error_out = L"could not encode the audio";
        return std::string();
    }

    // ASCII only, no quotes/backslashes: safe to inline in the JSON request.
    const char* kPrompt =
        "You are a Bengali (Bangla) speech to text engine. Transcribe the audio "
        "exactly, in Bengali script (Unicode). Use Bengali punctuation such as "
        "the dari at the end of a sentence. Do not translate. Write personal "
        "names and common English words the way they are normally written in "
        "Bengali. Return ONLY the final sentence wrapped between <FINAL_TEXT> "
        "and </FINAL_TEXT> tags, with no extra explanation.";

    std::string body = "{\"contents\":[{\"parts\":[{\"text\":\"";
    body += kPrompt;
    body += "\"},{\"inlineData\":{\"mimeType\":\"audio/wav\",\"data\":\"";
    body += b64;
    body += "\"}}]}],\"generationConfig\":{\"temperature\":0.0,\"maxOutputTokens\":1024}}";

    std::wstring path = L"/v1beta/models/";
    path += std::wstring(cfg.model.begin(), cfg.model.end());
    path += L":generateContent?key=";
    path += std::wstring(cfg.api_key.begin(), cfg.api_key.end());

    std::string response;
    if (!HttpPost(L"generativelanguage.googleapis.com", path,
                  L"Content-Type: application/json\r\n", body, response, error_out)) {
        return std::string();
    }
    std::string text;
    if (!Voice_ParseGeminiResponse(response, text)) {
        if (error_out) *error_out = L"the provider returned no transcript";
        return std::string();
    }
    return text;
}

// =====================================================================
// transcribe
// =====================================================================

std::string Voice_Transcribe(const std::vector<int16_t>& pcm, const VoiceConfig& cfg,
                             std::wstring* error_out) {
    if (pcm.empty()) {
        if (error_out) *error_out = L"nothing was recorded";
        return std::string();
    }

    // Gemini first: it is the path the reference engine uses and the one the
    // default voice.json points at.
    if (cfg.provider == "gemini") {
        return Voice_TranscribeGemini(pcm, cfg, error_out);
    }
    if (!cfg.enabled()) {
        if (error_out) *error_out = L"no API key configured (voice.json)";
        return std::string();
    }

    std::vector<uint8_t> wav = Voice_BuildWav(pcm, VoiceRecorder::kSampleRate);
    std::string response;

    if (cfg.provider == "google") {
        std::string path = "/v1/speech:recognize?key=" + cfg.api_key;
        std::wstring pathw(path.begin(), path.end());
        std::string json = "{\"config\":{\"encoding\":\"LINEAR16\",\"sampleRateHertz\":16000,"
                           "\"languageCode\":\"" +
                           (cfg.language.empty() ? std::string("bn-BD") :
                                                   cfg.language + "-BD") +
                           "\"},\"audio\":{\"content\":\"" +
                           Voice_Base64Encode(wav.data(), wav.size()) + "\"}}";
        if (!HttpPost(L"speech.googleapis.com", pathw, L"Content-Type: application/json\r\n",
                      json, response, error_out)) {
            return std::string();
        }
    } else {
        const char* kBoundary = "----LikhiVoiceBoundary7d82";
        std::string body;
        body += "--";
        body += kBoundary;
        body += "\r\nContent-Disposition: form-data; name=\"file\"; filename=\"speech.wav\"\r\n";
        body += "Content-Type: audio/wav\r\n\r\n";
        body.append(reinterpret_cast<const char*>(wav.data()), wav.size());
        body += "\r\n--";
        body += kBoundary;
        body += "\r\nContent-Disposition: form-data; name=\"model\"\r\n\r\n";
        body += cfg.model.empty() ? "whisper-1" : cfg.model;
        body += "\r\n--";
        body += kBoundary;
        body += "\r\nContent-Disposition: form-data; name=\"language\"\r\n\r\n";
        body += cfg.language.empty() ? "bn" : cfg.language;
        body += "\r\n--";
        body += kBoundary;
        body += "--\r\n";

        std::wstring headers = L"Content-Type: multipart/form-data; boundary=";
        headers += std::wstring(kBoundary, kBoundary + strlen(kBoundary));
        headers += L"\r\nAuthorization: Bearer ";
        headers += std::wstring(cfg.api_key.begin(), cfg.api_key.end());
        headers += L"\r\n";

        if (!HttpPost(L"api.openai.com", L"/v1/audio/transcriptions", headers, body,
                      response, error_out)) {
            return std::string();
        }
    }

    std::string text;
    if (!Voice_ParseTranscript(cfg.provider, response, text)) {
        if (error_out) *error_out = L"the speech service returned no text";
        return std::string();
    }
    if (error_out) error_out->clear();
    return text;
}

}  // namespace bangla_tsf
