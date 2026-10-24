import ctypes
from ctypes import wintypes
import os
import time
import sys
import subprocess

kernel32 = ctypes.windll.kernel32
user32 = ctypes.windll.user32

class STARTUPINFOW(ctypes.Structure):
    _fields_ = [
        ('cb', wintypes.DWORD),
        ('lpReserved', wintypes.LPWSTR),
        ('lpDesktop', wintypes.LPWSTR),
        ('lpTitle', wintypes.LPWSTR),
        ('dwX', wintypes.DWORD),
        ('dwY', wintypes.DWORD),
        ('dwXSize', wintypes.DWORD),
        ('dwYSize', wintypes.DWORD),
        ('dwXCountChars', wintypes.DWORD),
        ('dwYCountChars', wintypes.DWORD),
        ('dwFillAttribute', wintypes.DWORD),
        ('dwFlags', wintypes.DWORD),
        ('wShowWindow', wintypes.WORD),
        ('cbReserved2', wintypes.WORD),
        ('lpReserved2', ctypes.c_char_p),
        ('hStdInput', wintypes.HANDLE),
        ('hStdOutput', wintypes.HANDLE),
        ('hStdError', wintypes.HANDLE)
    ]

class PROCESS_INFORMATION(ctypes.Structure):
    _fields_ = [
        ('hProcess', wintypes.HANDLE),
        ('hThread', wintypes.HANDLE),
        ('dwProcessId', wintypes.DWORD),
        ('dwThreadId', wintypes.DWORD)
    ]

def spawn_on_desktop():
    subprocess.run(["taskkill", "/F", "/IM", "likhi_universal.exe"], capture_output=True)
    time.sleep(0.5)

    si = STARTUPINFOW()
    si.cb = ctypes.sizeof(STARTUPINFOW)
    si.lpDesktop = "WinSta0\\Default"

    pi = PROCESS_INFORMATION()

    exe = os.path.expandvars(r"C:\Users\Pervez\AppData\Roaming\PC-Bangla-Typing-App\likhi_universal.exe")
    cmd = f'"{exe}"'

    flags = 0x00000200 # CREATE_NEW_PROCESS_GROUP

    success = kernel32.CreateProcessW(
        None, cmd, None, None, False,
        flags, None, None, ctypes.byref(si), ctypes.byref(pi)
    )

    if success:
        print(f"Spawned Likhi on WinSta0\\Default PID: {pi.dwProcessId}")
        sys.stdout.flush()
        kernel32.CloseHandle(pi.hThread)
        return pi.hProcess, pi.dwProcessId
    else:
        print(f"Failed to spawn on WinSta0\\Default: {kernel32.GetLastError()}")
        sys.stdout.flush()
        return None, 0

hProcess, pid = spawn_on_desktop()

while True:
    time.sleep(1)
    if hProcess:
        res = kernel32.WaitForSingleObject(hProcess, 0)
        if res == 0: # WAIT_OBJECT_0: exited
            kernel32.CloseHandle(hProcess)
            print("Process exited, respawning...")
            sys.stdout.flush()
            time.sleep(1)
            hProcess, pid = spawn_on_desktop()
