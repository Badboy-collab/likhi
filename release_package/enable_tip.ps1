# Likhi (লিখি) - Register TIP into Windows Modern Language List (Win + Space)
[CmdletBinding()]
param()

$bdTip = "0845:{B4F1470A-7C69-4C62-972F-6379532856E1}{D85B64E2-0D5C-40EE-BE15-1E7C146603F2}"
$usTip = "0409:{B4F1470A-7C69-4C62-972F-6379532856E1}{D85B64E2-0D5C-40EE-BE15-1E7C146603F2}"

try {
    $list = Get-WinUserLanguageList

    # 1. Add/Configure Bengali (Bangladesh)
    $bn = $list | Where-Object { $_.LanguageTag -eq "bn-BD" }
    if (-not $bn) {
        $bnList = New-WinUserLanguageList "bn-BD"
        $list.Add($bnList[0])
        $bn = $list | Where-Object { $_.LanguageTag -eq "bn-BD" }
    }
    if ($bn -and -not ($bn.InputMethodTips -contains $bdTip)) {
        $bn.InputMethodTips.Add($bdTip)
    }

    # 2. Configure under en-US as well for convenient Win+Space switching
    $us = $list | Where-Object { $_.LanguageTag -eq "en-US" }
    if ($us -and -not ($us.InputMethodTips -contains $usTip)) {
        $us.InputMethodTips.Add($usTip)
    }

    Set-WinUserLanguageList $list -Force
    Write-Output "SUCCESS: Likhi TIP added to Windows Language List."
} catch {
    Write-Warning "Failed to set WinUserLanguageList: $_"
}
