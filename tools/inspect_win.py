import ctypes
from ctypes import wintypes
import sys

user32 = ctypes.windll.user32

hdesk = user32.OpenDesktopW("Default", 0, False, 0x0100) # DESKTOP_ENUMERATE
print("OpenDesktop handle:", hdesk)

hwnds = []
def callback(hwnd, extra):
    len_title = user32.GetWindowTextLengthW(hwnd)
    buff_title = ctypes.create_unicode_buffer(len_title + 1)
    user32.GetWindowTextW(hwnd, buff_title, len_title + 1)

    buff_class = ctypes.create_unicode_buffer(256)
    user32.GetClassNameW(hwnd, buff_class, 256)

    rect = wintypes.RECT()
    user32.GetWindowRect(hwnd, ctypes.byref(rect))
    w = rect.right - rect.left
    h = rect.bottom - rect.top

    title = buff_title.value
    cls = buff_class.value
    if "Likhi" in title or "Likhi" in cls or "TopBar" in cls:
        is_vis = user32.IsWindowVisible(hwnd)
        print(f"HWND: {hwnd}, Class: '{cls}', Title: '{title}', Visible: {bool(is_vis)}, Rect: ({rect.left}, {rect.top}, {rect.right}, {rect.bottom}), Size: {w}x{h}")
    return True

cb = ctypes.WINFUNCTYPE(ctypes.c_bool, wintypes.HWND, wintypes.LPARAM)(callback)
user32.EnumDesktopWindows(hdesk, cb, 0)
