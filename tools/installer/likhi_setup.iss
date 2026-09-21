; Likhi (লিখি) - Production Inno Setup Script
; "বাংলা লিখুন, সহজেই।"

#define MyAppName "Likhi"
#define MyAppFullName "Likhi (লিখি)"
#define MyAppVersion "1.0.0"
#define MyAppPublisher "Likhi Team"
#define MyAppURL "https://github.com/likhi-bangla"
#define MyAppExeName "bangla_settings.exe"

[Setup]
AppId={{B4F1470A-7C69-4C62-972F-6379532856E1}
AppName={#MyAppFullName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppFullName} v{#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}
DefaultDirName={autopf}\Likhi
DefaultGroupName={#MyAppName}
AllowNoIcons=yes
OutputDir=..\..\release_package
OutputBaseFilename=Likhi_Setup_v{#MyAppVersion}
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
PrivilegesRequired=admin
ArchitecturesInstallIn64BitMode=x64
CloseApplications=force
UninstallDisplayName={#MyAppFullName} - Intelligent Bangla Typing System
UninstallDisplayIcon={app}\bangla_settings.exe

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Files]
; Core TSF Inproc DLL (64-bit for native 64-bit applications)
Source: "..\..\build\bangla_tsf.dll"; DestDir: "{app}"; Flags: ignoreversion restartreplace
; Core TSF Inproc DLL (32-bit for WOW64 applications like 32-bit MS Office)
Source: "..\..\build\bangla_tsf32.dll"; DestDir: "{app}"; Flags: ignoreversion restartreplace; Check: IsWin64
; Settings Application
Source: "..\..\build\bangla_settings.exe"; DestDir: "{app}"; Flags: ignoreversion
; Dictionary Binary
Source: "..\..\engine\data\lexicon.bin"; DestDir: "{app}\data"; Flags: ignoreversion
; Scripts for modern language bar configuration
Source: "enable_tip.ps1"; DestDir: "{app}\tools"; Flags: ignoreversion
Source: "disable_tip.ps1"; DestDir: "{app}\tools"; Flags: ignoreversion
; Documentation
Source: "README.txt"; DestDir: "{app}"; Flags: ignoreversion isreadme

[Icons]
Name: "{group}\Likhi Settings"; Filename: "{app}\{#MyAppExeName}"
Name: "{group}\Likhi Documentation"; Filename: "{app}\README.txt"
Name: "{group}\{cm:UninstallProgram,{#MyAppFullName}}"; Filename: "{uninstallexe}"

[Registry]
; -------------------------------------------------------------
; 1. 64-Bit COM Server Registration (HKA = HKLM if admin, HKCU if user)
; -------------------------------------------------------------
Root: HKA; Subkey: "Software\Classes\CLSID\{{B4F1470A-7C69-4C62-972F-6379532856E1}"; ValueType: string; ValueData: "Likhi (লিখি)"; Flags: uninsdeletekey
Root: HKA; Subkey: "Software\Classes\CLSID\{{B4F1470A-7C69-4C62-972F-6379532856E1}\InprocServer32"; ValueType: string; ValueData: "{app}\bangla_tsf.dll"; Flags: uninsdeletekey
Root: HKA; Subkey: "Software\Classes\CLSID\{{B4F1470A-7C69-4C62-972F-6379532856E1}\InprocServer32"; ValueType: string; ValueName: "ThreadingModel"; ValueData: "Apartment"

; -------------------------------------------------------------
; 2. 32-Bit WOW6432Node COM Server Registration (for 32-bit hosts)
; -------------------------------------------------------------
Root: HKA; Subkey: "Software\WOW6432Node\Classes\CLSID\{{B4F1470A-7C69-4C62-972F-6379532856E1}"; ValueType: string; ValueData: "Likhi (লিখি)"; Flags: uninsdeletekey; Check: IsWin64
Root: HKA; Subkey: "Software\WOW6432Node\Classes\CLSID\{{B4F1470A-7C69-4C62-972F-6379532856E1}\InprocServer32"; ValueType: string; ValueData: "{app}\bangla_tsf32.dll"; Flags: uninsdeletekey; Check: IsWin64
Root: HKA; Subkey: "Software\WOW6432Node\Classes\CLSID\{{B4F1470A-7C69-4C62-972F-6379532856E1}\InprocServer32"; ValueType: string; ValueName: "ThreadingModel"; ValueData: "Apartment"; Check: IsWin64

; -------------------------------------------------------------
; 3. TSF TIP Registration (HKA\Software\Microsoft\CTF\TIP)
; -------------------------------------------------------------
Root: HKA; Subkey: "Software\Microsoft\CTF\TIP\{{B4F1470A-7C69-4C62-972F-6379532856E1}"; ValueType: dword; ValueName: "Enable"; ValueData: 1; Flags: uninsdeletekey

; Bengali (Bangladesh) - Canonical 0x0845
Root: HKA; Subkey: "Software\Microsoft\CTF\TIP\{{B4F1470A-7C69-4C62-972F-6379532856E1}\LanguageProfile\0x00000845\{{D85B64E2-0D5C-40EE-BE15-1E7C146603F2}"; ValueType: string; ValueName: "Description"; ValueData: "Likhi (লিখি) - Bangla (Bangladesh)"; Flags: uninsdeletekey
Root: HKA; Subkey: "Software\Microsoft\CTF\TIP\{{B4F1470A-7C69-4C62-972F-6379532856E1}\LanguageProfile\0x00000845\{{D85B64E2-0D5C-40EE-BE15-1E7C146603F2}"; ValueType: string; ValueName: "IconFile"; ValueData: "{app}\bangla_tsf.dll"
Root: HKA; Subkey: "Software\Microsoft\CTF\TIP\{{B4F1470A-7C69-4C62-972F-6379532856E1}\LanguageProfile\0x00000845\{{D85B64E2-0D5C-40EE-BE15-1E7C146603F2}"; ValueType: dword; ValueName: "IconIndex"; ValueData: 0
Root: HKA; Subkey: "Software\Microsoft\CTF\TIP\{{B4F1470A-7C69-4C62-972F-6379532856E1}\LanguageProfile\0x00000845\{{D85B64E2-0D5C-40EE-BE15-1E7C146603F2}"; ValueType: dword; ValueName: "Enable"; ValueData: 1

; Bengali (India) - Canonical 0x0445
Root: HKA; Subkey: "Software\Microsoft\CTF\TIP\{{B4F1470A-7C69-4C62-972F-6379532856E1}\LanguageProfile\0x00000445\{{D85B64E2-0D5C-40EE-BE15-1E7C146603F2}"; ValueType: string; ValueName: "Description"; ValueData: "Likhi (লিখি) - Bangla (India)"; Flags: uninsdeletekey
Root: HKA; Subkey: "Software\Microsoft\CTF\TIP\{{B4F1470A-7C69-4C62-972F-6379532856E1}\LanguageProfile\0x00000445\{{D85B64E2-0D5C-40EE-BE15-1E7C146603F2}"; ValueType: string; ValueName: "IconFile"; ValueData: "{app}\bangla_tsf.dll"
Root: HKA; Subkey: "Software\Microsoft\CTF\TIP\{{B4F1470A-7C69-4C62-972F-6379532856E1}\LanguageProfile\0x00000445\{{D85B64E2-0D5C-40EE-BE15-1E7C146603F2}"; ValueType: dword; ValueName: "IconIndex"; ValueData: 0
Root: HKA; Subkey: "Software\Microsoft\CTF\TIP\{{B4F1470A-7C69-4C62-972F-6379532856E1}\LanguageProfile\0x00000445\{{D85B64E2-0D5C-40EE-BE15-1E7C146603F2}"; ValueType: dword; ValueName: "Enable"; ValueData: 1

; English (United States) - 0x0409
Root: HKA; Subkey: "Software\Microsoft\CTF\TIP\{{B4F1470A-7C69-4C62-972F-6379532856E1}\LanguageProfile\0x00000409\{{D85B64E2-0D5C-40EE-BE15-1E7C146603F2}"; ValueType: string; ValueName: "Description"; ValueData: "Likhi (লিখি) - English (US)"; Flags: uninsdeletekey
Root: HKA; Subkey: "Software\Microsoft\CTF\TIP\{{B4F1470A-7C69-4C62-972F-6379532856E1}\LanguageProfile\0x00000409\{{D85B64E2-0D5C-40EE-BE15-1E7C146603F2}"; ValueType: string; ValueName: "IconFile"; ValueData: "{app}\bangla_tsf.dll"
Root: HKA; Subkey: "Software\Microsoft\CTF\TIP\{{B4F1470A-7C69-4C62-972F-6379532856E1}\LanguageProfile\0x00000409\{{D85B64E2-0D5C-40EE-BE15-1E7C146603F2}"; ValueType: dword; ValueName: "IconIndex"; ValueData: 0
Root: HKA; Subkey: "Software\Microsoft\CTF\TIP\{{B4F1470A-7C69-4C62-972F-6379532856E1}\LanguageProfile\0x00000409\{{D85B64E2-0D5C-40EE-BE15-1E7C146603F2}"; ValueType: dword; ValueName: "Enable"; ValueData: 1

; Category: Keyboard TIP
Root: HKA; Subkey: "Software\Microsoft\CTF\TIP\{{B4F1470A-7C69-4C62-972F-6379532856E1}\Category\Category\{{34745C63-B2F0-4784-8B67-5E12C8701A31}\{{B4F1470A-7C69-4C62-972F-6379532856E1}"; ValueType: none; Flags: uninsdeletekey
Root: HKA; Subkey: "Software\Microsoft\CTF\TIP\{{B4F1470A-7C69-4C62-972F-6379532856E1}\Category\Item\{{B4F1470A-7C69-4C62-972F-6379532856E1}"; ValueType: dword; ValueName: "{{34745C63-B2F0-4784-8B67-5E12C8701A31}"; ValueData: 0; Flags: uninsdeletekey

; Category: Immersive / UWP Support (Required for Windows Store apps like WhatsApp Desktop)
Root: HKA; Subkey: "Software\Microsoft\CTF\TIP\{{B4F1470A-7C69-4C62-972F-6379532856E1}\Category\Category\{{13A016DF-560B-46CD-947A-4C3AF1E0E35D}\{{B4F1470A-7C69-4C62-972F-6379532856E1}"; ValueType: none; Flags: uninsdeletekey
Root: HKA; Subkey: "Software\Microsoft\CTF\TIP\{{B4F1470A-7C69-4C62-972F-6379532856E1}\Category\Item\{{B4F1470A-7C69-4C62-972F-6379532856E1}"; ValueType: dword; ValueName: "{{13A016DF-560B-46CD-947A-4C3AF1E0E35D}"; ValueData: 0; Flags: uninsdeletekey

; Category: Systray Support
Root: HKA; Subkey: "Software\Microsoft\CTF\TIP\{{B4F1470A-7C69-4C62-972F-6379532856E1}\Category\Category\{{25504FB4-7BAB-4BC1-9C69-CF81890F0EF5}\{{B4F1470A-7C69-4C62-972F-6379532856E1}"; ValueType: none; Flags: uninsdeletekey
Root: HKA; Subkey: "Software\Microsoft\CTF\TIP\{{B4F1470A-7C69-4C62-972F-6379532856E1}\Category\Item\{{B4F1470A-7C69-4C62-972F-6379532856E1}"; ValueType: dword; ValueName: "{{25504FB4-7BAB-4BC1-9C69-CF81890F0EF5}"; ValueData: 0; Flags: uninsdeletekey

[Run]
; Grant ALL APPLICATION PACKAGES permission to installation folder for UWP / Windows Store apps (WhatsApp Desktop)
Filename: "icacls.exe"; Parameters: """{app}"" /grant ""*S-1-15-2-1:(OI)(CI)(RX)"" /T"; Flags: runhidden; StatusMsg: "Configuring security permissions for Windows Store apps..."
; Self-register the 64-bit DLL via regsvr32
Filename: "{sys}\regsvr32.exe"; Parameters: "/s ""{app}\bangla_tsf.dll"""; StatusMsg: "Registering 64-bit Text Services Framework components..."
; Self-register the 32-bit DLL via 32-bit regsvr32 (for 32-bit apps like MS Office)
Filename: "{syswow64}\regsvr32.exe"; Parameters: "/s ""{app}\bangla_tsf32.dll"""; StatusMsg: "Registering 32-bit Text Services Framework components..."; Check: IsWin64
; Configure modern Windows Language Bar (Win + Space)
Filename: "powershell.exe"; Parameters: "-NoProfile -ExecutionPolicy Bypass -File ""{app}\tools\enable_tip.ps1"""; Flags: runhidden; StatusMsg: "Configuring Windows Language Switcher (Win + Space)..."
; Optional post-install launch
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent

[UninstallRun]
; Remove from modern Windows Language Bar
Filename: "powershell.exe"; Parameters: "-NoProfile -ExecutionPolicy Bypass -File ""{app}\tools\disable_tip.ps1"""; Flags: runhidden; StatusMsg: "Removing Likhi from Windows Language Switcher..."
; Unregister 32-bit DLL
Filename: "{syswow64}\regsvr32.exe"; Parameters: "/u /s ""{app}\bangla_tsf32.dll"""; StatusMsg: "Unregistering 32-bit Text Services Framework components..."; Check: IsWin64
; Unregister 64-bit DLL
Filename: "{sys}\regsvr32.exe"; Parameters: "/u /s ""{app}\bangla_tsf.dll"""; StatusMsg: "Unregistering 64-bit Text Services Framework components..."

[Code]
// Ensures user data in %APPDATA%\PC-Bangla-Typing-App is NEVER deleted during uninstall
procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
begin
  if CurUninstallStep = usPostUninstall then
  begin
    Log('Likhi uninstalled successfully. User learning data in %APPDATA% was preserved.');
  end;
end;
