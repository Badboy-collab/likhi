# tools/run_all_tests.ps1
$ErrorActionPreference = "Continue"

$rootDir = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$buildDir = Join-Path $rootDir "build"

$tests = @(
    "test_runner.exe",
    "test_key_policy.exe",
    "test_p0_keyboard.exe",
    "test_tsf_integration.exe",
    "unicode_bengali_tests.exe",
    "test_roadmap_stage1.exe",
    "real_world_qa_runner.exe",
    "test_global_model_reader.exe",
    "test_tsf_integration32.exe"
)

$summary = @()
$failedCount = 0

foreach ($t in $tests) {
    $exePath = Join-Path $buildDir $t
    if (!(Test-Path $exePath)) {
        Write-Warning "Skipping missing test: $t"
        continue
    }

    Write-Host "=========================================================" -ForegroundColor Cyan
    Write-Host " Running $t ..." -ForegroundColor Cyan
    Write-Host "=========================================================" -ForegroundColor Cyan
    
    & $exePath
    $code = $LASTEXITCODE
    if ($code -eq 0) {
        Write-Host " [PASS] $t" -ForegroundColor Green
        $summary += [PSCustomObject]@{ Test = $t; Result = "PASS" }
    } else {
        Write-Host " [FAIL] $t (exit code $code)" -ForegroundColor Red
        $summary += [PSCustomObject]@{ Test = $t; Result = "FAIL" }
        $failedCount++
    }
}

Write-Host "`n=================== TEST SUMMARY ===================" -ForegroundColor Yellow
$summary | Format-Table -AutoSize

if ($failedCount -eq 0) {
    Write-Host "ALL $(@($summary).Count) TEST SUITES PASSED! ZERO FAILURES!" -ForegroundColor Green
    exit 0
} else {
    Write-Error "$failedCount TEST SUITE(S) FAILED!"
    exit 1
}
