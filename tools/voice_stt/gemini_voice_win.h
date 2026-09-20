#ifndef GEMINI_VOICE_WIN_H
#define GEMINI_VOICE_WIN_H

#include <string>
#include <vector>
#include <functional>

namespace bangla_voice {

struct VoiceConfig {
    std::string api_key;
    std::string model; // default: "gemini-1.5-flash"
    float temperature; // default: 0.1f

    VoiceConfig()
        : model("gemini-1.5-flash"),
          temperature(0.1f) {}
};

class GeminiVoiceSTT {
public:
    explicit GeminiVoiceSTT(const VoiceConfig& config);
    ~GeminiVoiceSTT() = default;

    void SetApiKey(const std::string& key);
    const std::string& GetApiKey() const;

    /**
     * Build the REST JSON payload for Gemini generateContent with inline base64 audio.
     */
    static std::string BuildJsonPayload(const std::string& base64_audio, const std::string& mime_type = "audio/wav");

    /**
     * Parse the raw response from Gemini generateContent to extract the text inside <FINAL_TEXT>...</FINAL_TEXT>.
     */
    static std::string ExtractFinalText(const std::string& response_json);

    /**
     * Encode binary audio buffer into standard Base64 string.
     */
    static std::string Base64Encode(const unsigned char* data, size_t length);

    /**
     * Helper to transcribe raw PCM/WAV buffer via Gemini REST API.
     * @param wav_data Raw WAV file or audio buffer
     * @param wav_len Length in bytes
     * @param out_text Transcribed Bengali text output
     * @param out_error Detailed error message if failed
     * @return true on success, false on failure
     */
    bool TranscribeAudioBuffer(const unsigned char* wav_data, size_t wav_len, std::string& out_text, std::string& out_error);

private:
    VoiceConfig config_;
};

} // namespace bangla_voice

#endif // GEMINI_VOICE_WIN_H
