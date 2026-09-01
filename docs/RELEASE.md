# Release & Distribution Strategy: PC Bangla Typing App

## 1. Release Packaging
The application distribution will consist of:
1. **Windows Installer (MSI / WiX Toolset or Inno Setup)**:
   - Self-contained, lightweight installer (< 10 MB).
   - Automatically registers the Windows TSF COM InProcServer DLL (`bangla_tsf.dll`) using `regsvr32` / TSF API calls.
   - Creates Start Menu shortcuts for Settings and Documentation.
   - Configures clean uninstallation (removes COM registrations, binaries, and optionally offers to delete local user dictionary).
2. **Portable ZIP Package**:
   - For advanced users, containing registration scripts (`register_ime.bat`, `unregister_ime.bat`) and standalone CLI engine.

---

## 2. Windows Code Signing
To ensure seamless execution on Windows 10/11 without SmartScreen warnings:
- Executables and COM DLLs will be signed with a standard Microsoft Authenticode code signing certificate.
- SHA-256 digital signatures applied via `signtool.exe`.

---

## 3. Versioning Strategy
Semantic Versioning (`MAJOR.MINOR.PATCH`):
- `0.1.0`: Phase 1 Standalone Engine Prototype (CLI).
- `0.2.0`: Phase 2 Engine with Lexicon & N-Gram Ranking Benchmark.
- `0.5.0`: Phase 4 Windows TSF Integration & Suggestion UI Preview.
- `1.0.0`: Production-ready Windows installer with Auto-Correct, Personal Dictionary, and full TSF support.
