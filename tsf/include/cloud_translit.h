#ifndef BANGLA_CLOUD_TRANSLIT_H
#define BANGLA_CLOUD_TRANSLIT_H

// ===========================================================================
// NETWORK BOUNDARY NOTE (decision 2026-09-14, user-approved)
//
// This is the ONLY code in the tree that talks to the network
// (WinHTTP -> inputtools.google.com). It IS intentionally linked into
// bangla_tsf.dll again, because the online suggestions are a wanted feature.
// Consequence: bangla_tsf.dll imports WINHTTP.dll.
//
// The network-isolation gate (tools/check_network_isolation.cmake) therefore
// runs in WARNING mode and prints any network import on every link, so
// re-isolating the IME later (moving this file into likhi_sync.exe with
// filesystem staging only - no pipes, no COM IPC, no window messages) is a
// one-line switch back to FATAL (LIKHI_ALLOW_NETWORK=0).
// ===========================================================================

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
#include <map>

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
    bool HttpGet(const std::wstring& path, std::string& body);

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

    // Timestamp of the last successful request. A socket idle longer than a
    // few seconds is closed by the server, so the next request on it fails;
    // we reopen proactively after kFreshAfter to avoid that ("sometimes no
    // Google suggestion").
    static constexpr int kFreshAfterSeconds = 20;
    std::chrono::steady_clock::time_point fresh_after_{};

    // Word -> candidates cache (worker thread only). A repeated word answers
    // instantly instead of paying another HTTPS round trip - this is what makes
    // the Google suggestions show up reliably while typing.
    static constexpr size_t kCacheMax = 256;
    std::map<std::wstring, std::vector<std::wstring>> cache_;
    void CacheStore(const std::wstring& word, const std::vector<std::wstring>& cands);

    // True only for the connection warm-up call, whose result must never be
    // published as a suggestion.
    bool warming_ = false;
};

} // namespace bangla_tsf

#endif // BANGLA_CLOUD_TRANSLIT_H
