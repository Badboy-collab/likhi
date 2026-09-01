#ifndef BANGLA_TSF_UTILS_H
#define BANGLA_TSF_UTILS_H

#include <windows.h>
#include <string>
#include <vector>

namespace bangla_tsf {

inline std::wstring Utf8ToUtf16(const std::string& utf8_str) {
    if (utf8_str.empty()) return std::wstring();
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, utf8_str.data(), (int)utf8_str.size(), NULL, 0);
    if (size_needed <= 0) return std::wstring();
    std::wstring result(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, utf8_str.data(), (int)utf8_str.size(), &result[0], size_needed);
    return result;
}

inline std::string Utf16ToUtf8(const std::wstring& utf16_str) {
    if (utf16_str.empty()) return std::string();
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, utf16_str.data(), (int)utf16_str.size(), NULL, 0, NULL, NULL);
    if (size_needed <= 0) return std::string();
    std::string result(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, utf16_str.data(), (int)utf16_str.size(), &result[0], size_needed, NULL, NULL);
    return result;
}

} // namespace bangla_tsf

#endif // BANGLA_TSF_UTILS_H
