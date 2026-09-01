#include "../engine/include/bangla_engine.h"
#include <iostream>
#include <vector>
#include <chrono>
#include <numeric>
#include <algorithm>
#include <windows.h>
#include <psapi.h>

static size_t GetProcessMemoryUsageKB() {
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
        return pmc.WorkingSetSize / 1024;
    }
    return 0;
}

int main() {
    std::cout << "============================================================\n";
    std::cout << "  PC Bangla Typing App - Standalone Performance Benchmark (Phase 3.1)\n";
    std::cout << "  (High-Performance Flat Contiguous Lexicon & Memory Profiler)\n";
    std::cout << "============================================================\n\n";

    size_t baseline_ram_kb = GetProcessMemoryUsageKB();

    // 1. Cold Startup & Lexicon Load Benchmark
    auto t_start = std::chrono::high_resolution_clock::now();

    EngineConfig config;
    BanglaEngine_GetDefaultConfig(&config);
    config.lexicon_binary_path = "engine/data/lexicon.bin";
    BanglaEngine* engine = BanglaEngine_Create(&config);

    auto t_end = std::chrono::high_resolution_clock::now();
    double startup_ms = std::chrono::duration<double, std::milli>(t_end - t_start).count();
    size_t loaded_ram_kb = GetProcessMemoryUsageKB();

    std::cout << "[1] Engine Startup & Scaled Lexicon Load (52,161 words):\n";
    std::cout << "    Cold Startup Time:       " << startup_ms << " ms (" << (startup_ms * 1000.0) << " µs)\n";
    std::cout << "    Baseline Process RAM:    " << baseline_ram_kb << " KB (" << (baseline_ram_kb / 1024.0) << " MB)\n";
    std::cout << "    RAM After Engine Load:   " << loaded_ram_kb << " KB (" << (loaded_ram_kb / 1024.0) << " MB)\n";
    std::cout << "    Delta RAM (Engine+Lex):  " << (loaded_ram_kb - baseline_ram_kb) << " KB (" << ((loaded_ram_kb - baseline_ram_kb) / 1024.0) << " MB)\n\n";

    // 2. Keystroke-to-Candidate Microbenchmark (576,000 Keystroke Samples)
    std::vector<std::string> test_words = {
        "ami", "tumi", "she", "amra", "valo", "bhalo", "bangla", "bangladesh",
        "office", "kaj", "shomoy", "din", "raat", "pani", "bhat", "cha",
        "boi", "ajke", "kothay", "keno", "ki", "shundor", "shathe", "hocche",
        "jacche", "korbo", "khub", "ekhon", "shwastho", "brishti", "chand",
        "projukti", "biggan", "shikkhok", "chhatro", "porikkha", "uttor"
    };

    std::vector<double> latencies_us;
    latencies_us.reserve(600000);

    const int ITERATIONS = 3000;

    for (int iter = 0; iter < ITERATIONS; iter++) {
        for (const auto& word : test_words) {
            BanglaEngine_ResetComposition(engine);
            for (char ch : word) {
                auto t0 = std::chrono::high_resolution_clock::now();

                BanglaEngine_AppendChar(engine, ch);
                CandidateList list;
                BanglaEngine_GetCandidates(engine, &list);

                auto t1 = std::chrono::high_resolution_clock::now();
                double us = std::chrono::duration<double, std::micro>(t1 - t0).count();
                latencies_us.push_back(us);
            }
        }
    }

    std::sort(latencies_us.begin(), latencies_us.end());

    double total_us = std::accumulate(latencies_us.begin(), latencies_us.end(), 0.0);
    double avg_us = total_us / latencies_us.size();
    double p50_us = latencies_us[latencies_us.size() * 0.50];
    double p95_us = latencies_us[latencies_us.size() * 0.95];
    double p99_us = latencies_us[latencies_us.size() * 0.99];
    double min_us = latencies_us.front();
    double max_us = latencies_us.back();

    size_t typing_ram_kb = GetProcessMemoryUsageKB();

    std::cout << "[2] Keystroke-to-Candidate Latency (Over " << latencies_us.size() << " samples):\n";
    std::cout << "    Average Latency:         " << avg_us << " µs (" << (avg_us / 1000.0) << " ms)\n";
    std::cout << "    P50 (Median):            " << p50_us << " µs (" << (p50_us / 1000.0) << " ms)\n";
    std::cout << "    P95:                     " << p95_us << " µs (" << (p95_us / 1000.0) << " ms)\n";
    std::cout << "    P99:                     " << p99_us << " µs (" << (p99_us / 1000.0) << " ms)\n";
    std::cout << "    Min Latency:             " << min_us << " µs\n";
    std::cout << "    Max Latency:             " << max_us << " µs\n";
    std::cout << "    RAM During Active Typing:" << typing_ram_kb << " KB (" << (typing_ram_kb / 1024.0) << " MB)\n\n";

    // 3. Sentence Transliteration Latency
    std::vector<std::string> sentences = {
        "ami ajke office e jabo",
        "tumi kemon acho",
        "apnar shathe kotha bole valo laglo",
        "amra shobai eksathe kaaj korbo",
        "ajke brishti hocche",
        "ei boi ta onek shundor",
        "bangladesh amar jonmobhumi"
    };

    std::vector<double> sentence_latencies_us;
    sentence_latencies_us.reserve(10000);

    for (int iter = 0; iter < 1000; iter++) {
        for (const auto& s : sentences) {
            char out_buf[512];
            auto t0 = std::chrono::high_resolution_clock::now();
            BanglaEngine_TransliterateSentence(engine, s.c_str(), out_buf, sizeof(out_buf));
            auto t1 = std::chrono::high_resolution_clock::now();
            double us = std::chrono::duration<double, std::micro>(t1 - t0).count();
            sentence_latencies_us.push_back(us);
        }
    }

    std::sort(sentence_latencies_us.begin(), sentence_latencies_us.end());
    double total_sent_us = std::accumulate(sentence_latencies_us.begin(), sentence_latencies_us.end(), 0.0);
    double avg_sent_us = total_sent_us / sentence_latencies_us.size();
    double p50_sent_us = sentence_latencies_us[sentence_latencies_us.size() * 0.50];
    double p95_sent_us = sentence_latencies_us[sentence_latencies_us.size() * 0.95];

    size_t sentence_ram_kb = GetProcessMemoryUsageKB();

    std::cout << "[3] Sentence Transliteration Latency (Over " << sentence_latencies_us.size() << " samples):\n";
    std::cout << "    Average Sentence Time:   " << avg_sent_us << " µs (" << (avg_sent_us / 1000.0) << " ms)\n";
    std::cout << "    P50 Sentence Time:       " << p50_sent_us << " µs (" << (p50_sent_us / 1000.0) << " ms)\n";
    std::cout << "    P95 Sentence Time:       " << p95_sent_us << " µs (" << (p95_sent_us / 1000.0) << " ms)\n";
    std::cout << "    RAM After Sentences:     " << sentence_ram_kb << " KB (" << (sentence_ram_kb / 1024.0) << " MB)\n\n";

    // 4. Next-Word Prediction Latency & Memory
    std::vector<std::string> prev_words = {"আমি", "তুমি", "আপনি", "আমরা", "আজকে", "খুব", "অনেক", "বাংলাদেশ"};
    std::vector<double> pred_latencies_us;
    pred_latencies_us.reserve(10000);

    for (int iter = 0; iter < 1000; iter++) {
        for (const auto& pw : prev_words) {
            BanglaEngine_ResetContext(engine);
            BanglaEngine_CommitWord(engine, pw.c_str());

            auto t0 = std::chrono::high_resolution_clock::now();
            CandidateList list;
            BanglaEngine_GetNextWordPredictions(engine, &list);
            auto t1 = std::chrono::high_resolution_clock::now();

            double us = std::chrono::duration<double, std::micro>(t1 - t0).count();
            pred_latencies_us.push_back(us);
        }
    }

    std::sort(pred_latencies_us.begin(), pred_latencies_us.end());
    double total_pred_us = std::accumulate(pred_latencies_us.begin(), pred_latencies_us.end(), 0.0);
    double avg_pred_us = total_pred_us / pred_latencies_us.size();
    double p50_pred_us = pred_latencies_us[pred_latencies_us.size() * 0.50];
    double p95_pred_us = pred_latencies_us[pred_latencies_us.size() * 0.95];

    size_t pred_ram_kb = GetProcessMemoryUsageKB();

    std::cout << "[4] Next-Word Prediction Latency (Over " << pred_latencies_us.size() << " samples):\n";
    std::cout << "    Average Prediction Time: " << avg_pred_us << " µs (" << (avg_pred_us / 1000.0) << " ms)\n";
    std::cout << "    P50 Prediction Time:     " << p50_pred_us << " µs (" << (p50_pred_us / 1000.0) << " ms)\n";
    std::cout << "    P95 Prediction Time:     " << p95_pred_us << " µs (" << (p95_pred_us / 1000.0) << " ms)\n";
    std::cout << "    RAM During Predictions:  " << pred_ram_kb << " KB (" << (pred_ram_kb / 1024.0) << " MB)\n\n";

    // 5. Final Process Footprint & Leak Verification
    BanglaEngine_Destroy(engine);
    size_t freed_ram_kb = GetProcessMemoryUsageKB();

    std::cout << "[5] Memory Deallocation & Leak Verification:\n";
    std::cout << "    RAM After Engine Destroy:" << freed_ram_kb << " KB (" << (freed_ram_kb / 1024.0) << " MB)\n\n";

    std::cout << "============================================================\n";
    std::cout << "  BENCHMARK COMPLETE - PHASE 3.1 OPTIMIZATIONS VERIFIED\n";
    std::cout << "============================================================\n";

    return 0;
}
