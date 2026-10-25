import ctypes
import time
from ctypes import wintypes

user32 = ctypes.windll.user32

def find_topbar():
    found = []
    def cb(hwnd, extra):
        buf = ctypes.create_unicode_buffer(256)
        user32.GetClassNameW(hwnd, buf, 256)
        if buf.value == "LikhiFloatingTopBar":
            found.append(hwnd)
        return True
    
    hdesk = user32.OpenDesktopW("Default", 0, False, 0x0100)
    user32.EnumDesktopWindows(hdesk, ctypes.WINFUNCTYPE(ctypes.c_bool, wintypes.HWND, wintypes.LPARAM)(cb), 0)
    return found[0] if found else None

if __name__ == '__main__':
    hwnd = find_topbar()
    print("Found TopBar HWND:", hwnd)
    assert hwnd, "TopBar window not found"

    def get_rect(h):
        r = wintypes.RECT()
        user32.GetWindowRect(h, ctypes.byref(r))
        return r.left, r.top, r.right, r.bottom, (r.right - r.left), (r.bottom - r.top)

    print("Initial Rect:", get_rect(hwnd))

    # Move mouse over TopBar to trigger hover expansion
    left, top, right, bottom, w, h = get_rect(hwnd)
    cx = left + 10
    cy = top + 10

    WM_MOUSEMOVE = 0x0200
    lparam = (10 << 16) | 10
    user32.PostMessageW(hwnd, WM_MOUSEMOVE, 0, lparam)
    time.sleep(0.3)
    print("After hover Rect:", get_rect(hwnd))

    # Move window directly to (520, 120) with proper 64-bit HWND_TOPMOST
    user32.SetWindowPos.argtypes = [wintypes.HWND, ctypes.c_void_p, ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_int, wintypes.UINT]
    user32.SetWindowPos(hwnd, ctypes.c_void_p(-1), 520, 120, 262, 24, 0x0040)
    WM_EXITSIZEMOVE = 0x0232
    user32.SendMessageW(hwnd, WM_EXITSIZEMOVE, 0, 0)
    time.sleep(0.5)

    print("After move to (520, 120):", get_rect(hwnd))

    print("Waiting 4.5s across 3-second timer ticks...")
    time.sleep(4.5)

    r2 = get_rect(hwnd)
    print("Rect after 4.5s idle:", r2)
    assert r2[0] == 520, f"Expected pos_x == 520, got {r2[0]} (snapped back!)"
    assert r2[1] == 120, f"Expected pos_y == 120, got {r2[1]} (snapped back!)"
    print("SUCCESS: TopBar stayed at custom position (520, 120) without resetting to center!")

