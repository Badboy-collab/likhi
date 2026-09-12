#ifndef BANGLA_CLOUD_TRANSLIT_H
#define BANGLA_CLOUD_TRANSLIT_H

#include <windows.h>
#include <winhttp.h>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <functional>
#include <chrono>

namespace bangla_tsf {

// Parses a Google Input Tools cloud transliteration response body.
// Expected shape (the JS engine in bangla_extracted_engine uses the same API):
//   ["SUCCESS", [["<input_word>", ["<c1>","<c2>",...]], ...]]
// Fills `out` with the candidate UTF-8 strings of the first input word.
// Returns true only on "SUCCESS" with at least one candidate.
// A tiny tolerant parser is used on purpose: the payload is small and fixed.
bool CloudTranslit_ParseGoogleResponse(const std::string& body,
                                       std::vector<std::string>& out);

// Online Google Input Tools transliteration client for Likhi.
//
// Behaviour contract (mirrors what the extracted JS engine does, natively):
//  * A background worker thread performs the HTTP(S) round-trip, so typing
//    never blocks and the UI shows the LOCAL engine result at 0 ms.
//  * A single persistent WinHTTP session/connection is reused for the host
//    (keep-alive) — previously-per-request handshakes cost 400-800 ms, a
//    reused connection drops the steady-state latency to ~90 ms.
//  * 100 ms typing debounce: only the LATEST full word is queried; rapid
//    intermediate single-character snapshots never build a request queue.
//  * Failures (offline / slow network / HTTP error) are silent: no results
//    arrive and the caller simply keeps the local engine — automatic offline
//    fallback. A short cooldown avoids hammering a dead network.
class CloudTranslit {
public:
    using ResultCallback = std::function<void(const std::wstring& word,
                                              uint32_t generation,
                                              const std::vector<std::wstring>& candidates)>;

    CloudTranslit();
    ~CloudTranslit();

    // Starts the worker thread. Safe to call once.
    bool Start();
    // Stops and joins the worker thread, closes HTTP handles. Idempotent.
    void Stop();

    // Schedules a lookup for `word` tagged with `generation`. Only the newest
    // request survives the debounce window; older ones are dropped.
    void Request(const std::wstring& word, uint32_t generation);

    // Drops any pending (not yet sent) request. In-flight replies may still
    // arrive — callers filter them by generation/word.
    void Cancel();

    void SetResultCallback(ResultCallback cb) { result_cb_ = std::move(cb); }

    // Number of milliseconds a word must stop changing before it is queried.
    static constexpr int kDebounceMs = 100;

private:
    void WorkerMain();
    void QueryLoop();
    bool EnsureHttp();
    void CloseHttp();
    bool DoHttpRequest(const std::wstring& word, uint32_t generation);

    std::thread worker_;
    std::atomic<bool> stop_{false};
    std::mutex mu_;
    std::condition_variable cv_;
    std::wstring pending_word_;
    uint32_t pending_gen_ = 0;

    ResultCallback result_cb_;

    // Persistent WinHTTP handles: one session + one connection => keep-alive.
    HINTERNET h_session_ = nullptr;
    HINTERNET h_connect_ = nullptr;
    std::chrono::steady_clock::time_point offline_until_{};
};

} // namespace bangla_tsf

#endif // BANGLA_CLOUD_TRANSLIT_H
