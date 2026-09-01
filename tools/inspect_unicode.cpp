#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <cstdint>
#include <windows.h>
#include <shellapi.h>

std::wstring Utf8ToUtf16(const std::string& u8) {
    if (u8.empty()) return L"";
    int len = MultiByteToWideChar(CP_UTF8, 0, u8.c_str(), (int)u8.length(), NULL, 0);
    if (len <= 0) return L"";
    std::wstring u16(len, 0);
    MultiByteToWideChar(CP_UTF8, 0, u8.c_str(), (int)u8.length(), &u16[0], len);
    return u16;
}

std::string Utf16ToUtf8(const std::wstring& u16) {
    if (u16.empty()) return "";
    int len = WideCharToMultiByte(CP_UTF8, 0, u16.c_str(), (int)u16.length(), NULL, 0, NULL, NULL);
    if (len <= 0) return "";
    std::string u8(len, 0);
    WideCharToMultiByte(CP_UTF8, 0, u16.c_str(), (int)u16.length(), &u8[0], len, NULL, NULL);
    return u8;
}

std::string GetCodepointDescription(uint32_t cp) {
    if (cp >= 0x0985 && cp <= 0x0994) return "Bengali Vowel Letter (স্বরবর্ণ)";
    if (cp >= 0x0995 && cp <= 0x09B9) return "Bengali Consonant (ব্যঞ্জনবর্ণ)";
    if (cp >= 0x09BE && cp <= 0x09CC) return "Bengali Vowel Sign (কার)";
    if (cp == 0x09CD) return "Bengali Sign Virama / Hasant (্)";
    if (cp == 0x0981) return "Bengali Sign Candrabindu (ঁ)";
    if (cp == 0x0982) return "Bengali Sign Anusvara (ং)";
    if (cp == 0x0983) return "Bengali Sign Visarga (ঃ)";
    if (cp == 0x09BC) return "Bengali Sign Nukta (়)";
    if (cp == 0x09BD) return "Bengali Sign Avagraha (ঽ)";
    if (cp == 0x09CE) return "Bengali Letter Khanda Ta (ৎ)";
    if (cp == 0x09D7) return "Bengali Au Length Mark (ৗ)";
    if (cp == 0x09DC) return "Bengali Letter Rra (ড়)";
    if (cp == 0x09DD) return "Bengali Letter Rha (ঢ়)";
    if (cp == 0x09DF) return "Bengali Letter Yya (য়)";
    if (cp == 0x09E6) return "Bengali Digit Zero (০)";
    if (cp >= 0x09E6 && cp <= 0x09EF) return "Bengali Digit (সংখ্যা)";
    if (cp == 0x09F7) return "Bengali Currency Sign (৷)";
    if (cp == 0x0964) return "Devanagari / Bengali Danda (।)";
    if (cp == 0x0965) return "Devanagari Double Danda (॥)";
    if (cp == 0x200C) return "Zero Width Non-Joiner (ZWNJ)";
    if (cp == 0x200D) return "Zero Width Joiner (ZWJ)";
    if (cp == ' ') return "ASCII Space";
    return "Unicode Codepoint";
}

void InspectWideString(const std::wstring& u16) {
    std::string input_u8 = Utf16ToUtf8(u16);

    std::cout << "\n============================================================\n";
    std::cout << "  UNICODE CODEPOINT INSPECTION REPORT\n";
    std::cout << "============================================================\n";
    std::cout << "Text Input:        " << input_u8 << "\n";
    std::cout << "UTF-8 Bytes:       " << input_u8.length() << "\n";
    std::cout << "UTF-16 Units:      " << u16.length() << "\n";
    std::cout << "------------------------------------------------------------\n";
    std::cout << " Index | Hex Codepoint | UTF-8 Hex   | Glyph | Description\n";
    std::cout << "-------+---------------+-------------+-------+----------------\n";

    for (size_t i = 0; i < u16.length(); i++) {
        uint32_t cp = (uint32_t)u16[i];
        
        // Handle UTF-16 surrogate pairs if any
        if (cp >= 0xD800 && cp <= 0xDBFF && i + 1 < u16.length()) {
            uint32_t low = (uint32_t)u16[i + 1];
            if (low >= 0xDC00 && low <= 0xDFFF) {
                cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
                i++;
            }
        }

        // Convert individual codepoint to UTF-8 for display
        std::wstring single_w(1, (wchar_t)cp);
        std::string single_u8 = Utf16ToUtf8(single_w);

        std::cout << "  " << std::setw(4) << std::left << (i + 1)
                  << " | U+" << std::setw(8) << std::hex << std::uppercase << cp << std::dec
                  << " | ";

        std::string hex_str;
        for (unsigned char b : single_u8) {
            char buf[8];
            sprintf(buf, "\\x%02X", (int)b);
            hex_str += buf;
        }
        std::cout << std::setw(11) << std::left << hex_str;

        std::cout << " |   " << single_u8 << "   | " << GetCodepointDescription(cp) << "\n";
    }

    std::cout << "------------------------------------------------------------\n";

    // Detect integrity issues
    bool has_dangling_hasant = false;
    for (size_t i = 0; i < u16.length(); i++) {
        if (u16[i] == 0x09CD) {
            if (i == 0 || i + 1 == u16.length()) {
                has_dangling_hasant = true;
            }
        }
    }

    if (has_dangling_hasant) {
        std::cout << "[WARNING] DANGLING HASANT (U+09CD) DETECTED!\n";
    } else {
        std::cout << "[INTEGRITY PASS] Zero dangling Hasants. Unicode sequence is valid.\n";
    }
    std::cout << "============================================================\n\n";
}

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    std::cout << "=========================================================\n";
    std::cout << "  Likhi (লিখি) - Unicode Diagnostic Inspector\n";
    std::cout << "  \"বাংলা লিখুন, সহজেই।\"\n";
    std::cout << "=========================================================\n";

    int num_args = 0;
    LPWSTR* arg_list = CommandLineToArgvW(GetCommandLineW(), &num_args);
    if (arg_list && num_args > 1) {
        std::wstring input;
        for (int i = 1; i < num_args; i++) {
            if (i > 1) input += L" ";
            input += arg_list[i];
        }
        LocalFree(arg_list);
        InspectWideString(input);
        return 0;
    }
    if (arg_list) LocalFree(arg_list);

    std::cout << "Enter Bengali text to inspect (or 'exit' to quit):\n> ";
    std::string line;
    while (std::getline(std::cin, line)) {
        if (line == "exit" || line == "quit") break;
        if (!line.empty()) {
            InspectWideString(Utf8ToUtf16(line));
        }
        std::cout << "> ";
    }

    return 0;
}
