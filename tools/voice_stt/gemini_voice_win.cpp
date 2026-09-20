#include "gemini_voice_win.h"

#ifdef _WIN32
#include <windows.h>
#include <wininet.h>
#endif

#include <sstream>
#include <regex>

namespace bangla_voice {

GeminiVoiceSTT::GeminiVoiceSTT(const VoiceConfig& config)
    : config_(config) {
}

void GeminiVoiceSTT::SetApiKey(const std::string& key) {
    config_.api_key = key;
}

const std::string& GeminiVoiceSTT::GetApiKey() const {
    return config_.api_key;
}

std::string GeminiVoiceSTT::Base64Encode(const unsigned char* data, size_t length) {
    static const char base64_chars[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789+/";

    std::string ret;
    int i = 0;
    int j = 0;
    unsigned char char_array_3[3];
    unsigned char char_array_4[4];

    while (length--) {
        char_array_3[i++] = *(data++);
        if (i == 3) {
            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
            char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
            char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
            char_array_4[3] = char_array_3[2] & 0x3f;

            for (i = 0; i < 4; i++) {
                ret += base64_chars[char_array_4[i]];
            }
            i = 0;
        }
    }

    if (i) {
        for (j = i; j < 3; j++) {
            char_array_3[j] = '\0';
        }

        char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
        char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
        char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
        char_array_4[3] = char_array_3[2] & 0x3f;

        for (j = 0; j < i + 1; j++) {
            ret += base64_chars[char_array_4[j]];
        }

        while ((i++ < 3)) {
            ret += '=';
        }
    }

    return ret;
}

std::string GeminiVoiceSTT::BuildJsonPayload(const std::string& base64_audio, const std::string& mime_type) {
    std::ostringstream oss;
    oss << "{\n";
    oss << "  \"contents\": [{\n";
    oss << "    \"role\": \"user\",\n";
    oss << "    \"parts\": [\n";
    oss << "      {\"text\": \"You are an expert Bengali speech-to-text transcription engine. Transcribe the spoken audio into accurate, natural, and punctuated Bengali Unicode text. Ensure correct spelling, consonant conjuncts, and sentence endings with Bengali Dari. Important: Output the final transcription inside <FINAL_TEXT>...</FINAL_TEXT> tags only without additional commentary.\"},\n";
    oss << "      {\n";
    oss << "        \"inlineData\": {\n";
    oss << "          \"mimeType\": \"" << mime_type << "\",\n";
    oss << "          \"data\": \"" << base64_audio << "\"\n";
    oss << "        }\n";
    oss << "      }\n";
    oss << "    ]\n";
    oss << "  }],\n";
    oss << "  \"generationConfig\": {\n";
    oss << "    \"temperature\": 0.1,\n";
    oss << "    \"maxOutputTokens\": 1024\n";
    oss << "  }\n";
    oss << "}";
    return oss.str();
}

std::string GeminiVoiceSTT::ExtractFinalText(const std::string& response_json) {
    // Look for <FINAL_TEXT>(.*?)</FINAL_TEXT>
    std::regex final_tag_regex("<FINAL_TEXT>([\\s\\S]*?)</FINAL_TEXT>");
    std::smatch match;
    if (std::regex_search(response_json, match, final_tag_regex) && match.size() > 1) {
        return match[1].str();
    }

    // Fallback: search for "text": "..."
    std::regex text_field_regex("\"text\"\\s*:\\s*\"([^\"]+)\"");
    if (std::regex_search(response_json, match, text_field_regex) && match.size() > 1) {
        std::string raw = match[1].str();
        // Remove literal \n or escaped characters
        return raw;
    }

    return response_json;
}

bool GeminiVoiceSTT::TranscribeAudioBuffer(const unsigned char* wav_data, size_t wav_len, std::string& out_text, std::string& out_error) {
    if (config_.api_key.empty()) {
        out_error = "Gemini API key is not configured.";
        return false;
    }

    if (!wav_data || wav_len == 0) {
        out_error = "Audio buffer is empty.";
        return false;
    }

    std::string base64_audio = Base64Encode(wav_data, wav_len);
    std::string json_body = BuildJsonPayload(base64_audio, "audio/wav");

#ifdef _WIN32
    HINTERNET hInternet = InternetOpenA("Likhi-Voice-Engine/1.0", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
    if (!hInternet) {
        out_error = "Failed to initialize WinINet.";
        return false;
    }

    HINTERNET hConnect = InternetConnectA(hInternet, "generativelanguage.googleapis.com",
                                          INTERNET_DEFAULT_HTTPS_PORT, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);
    if (!hConnect) {
        InternetCloseHandle(hInternet);
        out_error = "Failed to connect to Google API host.";
        return false;
    }

    std::string path = "/v1beta/models/" + config_.model + ":generateContent?key=" + config_.api_key;
    HINTERNET hRequest = HttpOpenRequestA(hConnect, "POST", path.c_str(), NULL, NULL, NULL,
                                         INTERNET_FLAG_SECURE | INTERNET_FLAG_RELOAD, 0);
    if (!hRequest) {
        InternetCloseHandle(hConnect);
        InternetCloseHandle(hInternet);
        out_error = "Failed to create HTTP request.";
        return false;
    }

    std::string headers = "Content-Type: application/json\r\n";
    BOOL sent = HttpSendRequestA(hRequest, headers.c_str(), (DWORD)headers.length(),
                                 (LPVOID)json_body.c_str(), (DWORD)json_body.length());
    if (!sent) {
        InternetCloseHandle(hRequest);
        InternetCloseHandle(hConnect);
        InternetCloseHandle(hInternet);
        out_error = "Failed to send HTTP request.";
        return false;
    }

    std::string response_data;
    char buffer[4096];
    DWORD bytes_read = 0;
    while (InternetReadFile(hRequest, buffer, sizeof(buffer) - 1, &bytes_read) && bytes_read > 0) {
        buffer[bytes_read] = '\0';
        response_data.append(buffer, bytes_read);
    }

    InternetCloseHandle(hRequest);
    InternetCloseHandle(hConnect);
    InternetCloseHandle(hInternet);

    out_text = ExtractFinalText(response_data);
    return true;
#else
    out_error = "WinINet is only supported on Windows.";
    return false;
#endif
}

} // namespace bangla_voice
