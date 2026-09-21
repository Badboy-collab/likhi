# Likhi (লিখি) - Register TIP into Windows Modern Language List (Win + Space)
[CmdletBinding()]
param()

$bdTip = "0845:{B4F1470A-7C69-4C62-972F-6379532856E1}{D85B64E2-0D5C-40EE-BE15-1E7C146603F2}"
$usTip = "0409:{B4F1470A-7C69-4C62-972F-6379532856E1}{D85B64E2-0D5C-40EE-BE15-1E7C146603F2}"
$bdGenericTip = "0845:00000445"

try {
    $list = Get-WinUserLanguageList

    # 1. Clean en-US: keep only standard US keyboard, remove any Likhi from English
    $us = $list | Where-Object { $_.LanguageTag -eq "en-US" }
    if ($us) {
        if ($us.InputMethodTips -contains $usTip) {
            $us.InputMethodTips.Remove($usTip) | Out-Null
        }
    }

    # 2. Configure Bengali (Bangladesh): only Likhi, remove generic Windows Bangla
    $bn = $list | Where-Object { $_.LanguageTag -eq "bn-BD" }
    if (-not $bn) {
        $bnList = New-WinUserLanguageList "bn-BD"
        $list.Add($bnList[0])
        $bn = $list | Where-Object { $_.LanguageTag -eq "bn-BD" }
    }
    if ($bn) {
        if ($bn.InputMethodTips -contains $bdGenericTip) {
            $bn.InputMethodTips.Remove($bdGenericTip) | Out-Null
        }
        if (-not ($bn.InputMethodTips -contains $bdTip)) {
            $bn.InputMethodTips.Add($bdTip)
        }
    }

    # 3. Clean any ghost UK English layouts by aligning UI language override to en-US
    Set-WinUILanguageOverride -Language "en-US" -ErrorAction SilentlyContinue
    Set-Culture "en-US" -ErrorAction SilentlyContinue

    # 4. Unload any lingering ghost keyboard layouts via Win32 API
    $unloaderCode = @"
using System;
using System.Runtime.InteropServices;
public class GhostKbdCleaner {
    [DllImport("user32.dll")]
    public static extern bool UnloadKeyboardLayout(IntPtr hkl);
    [DllImport("user32.dll")]
    public static extern uint GetKeyboardLayoutList(int nBuff, [Out] IntPtr[] lpList);
    public static void Clean() {
        uint count = GetKeyboardLayoutList(0, null);
        if (count == 0) return;
        IntPtr[] list = new IntPtr[count];
        GetKeyboardLayoutList((int)count, list);
        for (int i = 0; i < count; i++) {
            long hkl = list[i].ToInt64();
            long lang = hkl & 0xFFFF;
            if (lang != 0x0409 && lang != 0x0845 && lang != 0x0445) {
                UnloadKeyboardLayout(list[i]);
            }
        }
    }
}
"@
    Add-Type -TypeDefinition $unloaderCode -ErrorAction SilentlyContinue
    [GhostKbdCleaner]::Clean()

    # 5. Restart TextInputHost to refresh UI flyout
    Stop-Process -Name "TextInputHost" -Force -ErrorAction SilentlyContinue
    Stop-Process -Name "ctfmon" -Force -ErrorAction SilentlyContinue
    Start-Process "ctfmon.exe"

    Write-Output "SUCCESS: Likhi TIP cleanly configured (Default US English + Likhi Bangla)."
} catch {
    Write-Warning "Failed to set WinUserLanguageList: $_"
}
