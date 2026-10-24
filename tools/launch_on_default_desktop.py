import ctypes
from ctypes import wintypes
import os
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

subprocess.run(["taskkill", "/F", "/IM", "likhi_universal.exe"], capture_output=True)

si = STARTUPINFOW()
si.cb = ctypes.sizeof(STARTUPINFOW)
si.lpDesktop = "WinSta0\\Default"

pi = PROCESS_INFORMATION()

exe = os.path.expandvars(r"C:\Users\Pervez\AppData\Roaming\PC-Bangla-Typing-App\likhi_universal.exe")
cmd = f'"{exe}"'

# CREATE_NEW_PROCESS_GROUP = 0x00000200, DETACHED_PROCESS = 0x00000008
flags = 0x00000200 | 0x00000008

success = kernel32.CreateProcessW(
    None, cmd, None, None, False,
    flags, None, None, ctypes.byref(si), ctypes.byref(pi)
)

print(f"CreateProcess on WinSta0\\Default result: {success}")
if success:
    print(f"Spawned PID: {pi.dwProcessId}")
    kernel32.CloseHandle(pi.hProcess)
    kernel32.CloseHandle(pi.hThread)
else:
    print(f"Failed with error: {kernel32.GetLastError()}")
