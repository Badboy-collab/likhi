import ctypes
from ctypes import wintypes
import sys
sys.stdout.reconfigure(encoding='utf-8')

user32 = ctypes.windll.user32
ole32 = ctypes.windll.ole32

hwinsta = user32.OpenWindowStationW("WinSta0", False, 0x037F)
if hwinsta: user32.SetProcessWindowStation(hwinsta)
hdesk = user32.OpenDesktopW("Default", 0, False, 0x01FF)
if hdesk: user32.SetThreadDesktop(hdesk)

ole32.CoInitialize(None)

class GUID(ctypes.Structure):
    _fields_ = [
        ('Data1', wintypes.DWORD),
        ('Data2', wintypes.WORD),
        ('Data3', wintypes.WORD),
        ('Data4', ctypes.c_byte * 8)
    ]

CLSID_CUIAutomation = GUID(0xff48dba4, 0x60ef, 0x4201, (ctypes.c_byte * 8)(0xaa, 0x87, 0x54, 0x10, 0x3e, 0xef, 0x59, 0x4e))
IID_IUIAutomation = GUID(0x30cbe57d, 0xd9d0, 0x452a, (ctypes.c_byte * 8)(0xab, 0x13, 0x7a, 0xc5, 0xac, 0x48, 0x25, 0xee))

pUIA = ctypes.c_void_p()
hr = ole32.CoCreateInstance(
    ctypes.byref(CLSID_CUIAutomation), None, 1,
    ctypes.byref(IID_IUIAutomation), ctypes.byref(pUIA)
)

if hr == 0 and pUIA.value:
    vtable_uia = ctypes.cast(pUIA.value, ctypes.POINTER(ctypes.POINTER(ctypes.c_void_p))).contents
    get_focused_el = ctypes.WINFUNCTYPE(wintypes.LONG, ctypes.c_void_p, ctypes.POINTER(ctypes.c_void_p))(vtable_uia[8])
    
    pFocused = ctypes.c_void_p()
    hr = get_focused_el(pUIA.value, ctypes.byref(pFocused))
    
    if hr == 0 and pFocused.value:
        vtable_el = ctypes.cast(pFocused.value, ctypes.POINTER(ctypes.POINTER(ctypes.c_void_p))).contents
        # Method 43: get_CurrentBoundingRectangle(RECT *rect)
        get_bounds = ctypes.WINFUNCTYPE(wintypes.LONG, ctypes.c_void_p, ctypes.POINTER(wintypes.RECT))(vtable_el[43])
        rc = wintypes.RECT()
        hr = get_bounds(pFocused.value, ctypes.byref(rc))
        print(f"Method 43 get_CurrentBoundingRectangle: hr={hr}, ({rc.left}, {rc.top}, {rc.right}, {rc.bottom}), w={rc.right-rc.left}, h={rc.bottom-rc.top}")

ole32.CoUninitialize()
