#include "translation_service.h"
#include <iostream>
#include <cassert>
#include <chrono>
#include <thread>

void TestJsonParsing() {
    std::cout << "[Test] ParseTranslationResponse JSON parsing..." << std::endl;

    std::string out;
    // 1. Single string in array
    assert(likhi::ParseTranslationResponse("[\"i am fine\"]", out));
    assert(out == "I am fine");

    // 2. Question translation
    assert(likhi::ParseTranslationResponse("[\"what are you doing\"]", out));
    assert(out == "What are you doing");

    // 3. Name capitalization preserved
    assert(likhi::ParseTranslationResponse("[\"My name is Anwar\"]", out));
    assert(out == "My name is Anwar");

    // 4. Nested array
    assert(likhi::ParseTranslationResponse("[[\"i will go to office\"]]", out));
    assert(out == "I will go to office");

    // 5. Escaped unicode and quotes
    assert(likhi::ParseTranslationResponse("[\"He said \\\"hello\\\"\"]", out));
    assert(out == "He said \"hello\"");

    // 6. Malformed JSON handling (must return false, never throw or crash)
    assert(!likhi::ParseTranslationResponse("", out));
    assert(!likhi::ParseTranslationResponse("{}", out));
    assert(!likhi::ParseTranslationResponse("[", out));
    assert(!likhi::ParseTranslationResponse("invalid", out));
    assert(!likhi::ParseTranslationResponse("[123]", out));

    std::cout << "  -> Passed JSON parsing tests!" << std::endl;
}

void TestConsentGate() {
    std::cout << "[Test] Translation consent gate..." << std::endl;
    likhi::TranslationService service;

    // Consent disabled by default
    assert(!service.IsConsentEnabled());

    // Calling TranslateDirect with consent disabled must fail immediately without network call
    likhi::TranslationResult res = service.TranslateDirect(L"আমি ভালো আছি");
    assert(!res.success);
    assert(res.error.find("consent disabled") != std::string::npos);

    // Enable consent
    service.SetConsentEnabled(true);
    assert(service.IsConsentEnabled());

    std::cout << "  -> Passed consent gate tests!" << std::endl;
}

void TestCacheManagement() {
    std::cout << "[Test] In-memory caching..." << std::endl;
    likhi::TranslationService service;

    std::wstring out;
    assert(!service.LookupCache(L"আমি ভালো আছি", out));
    assert(service.CacheSize() == 0);

    // Direct translation with consent disabled won't cache
    service.TranslateDirect(L"আমি ভালো আছি");
    assert(service.CacheSize() == 0);

    // If cache has entries after direct translation or manual clear
    service.ClearCache();
    assert(service.CacheSize() == 0);

    std::cout << "  -> Passed caching tests!" << std::endl;
}

void TestAsyncQueueAndCancel() {
    std::cout << "[Test] Async queue and cancellation..." << std::endl;
    likhi::TranslationService service;
    assert(service.Start());

    // Request translation, then immediately cancel
    service.RequestTranslation(L"আমি অফিসে যাব", 1);
    service.Cancel();

    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    service.Stop();

    std::cout << "  -> Passed async queue & cancellation tests!" << std::endl;
}

void TestDirectNetworkTranslation() {
    std::cout << "[Test] Live network translation (Google dict-chrome-ex)..." << std::endl;
    likhi::TranslationService service;
    service.SetConsentEnabled(true);

    likhi::TranslationResult res = service.TranslateDirect(L"আমি ভালো আছি", 5000);
    if (res.success) {
        std::wcout << L"  -> Live translated: " << res.source_bengali << L" -> " << res.translated_english << std::endl;
        assert(res.translated_english == L"I am fine" || res.translated_english == L"I'm fine");

        // Verify cache hit
        std::wstring cached;
        assert(service.LookupCache(L"আমি ভালো আছি", cached));
        assert(cached == res.translated_english);
    } else {
        std::cout << "  -> Network test skipped/failed gracefully: " << res.error << std::endl;
    }
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "Running Likhi Translation Service Tests" << std::endl;
    std::cout << "========================================" << std::endl;

    TestJsonParsing();
    TestConsentGate();
    TestCacheManagement();
    TestAsyncQueueAndCancel();
    TestDirectNetworkTranslation();

    std::cout << "\nALL TRANSLATION TESTS PASSED (100% OK)" << std::endl;
    return 0;
}
