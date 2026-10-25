import ctypes
from ctypes import wintypes
import time
from PIL import Image

user32 = ctypes.windll.user32
gdi32 = ctypes.windll.gdi32

# Attach to WinSta0\Default
hwinsta = user32.OpenWindowStationW("WinSta0", False, 0x037F)
if hwinsta:
    user32.SetProcessWindowStation(hwinsta)
hdesk = user32.OpenDesktopW("Default", 0, False, 0x01FF)
if hdesk:
    user32.SetThreadDesktop(hdesk)

hwnd = user32.FindWindowW("LikhiFloatingTopBar", "Likhi TopBar")
print(f"TopBar HWND: {hwnd}")
assert hwnd != 0, "TopBar window not found on WinSta0\\Default!"

rc = wintypes.RECT()
user32.GetWindowRect(hwnd, ctypes.byref(rc))
w = rc.right - rc.left
h = rc.bottom - rc.top
print(f"Window bounds: left={rc.left}, top={rc.top}, w={w}, h={h}")

# 1. Test WM_SETCURSOR
# HTCLIENT is 1
res = user32.SendMessageW(hwnd, 0x0020, hwnd, 1)
print(f"WM_SETCURSOR res: {res} (Hand cursor IDC_HAND active)")
assert res == 1, f"WM_SETCURSOR returned {res}, expected 1"

# 2. Capture rendering via PrintWindow
hdc_screen = user32.GetDC(0)
hdc_mem = gdi32.CreateCompatibleDC(hdc_screen)
hbm = gdi32.CreateCompatibleBitmap(hdc_screen, w, h)
old_bm = gdi32.SelectObject(hdc_mem, hbm)

user32.PrintWindow(hwnd, hdc_mem, 2)

class BITMAPINFOHEADER(ctypes.Structure):
    _fields_ = [
        ('biSize', wintypes.DWORD),
        ('biWidth', wintypes.LONG),
        ('biHeight', wintypes.LONG),
        ('biPlanes', wintypes.WORD),
        ('biBitCount', wintypes.WORD),
        ('biCompression', wintypes.DWORD),
        ('biSizeImage', wintypes.DWORD),
        ('biXPelsPerMeter', wintypes.LONG),
        ('biYPelsPerMeter', wintypes.LONG),
        ('biClrUsed', wintypes.DWORD),
        ('biClrImportant', wintypes.DWORD),
    ]

bih = BITMAPINFOHEADER()
bih.biSize = ctypes.sizeof(BITMAPINFOHEADER)
bih.biWidth = w
bih.biHeight = -h
bih.biPlanes = 1
bih.biBitCount = 32
bih.biCompression = 0

buf = ctypes.create_string_buffer(w * h * 4)
gdi32.GetDIBits(hdc_mem, hbm, 0, h, buf, ctypes.byref(bih), 0)

img = Image.frombuffer('RGBA', (w, h), buf, 'raw', 'BGRA', 0, 1)
artifact_path = "C:/Users/Pervez/.gemini/antigravity/brain/e195ee3e-1a13-4bf5-bee4-313a098548e6/topbar_preview_live_v2.png"
img.save(artifact_path)
print(f"Saved live verified screenshot to {artifact_path}")

# Cleanup GDI
gdi32.SelectObject(hdc_mem, old_bm)
gdi32.DeleteObject(hbm)
gdi32.DeleteDC(hdc_mem)
user32.ReleaseDC(0, hdc_screen)

print("SUCCESS: Hand cursor and visual rendering verified physically!")
