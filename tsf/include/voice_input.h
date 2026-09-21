#ifndef BANGLA_VOICE_INPUT_H
#define BANGLA_VOICE_INPUT_H

// ===========================================================================
// VOICE INPUT (speech -> text) — decision 2026-09-15 (user-approved)
//
// An ONLINE api is used on purpose: Windows' own speech engine has no Bangla
// recognizer, so offline recognition could not do what the user asked for.
// This file shares the network boundary described in cloud_translit.h:
// WinHTTP -> the configured provider, linked into bangla_tsf.dll.
//
// The API key is read from
//     %APPDATA%\PC-Bangla-Typing-App\voice.json
// and is never written to the registry, never logged, and never uploaded
// anywhere except the provider's own endpoint: as an Authorization header
// (OpenAI / Google Speech) or the provider's documented `key=` query parameter
// (Gemini / Google AI Studio).
//
// Audio path: winmm (waveIn) -> 16 kHz mono PCM16 -> canonical WAV in memory
// -> single HTTPS POST -> recognized UTF-8 text.
// ===========================================================================

#include <windows.h>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace bangla_tsf {

struct VoiceConfig {
    // "gemini" (Google AI Studio, the reference-engine path) | "openai" | "google"
    std::string provider = "gemini";
    std::string api_key;                     // empty -> voice input disabled
    std::string model = "gemini-3.6-flash";  // provider model id
    std::string language = "bn";             // recognition hint

    bool enabled() const { return !api_key.empty(); }
};

// ---- pure helpers (no I/O, no network, no audio) — unit tested -------------
VoiceConfig VoiceConfig_ParseJson(const std::string& text);
std::vector<uint8_t> Voice_BuildWav(const std::vector<int16_t>& pcm, uint32_t sample_rate);
std::string Voice_Base64Encode(const uint8_t* data, size_t len);
bool Voice_ParseTranscript(const std::string& provider, const std::string& body,
                           std::string& out_utf8);
// Gemini (generateContent) answer -> plain transcript. Reads the first text
// part of candidates[].content.parts[] and unwraps the <FINAL_TEXT> tag.
bool Voice_ParseGeminiResponse(const std::string& body, std::string& out_utf8);
// Unwraps <FINAL_TEXT>...</FINAL_TEXT>; without the tag the (trimmed) input is
// returned unchanged, exactly like the reference engine.
std::string Voice_ExtractFinalText(const std::string& text);

// ---- config on disk --------------------------------------------------------
VoiceConfig VoiceConfig_Load(const std::wstring& appdata_dir);
bool VoiceConfig_Save(const std::wstring& appdata_dir, const VoiceConfig& cfg);

// ---- microphone recorder (winmm waveIn, 16 kHz mono PCM16) -----------------
class VoiceRecorder {
public:
    static constexpr uint32_t kSampleRate = 16000;
    static constexpr size_t kMaxSamples = kSampleRate * 60;  // 60 s hard stop

    ~VoiceRecorder();

    bool Start();
    // Stops capture and returns everything recorded so far.
    std::vector<int16_t> Stop();
    bool IsRecording() const;
    std::wstring LastError() const;

    // Number of samples captured so far (UI can show a live duration).
    size_t SampleCount() const;

private:
    static void CALLBACK WaveInProc(HWAVEIN hwi, UINT msg, DWORD_PTR instance,
                                    DWORD_PTR param1, DWORD_PTR param2);
    void OnBuffer(WAVEHDR* hdr);

    static const int kBuffers = 8;
    static const int kBufferSamples = 1600;  // 100 ms per buffer

    HWAVEIN hwave_ = nullptr;
    WAVEHDR hdrs_[kBuffers];
    std::vector<int16_t> storage_[kBuffers];
    std::vector<int16_t> pcm_;
    bool recording_ = false;
    bool stopping_ = false;
    bool full_ = false;
    std::wstring error_;
    mutable std::mutex mu_;
};

// One-shot: uploads the PCM and returns the recognized UTF-8 text.
// Empty string on any failure; `error_out` receives a human-readable reason.
std::string Voice_Transcribe(const std::vector<int16_t>& pcm, const VoiceConfig& cfg,
                             std::wstring* error_out = nullptr);

}  // namespace bangla_tsf

#endif  // BANGLA_VOICE_INPUT_H
