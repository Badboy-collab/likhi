# tools/test_com32.ps1
$clsid = [Guid]"{B4F1470A-7C69-4C62-972F-6379532856E1}"
Write-Host "Process bitness: $([IntPtr]::Size * 8)-bit"
try {
    $type = [Type]::GetTypeFromCLSID($clsid, $true)
    $obj = [Activator]::CreateInstance($type)
    Write-Host "SUCCESS: Created instance $obj" -ForegroundColor Green
} catch {
    Write-Host "FAILED: $_" -ForegroundColor Red
}
