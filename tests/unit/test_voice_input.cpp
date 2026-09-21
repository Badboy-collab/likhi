// Unit tests for the voice-input helpers — pure, no audio device, no network.
// Compile/link: g++ test_voice_input.cpp ../../tsf/src/voice_input.cpp -lwinmm -lwinhttp
#include "../../tsf/include/voice_input.h"

#include <iostream>
#include <string>
#include <vector>

using namespace bangla_tsf;

static int g_fail = 0;
static int g_total = 0;

static void Check(bool cond, const char* what) {
    g_total++;
    if (!cond) {
        g_fail++;
        std::cout << "  [FAIL] " << what << "\n";
    }
}

int main() {
    // ---------------------------------------------------------------- config
    {
        std::string text =
            "{\n"
            "  \"provider\": \"OpenAI\",\n"
            "  \"api_key\": \"sk-test-123\",\n"
            "  \"model\": \"whisper-1\",\n"
            "  \"language\": \"bn\"\n"
            "}\n";
        VoiceConfig cfg = VoiceConfig_ParseJson(text);
        Check(cfg.provider == "openai", "provider lowercased -> openai");
        Check(cfg.api_key == "sk-test-123", "api_key parsed");
        Check(cfg.model == "whisper-1", "model parsed");
        Check(cfg.language == "bn", "language parsed");
        Check(cfg.enabled(), "config with key is enabled");
    }
    {
        // Hand-written file using camelCase, no model/language -> defaults kept.
        VoiceConfig cfg = VoiceConfig_ParseJson("{\"apiKey\": \"abc\"}");
        Check(cfg.api_key == "abc", "camelCase apiKey accepted");
        Check(cfg.provider == "gemini", "provider defaults to gemini");
        Check(cfg.model == "gemini-3.6-flash", "model defaults to gemini-3.6-flash");
        Check(cfg.language == "bn", "language defaults to bn");
    }
    {
        VoiceConfig cfg = VoiceConfig_ParseJson("{\"provider\": \"google\"}");
        Check(!cfg.enabled(), "no key -> disabled");
        Check(cfg.provider == "google", "google provider parsed");
    }

    {
        // Exactly the shape release_package\Set-Voice-Key.ps1 writes: an extra
        // _help key, reordered keys, no BOM.
        const char* helper_file =
            "{\n  \"provider\":  \"gemini\",\n  \"language\":  \"bn-BD\",\n"
            "  \"_help\":  \"api_key: provider key. Empty = voice typing off.\",\n"
            "  \"model\":  \"gemini-3.6-flash\",\n  \"api_key\":  \"AIzaSyTEST\"\n}";
        VoiceConfig cfg = VoiceConfig_ParseJson(helper_file);
        Check(cfg.provider == "gemini", "gemini provider parsed from the helper file");
        Check(cfg.model == "gemini-3.6-flash", "gemini model parsed from the helper file");
        Check(cfg.api_key == "AIzaSyTEST", "api key parsed from the helper file");
        Check(cfg.enabled(), "helper file with a key enables voice");
    }

    // ------------------------------------------------------- Gemini response
    {
        std::string body =
            "{\"candidates\":[{\"content\":{\"parts\":[{\"text\":\"<FINAL_TEXT>\u0986\u09ae\u09bf \u09ad\u09be\u09b2\u09cb \u0986\u099b\u09bf\u0964</FINAL_TEXT>\"}],"
            "\"role\":\"model\"},\"finishReason\":\"STOP\"}],\"usageMetadata\":{\"promptTokenCount\":10}}";
        std::string out;
        Check(Voice_ParseGeminiResponse(body, out), "gemini response parsed");
        Check(out == "\u0986\u09ae\u09bf \u09ad\u09be\u09b2\u09cb \u0986\u099b\u09bf\u0964",
              "FINAL_TEXT unwrapped to Bengali");
    }
    {
        std::string body =
            "{\"candidates\":[{\"content\":{\"parts\":[{\"text\":\"\\u09b9\\u09cd\\u09af\\u09be\\u09b2\\u09cb\"}]}}]}";
        std::string out;
        Check(Voice_ParseGeminiResponse(body, out), "u-escaped gemini response parsed");
        Check(out == "\u09b9\u09cd\u09af\u09be\u09b2\u09cb", "u-escapes decoded to Bengali");
    }
    {
        std::string out;
        Check(!Voice_ParseGeminiResponse("{\"error\":{\"message\":\"API key not valid\"}}", out),
              "gemini error body rejected");
        Check(!Voice_ParseGeminiResponse(std::string(), out), "empty gemini body rejected");
    }
    {
        Check(Voice_ExtractFinalText("plain text") == "plain text",
              "missing FINAL_TEXT tag keeps the text");
        Check(Voice_ExtractFinalText("  <FINAL_TEXT> abc </FINAL_TEXT>  ") == "abc",
              "FINAL_TEXT payload trimmed");
        Check(Voice_ExtractFinalText("<FINAL_TEXT>unclosed") == "<FINAL_TEXT>unclosed",
              "unclosed tag keeps the text");
    }

    // ------------------------------------------------------------------- WAV
    {
        std::vector<int16_t> pcm = {0, 1, -1, 258};
        std::vector<uint8_t> wav = Voice_BuildWav(pcm, 16000);
        Check(wav.size() == 44 + pcm.size() * 2, "wav size = 44 + 2*n");
        Check(std::string(reinterpret_cast<char*>(&wav[0]), 4) == "RIFF", "RIFF magic");
        Check(std::string(reinterpret_cast<char*>(&wav[8]), 4) == "WAVE", "WAVE magic");
        Check(std::string(reinterpret_cast<char*>(&wav[12]), 4) == "fmt ", "fmt chunk");
        Check(std::string(reinterpret_cast<char*>(&wav[36]), 4) == "data", "data chunk");

        auto le32 = [&](size_t off) {
            return (uint32_t)wav[off] | ((uint32_t)wav[off + 1] << 8) |
                   ((uint32_t)wav[off + 2] << 16) | ((uint32_t)wav[off + 3] << 24);
        };
        auto le16 = [&](size_t off) { return (uint16_t)(wav[off] | (wav[off + 1] << 8)); };

        Check(le32(4) == 36 + pcm.size() * 2, "RIFF chunk size");
        Check(le32(16) == 16, "fmt chunk size = 16");
        Check(le16(20) == 1, "format = PCM");
        Check(le16(22) == 1, "channels = 1 (mono)");
        Check(le32(24) == 16000, "sample rate = 16000");
        Check(le32(28) == 32000, "byte rate = 32000");
        Check(le16(32) == 2, "block align = 2");
        Check(le16(34) == 16, "bits per sample = 16");
        Check(le32(40) == pcm.size() * 2, "data size");
        Check(le16(44) == 0 && le16(46) == 1, "first sample = 0,1 little endian");
        Check(le16(48) == 0xFFFF, "third sample = -1 two's complement");
    }
    {
        std::vector<uint8_t> empty = Voice_BuildWav({}, 16000);
        Check(empty.size() == 44, "empty pcm -> header only");
    }

    // ---------------------------------------------------------------- base64
    {
        auto enc = [](const std::string& s) {
            return Voice_Base64Encode(reinterpret_cast<const uint8_t*>(s.data()), s.size());
        };
        // RFC 4648 test vectors.
        Check(enc("") == "", "base64 empty");
        Check(enc("f") == "Zg==", "base64 f");
        Check(enc("fo") == "Zm8=", "base64 fo");
        Check(enc("foo") == "Zm9v", "base64 foo");
        Check(enc("foob") == "Zm9vYg==", "base64 foob");
        Check(enc("fooba") == "Zm9vYmE=", "base64 fooba");
        Check(enc("foobar") == "Zm9vYmFy", "base64 foobar");
        // Bengali UTF-8 payload stays a multiple of 4 chars.
        std::string bn = "\u0986\u09ae\u09be\u09b0";  // আমার
        Check(enc(bn).size() % 4 == 0, "base64 bengali padded to 4");
    }

    // ------------------------------------------------------------- transcript
    {
        std::string out;
        // OpenAI shape (also used by gpt-4o-transcribe).
        Check(Voice_ParseTranscript("openai", "{\"text\":\"amar\"}", out), "openai parse ok");
        Check(out == "amar", "openai text value");

        // Bengali via \u escapes must come back as UTF-8.
        Check(Voice_ParseTranscript("openai",
                                    "{\"text\":\"\\u0986\\u09ae\\u09be\\u09b0\"}", out),
              "openai u-escape parse ok");
        Check(out == "\u0986\u09ae\u09be\u09b0", "u-escape decoded to UTF-8 (আমার)");

        // Google shape.
        std::string google =
            "{\"results\":[{\"alternatives\":[{\"transcript\":\"kemon acho\","
            "\"confidence\":0.94}]}]}";
        Check(Voice_ParseTranscript("google", google, out), "google parse ok");
        Check(out == "kemon acho", "google transcript value");

        // Negative cases.
        Check(!Voice_ParseTranscript("openai", "", out), "empty body -> false");
        Check(!Voice_ParseTranscript("openai", "{\"error\":{\"message\":\"bad key\"}}", out),
              "error body -> false");
        Check(!Voice_ParseTranscript("google", "{\"results\":[]}", out),
              "empty google results -> false");
        Check(!Voice_ParseTranscript("openai", "{\"text\":\"\"}", out),
              "empty text -> false");
    }

    std::cout << "Total Passed: " << (g_total - g_fail) << "\n";
    std::cout << "Total Failed: " << g_fail << "\n";
    return g_fail == 0 ? 0 : 1;
}
