# Likhi (লিখি) — Installation & Registration Guide

> **“বাংলা লিখুন, সহজেই।”**

## 1. Prerequisites

- Windows 10 / 11 (64-bit)
- Administrative privileges for COM DLL registration

---

## 2. Building from Source

```powershell
# Prepend MinGW and Python paths if needed
$env:Path = "C:\tools\mingw64\bin;C:\Users\Pervez\AppData\Local\Programs\Python\Python312;" + $env:Path

# Generate build files and compile in Release mode
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

Build outputs located in `build/`:
- `bangla_tsf.dll`: Native Windows TSF In-Process COM DLL
- `bangla_settings.exe`: Native Win32 Configuration App
- `test_tsf_integration.exe`: Automated TSF test runner
- `unicode_bengali_tests.exe`: Unicode integrity test runner
- `test_runner.exe`: Standalone engine unit test runner
- `benchmark_perf.exe`: Performance and RAM profiling benchmark

---

## 3. Registering the IME

### Option A: Via Automated Batch Script
Run `tools/installer/install.bat` as Administrator.

### Option B: Via PowerShell Helper
```powershell
powershell -ExecutionPolicy Bypass -File tools/installer/register_tsf.ps1
```

### Option C: Manual Registration
```powershell
regsvr32.exe build/bangla_tsf.dll
```

---

## 4. Unregistering the IME

Run `tools/installer/uninstall.bat` as Administrator, or execute:
```powershell
regsvr32.exe /u build/bangla_tsf.dll
```
