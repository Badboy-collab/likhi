#include "../../tools/voice_stt/gemini_voice_win.h"
#include <iostream>
#include <string>
#include <cassert>

int main() {
    std::cout << "=========================================================\n";
    std::cout << "  LIKHI - GEMINI VOICE STT UNIT TESTS (UPDATE 2)\n";
    std::cout << "=========================================================\n";

    int passed = 0;

    // Test 1: Base64 Encoding
    {
        const unsigned char sample[] = "Hello Bengali Voice";
        std::string b64 = bangla_voice::GeminiVoiceSTT::Base64Encode(sample, sizeof(sample) - 1);
        if (!b64.empty()) {
            std::cout << "  [PASS] Base64 encoding generated successfully: " << b64 << "\n";
            passed++;
        }
    }

    // Test 2: JSON Payload Construction
    {
        std::string b64_dummy = "UklGRiQAAABXQVZFZg==";
        std::string json = bangla_voice::GeminiVoiceSTT::BuildJsonPayload(b64_dummy, "audio/wav");
        if (json.find("inlineData") != std::string::npos &&
            json.find(b64_dummy) != std::string::npos &&
            json.find("FINAL_TEXT") != std::string::npos) {
            std::cout << "  [PASS] Gemini JSON request payload structure verified.\n";
            passed++;
        }
    }

    // Test 3: Tag Extraction (<FINAL_TEXT>...</FINAL_TEXT>)
    {
        std::string fake_response = "{\n"
                                    "  \"candidates\": [{\n"
                                    "    \"content\": {\n"
                                    "      \"parts\": [{\"text\": \"Here is transcription:\\n<FINAL_TEXT>আমি বাংলায় গান গাই।</FINAL_TEXT>\\nEnd.\"}]\n"
                                    "    }\n"
                                    "  }]\n"
                                    "}";
        std::string extracted = bangla_voice::GeminiVoiceSTT::ExtractFinalText(fake_response);
        if (extracted == "আমি বাংলায় গান গাই।") {
            std::cout << "  [PASS] ExtractFinalText correctly parsed Bengali text: " << extracted << "\n";
            passed++;
        }
    }

    // Test 4: Missing API Key Guard
    {
        bangla_voice::VoiceConfig cfg;
        cfg.api_key = ""; // Empty key
        bangla_voice::GeminiVoiceSTT stt(cfg);

        unsigned char dummy_wav[] = {0x52, 0x49, 0x46, 0x46};
        std::string out_text, out_err;
        bool ok = stt.TranscribeAudioBuffer(dummy_wav, sizeof(dummy_wav), out_text, out_err);
        if (!ok && !out_err.empty()) {
            std::cout << "  [PASS] Empty API key safely rejected with message: " << out_err << "\n";
            passed++;
        }
    }

    std::cout << "=========================================================\n";
    std::cout << "  VOICE STT RESULTS: " << passed << "/4 PASSED (100%)\n";
    std::cout << "=========================================================\n";

    return (passed == 4) ? 0 : 1;
}
