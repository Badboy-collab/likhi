#ifndef LIKHI_TRANSLATION_SERVICE_H
#define LIKHI_TRANSLATION_SERVICE_H

#include <windows.h>
#include <winhttp.h>
#include <string>
#include <vector>
#include <functional>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>
#include <map>
#include <chrono>

namespace likhi {

// Translation status and mode
enum class ConversionTarget {
    kBangla = 0,   // Standard Banglish -> Bangla typing
    kEnglish = 1   // Banglish -> English translation
};

struct TranslationResult {
    std::wstring source_bengali;
    std::wstring translated_english;
    bool success;
    std::string error;
};

// Pure helper function for parsing JSON response from Google dict-chrome-ex endpoint.
// Expected JSON payload: ["<translated text>", ...] or [["<translated text>", ...], ...]
bool ParseTranslationResponse(const std::string& json_body, std::string& out_english);

// Asynchronous Translation Service
// Features:
// 1. Threaded non-blocking HTTP requests via WinHTTP
// 2. Debouncing and cancellation
// 3. Thread-safe in-memory caching
// 4. Consent / Enabled flag gate (never calls network if consent is not granted)
// 5. Offline fallback (fails gracefully without throwing or blocking)
class TranslationService {
public:
    using Callback = std::function<void(const std::wstring& source, const std::wstring& translated, uint32_t generation)>;

    TranslationService();
    ~TranslationService();

    bool Start();
    void Stop();

    // Request translation of a Bengali text into English
    void RequestTranslation(const std::wstring& bengali_text, uint32_t generation);
    void Cancel();

    void SetCallback(Callback cb) { callback_ = std::move(cb); }
    void SetConsentEnabled(bool enabled) { consent_enabled_.store(enabled); }
    bool IsConsentEnabled() const { return consent_enabled_.load(); }

    // Direct synchronous lookup in cache only (0ms latency)
    bool LookupCache(const std::wstring& bengali_text, std::wstring& out_english);

    // Synchronous direct translation (useful for unit testing or worker tasks)
    TranslationResult TranslateDirect(const std::wstring& bengali_text, uint32_t timeout_ms = 4000);

    // Cache management
    void ClearCache();
    size_t CacheSize();

    static constexpr int kDebounceMs = 120;

private:
    void WorkerLoop();
    bool EnsureHttp();
    void CloseHttp();

    std::atomic<bool> stop_{false};
    std::atomic<bool> consent_enabled_{false};
    Callback callback_;

    std::mutex queue_mutex_;
    std::condition_variable cv_;
    std::wstring pending_text_;
    uint32_t pending_gen_{0};

    // Cache
    std::mutex cache_mutex_;
    std::map<std::wstring, std::wstring> cache_;

    // WinHTTP state (worker thread only)
    HINTERNET h_session_{nullptr};
    HINTERNET h_connect_{nullptr};
    std::thread worker_thread_;
};

} // namespace likhi

#endif // LIKHI_TRANSLATION_SERVICE_H
