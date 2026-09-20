#pragma once
#include <windows.h>
#include <cstdio>
#include <cstdarg>

namespace bangla_tsf {

inline void TsfLog(const char* fmt, ...) {
    static wchar_t logPath[MAX_PATH] = {0};
    if (logPath[0] == 0) {
        wchar_t tempDir[MAX_PATH] = {0};
        if (GetTempPathW(MAX_PATH, tempDir) > 0) {
            wsprintfW(logPath, L"%slikhi_tsf_debug.log", tempDir);
        } else {
            wcscpy_s(logPath, L"C:\\Users\\Pervez\\AppData\\Local\\Temp\\likhi_tsf_debug.log");
        }
    }
    
    char buf[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    OutputDebugStringA(buf);

    FILE* f = _wfopen(logPath, L"a");
    if (!f) return;
    
    DWORD pid = GetCurrentProcessId();
    DWORD tid = GetCurrentThreadId();
    fprintf(f, "[%lu:%lu] %s\n", pid, tid, buf);
    fflush(f);
    fclose(f);
}

} // namespace bangla_tsf
