# Likhi (লিখি) - Unregister TIP from Windows Modern Language List (Win + Space)
[CmdletBinding()]
param()

$bdTip = "0845:{B4F1470A-7C69-4C62-972F-6379532856E1}{D85B64E2-0D5C-40EE-BE15-1E7C146603F2}"
$usTip = "0409:{B4F1470A-7C69-4C62-972F-6379532856E1}{D85B64E2-0D5C-40EE-BE15-1E7C146603F2}"

try {
    $list = Get-WinUserLanguageList
    $changed = $false

    foreach ($lang in $list) {
        if ($lang.InputMethodTips -contains $bdTip) {
            $lang.InputMethodTips.Remove($bdTip) | Out-Null
            $changed = $true
        }
        if ($lang.InputMethodTips -contains $usTip) {
            $lang.InputMethodTips.Remove($usTip) | Out-Null
            $changed = $true
        }
    }

    if ($changed) {
        Set-WinUserLanguageList $list -Force
        Write-Output "SUCCESS: Likhi TIP removed from Windows Language List."
    } else {
        Write-Output "INFO: Likhi TIP was not present in language list."
    }
} catch {
    Write-Warning "Failed to update WinUserLanguageList: $_"
}
