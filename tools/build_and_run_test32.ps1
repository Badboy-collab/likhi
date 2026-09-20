# tools/build_and_run_test32.ps1
$ErrorActionPreference = "Stop"

$cxx = "C:/tools/mingw32/bin/g++.exe"
$rootDir = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path.Replace("\", "/")
$buildDir = "$rootDir/build"
$objDir = "$buildDir/obj32"

$inc = @(
    "-I$rootDir/engine/include",
    "-I$rootDir/engine/src",
    "-I$rootDir/tsf/include"
)
$flags = @("-O3", "-std=c++20", "-DNDEBUG")

# Reuse the already compiled engine object files from objDir!
$engineObjs = @(
    "$objDir/bangla_unicode.obj",
    "$objDir/phonetic_parser.obj",
    "$objDir/lexicon_trie.obj",
    "$objDir/context_ranker.obj",
    "$objDir/personal_dictionary.obj",
    "$objDir/inscript_layout.obj",
    "$objDir/global_model_reader.obj",
    "$objDir/bangla_engine.obj",
    "$objDir/composition_mgr.obj",
    "$objDir/edit_session.obj",
    "$objDir/candidate_window.obj"
)

Write-Host "Compiling test_tsf_integration.cpp (32-bit)..."
$testObj = "$objDir/test_tsf_integration.obj"
& $cxx @flags @inc -c "$rootDir/tests/tsf_integration/test_tsf_integration.cpp" -o $testObj
if ($LASTEXITCODE -ne 0) {
    Write-Error "Failed to compile test_tsf_integration.cpp"
    exit 1
}

$outExe = "$buildDir/test_tsf_integration32.exe"
Write-Host "Linking $outExe ..."
& $cxx -O3 -static -static-libgcc -static-libstdc++ $testObj @engineObjs -o $outExe -lole32 -loleaut32 -luuid -luser32 -lgdi32 -lcomctl32 -ladvapi32 -lshell32
if ($LASTEXITCODE -ne 0) {
    Write-Error "Failed to link $outExe"
    exit 1
}

Write-Host "Running $outExe ..." -ForegroundColor Cyan
& $outExe
