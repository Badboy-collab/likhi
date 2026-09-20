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

// Language IDs as CANONICAL numeric literals matching Windows winnt.h:
// MAKELANGID(LANG_BENGALI, SUBLANG_BENGALI_BANGLADESH) = (0x02 << 10) | 0x45 = 0x0845 (bn-BD)
// MAKELANGID(LANG_BENGALI, SUBLANG_BENGALI_INDIA)      = (0x01 << 10) | 0x45 = 0x0445 (bn-IN)
// MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US)         = (0x01 << 10) | 0x09 = 0x0409 (en-US)
#define BANGLA_LANGID_BD   0x0845   // Bengali (Bangladesh) - Canonical Windows LCID
#define BANGLA_LANGID_IN   0x0445   // Bengali (India) - Canonical Windows LCID
#define BANGLA_LANGID_US   0x0409   // English (United States)

#define BANGLA_IME_NAME_W      L"Likhi (লিখি)"
#define BANGLA_IME_DESC_W      L"Likhi (লিখি) — বাংলা লিখুন, সহজেই।"

#endif // BANGLA_TSF_CLSID_H
