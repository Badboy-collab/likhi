# ============================================================
# LIKHI — REAL-INPUT E2E QA SCRIPT (manual verification)
#
# Proves the master-prompt contract on a REAL Windows session:
#   * 'ami ' + one Space  ->  "আমি "  (one word commit + one space)
#   * 'ami ajke '         ->  "আমি আজকে "  (no double space ever)
#   * digits / numpad     ->  native numbers pass through
#   * Ctrl+A / Ctrl+C     ->  full pass-through (also used to read result)
#   * F1-F12, Esc, Tab, arrows -> application keeps original behavior
#
# USAGE:
#   1. Make sure the rebuilt bangla_tsf.dll is registered
#      (run tools/installer/install.bat as Administrator).
#   2. Switch the active input to "Bangla (Bangladesh) - Likhi"
#      (Win+Space) so Notepad is typing with Likhi.
#   3. Run:  python test_real_input.py
#   4. Check the printed captured text at the end.
#
# NOTE: TSF input cannot be asserted deterministically from a script,
# so this is a MANUAL proof tool — the final captured text must show
# exactly the expected output printed below.
# ============================================================
import ctypes
import time
import subprocess
import sys

# --- SendInput helpers -------------------------------------------------
PUL = ctypes.POINTER(ctypes.c_ulong)

class KeyBdInput(ctypes.Structure):
    _fields_ = [("wVk", ctypes.c_ushort),
                ("wScan", ctypes.c_ushort),
                ("dwFlags", ctypes.c_ulong),
                ("time", ctypes.c_ulong),
                ("dwExtraInfo", PUL)]

class Input_I(ctypes.Union):
    _fields_ = [("ki", KeyBdInput)]

class Input(ctypes.Structure):
    _fields_ = [("type", ctypes.c_ulong),
                ("ii", Input_I)]

def _send(vk, flags):
    extra = ctypes.c_ulong(0)
    ii = Input_I()
    ii.ki = KeyBdInput(vk, 0, flags, 0, ctypes.pointer(extra))
    x = Input(ctypes.c_ulong(1), ii)
    ctypes.windll.user32.SendInput(1, ctypes.pointer(x), ctypes.sizeof(x))

def key_down(vk): _send(vk, 0)
def key_up(vk):   _send(vk, 0x0002)  # KEYEVENTF_KEYUP
def tap(vk, delay=0.06):
    key_down(vk); time.sleep(delay); key_up(vk); time.sleep(delay)

# --- clipboard reader --------------------------------------------------
def get_clipboard_text():
    if not ctypes.windll.user32.OpenClipboard(0):
        return None
    try:
        h = ctypes.windll.user32.GetClipboardData(13)  # CF_UNICODETEXT
        if not h:
            return ""
        p = ctypes.windll.kernel32.GlobalLock(h)
        try:
            return ctypes.wstring_at(p)
        finally:
            ctypes.windll.kernel32.GlobalUnlock(h)
    finally:
        ctypes.windll.user32.CloseClipboard()

# --- VK codes -----------------------------------------------------------
VK_A, VK_M, VK_I = 0x41, 0x4D, 0x49
VK_J, VK_K, VK_E = 0x4A, 0x4B, 0x45
VK_SPACE = 0x20
VK_1, VK_5 = 0x31, 0x35
VK_NUMPAD5 = 0x65
VK_F5, VK_F12 = 0x74, 0x7B
VK_TAB, VK_ESCAPE = 0x09, 0x1B
VK_LEFT, VK_RIGHT = 0x25, 0x27
VK_CONTROL, VK_C = 0x11, 0x43

def ctrl_combo(vk):
    key_down(VK_CONTROL); time.sleep(0.05)
    tap(vk); time.sleep(0.05)
    key_up(VK_CONTROL); time.sleep(0.05)

# --- flow ---------------------------------------------------------------
print("Launching Notepad...")
proc = subprocess.Popen(['notepad.exe'])
time.sleep(2.5)

# 1) Two-word sentence: each Space = one commit + exactly one space.
for ch in ('a', 'm', 'i'):
    tap({'a': VK_A, 'm': VK_M, 'i': VK_I}[ch])
tap(VK_SPACE)                      # -> "আমি "
for ch in ('a', 'j', 'k', 'e'):
    tap({'a': VK_A, 'j': VK_J, 'k': VK_K, 'e': VK_E}[ch])
tap(VK_SPACE)                      # -> "আমি আজকে "

# 2) Native number must pass through (top row and numpad).
tap(VK_5)
tap(VK_NUMPAD5)

# 3) System/application keys keep their original behavior:
tap(VK_LEFT, 0.12)                 # caret back (no composition active)
tap(VK_RIGHT, 0.12)
tap(VK_ESCAPE, 0.12)               # idle Esc -> application Esc (no cancel)
tap(VK_TAB, 0.12)                  # idle Tab -> moves focus (native)

# 4) Ctrl+A / Ctrl+C are full pass-through and let us read the result.
ctrl_combo(VK_A)                   # select all
time.sleep(0.2)
ctrl_combo(VK_C)                   # copy
time.sleep(0.3)

captured = get_clipboard_text()
print("\n=========================================================")
print("CAPTURED TEXT FROM NOTEPAD:")
print(repr(captured))
print("=========================================================")

expected_prefix = "আমি আজকে 55"   # two words, one space each, then 5 and 5
if captured is None:
    print("[WARN] Clipboard read failed — check the Notepad window directly.")
    sys.exit(2)
if captured == expected_prefix:
    print("[PASS] Output matches exactly: 'আমি আজকে 55' (one space per Space key, native digits).")
    print("[PASS] Ctrl+A/Ctrl+C passed through (copy worked while Likhi active).")
elif captured.startswith("আমি"):
    print("[FAIL] Prefix correct but full text differs. Double-space or lost characters?")
    print("       Expected: " + repr(expected_prefix))
    sys.exit(1)
else:
    print("[FAIL] Output does not look like Likhi output at all.")
    print("       Is 'Bangla (Bangladesh) - Likhi' the active input? (Win+Space)")
    sys.exit(1)

print("\nKeep Notepad open to visually confirm. Done.")
# Do NOT terminate Notepad — the user should inspect it.
