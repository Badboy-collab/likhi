#ifndef BANGLA_TSF_CLSID_H
#define BANGLA_TSF_CLSID_H

#include <windows.h>
#include <initguid.h>

// CLSID_BanglaTextService: {B4F1470A-7C69-4C62-972F-6379532856E1}
DEFINE_GUID(CLSID_BanglaTextService,
    0xb4f1470a, 0x7c69, 0x4c62, 0x97, 0x2f, 0x63, 0x79, 0x53, 0x28, 0x56, 0xe1);

// GUID_BanglaProfile: {D85B64E2-0D5C-40EE-BE15-1E7C146603F2}
DEFINE_GUID(GUID_BanglaProfile,
    0xd85b64e2, 0x0d5c, 0x40ee, 0xbe, 0x15, 0x1e, 0x7c, 0x14, 0x66, 0x03, 0xf2);

// GUID_BanglaDisplayAttribute: {4C1B14E1-2678-4F15-B6D1-FE3C8B72AA41}
DEFINE_GUID(GUID_BanglaDisplayAttribute,
    0x4c1b14e1, 0x2678, 0x4f15, 0xb6, 0xd1, 0xfe, 0x3c, 0x8b, 0x72, 0xaa, 0x41);

// Language IDs as CANONICAL numeric literals.
// IMPORTANT: do NOT derive these from SUBLANG_BENGALI_BANGLADESH/INDIA:
// several MinGW-w64 header versions SWAP those two constants (BD=0x02, IN=0x01),
// which silently registers the TSF profile under the wrong Bengali locale.
// Canonical values (winnt.h): SUBLANG_BENGALI_BANGLADESH = 0x01 -> 0x0445,
//                             SUBLANG_BENGALI_INDIA      = 0x02 -> 0x0845.
#define BANGLA_LANGID_BD   0x0445   // Bengali (Bangladesh)
#define BANGLA_LANGID_IN   0x0845   // Bengali (India)
#define BANGLA_LANGID_US   0x0409   // English (United States)

#define BANGLA_IME_NAME_W      L"Likhi (লিখি)"
#define BANGLA_IME_DESC_W      L"Likhi (লিখি) — বাংলা লিখুন, সহজেই।"

#endif // BANGLA_TSF_CLSID_H
