# Build script for 32-bit TSF DLL (bangla_tsf32.dll)
$ErrorActionPreference = "Stop"

$cxx = "C:/tools/mingw32/bin/g++.exe"
$windres = "C:/tools/mingw32/bin/windres.exe"

if (!(Test-Path $cxx)) {
    Write-Error "32-bit g++ not found at $cxx"
    exit 1
}

$rootDir = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path.Replace("\", "/")
$buildDir = "$rootDir/build"
$objDir = "$buildDir/obj32"
New-Item -ItemType Directory -Force -Path $objDir | Out-Null

$includes = @(
    "-I$rootDir/engine/include",
    "-I$rootDir/engine/src",
    "-I$rootDir/tsf/include"
)

$flags = @(
    "-O3",
    "-std=c++20",
    "-DNDEBUG",
    "-Wall",
    "-Wextra",
    "-Dbangla_tsf_EXPORTS"
)

Write-Host "Compiling 32-bit resource..." -ForegroundColor Cyan
$rcObj = "$objDir/tsf_rc32.obj"
$rcSrc = "$rootDir/tsf/src/tsf.rc"
& $windres -i $rcSrc -o $rcObj
if ($LASTEXITCODE -ne 0) {
    Write-Error "windres failed"
    exit 1
}

$engineSources = @(
    "engine/src/unicode/bangla_unicode.cpp",
    "engine/src/transliteration/phonetic_parser.cpp",
    "engine/src/dictionary/lexicon_trie.cpp",
    "engine/src/ranking/context_ranker.cpp",
    "engine/src/personal_dict/personal_dictionary.cpp",
    "engine/src/layout/inscript_layout.cpp",
    "engine/src/global_model/global_model_reader.cpp",
    "engine/src/bangla_engine.cpp"
)

$tsfSources = @(
    "tsf/src/dll_main.cpp",
    "tsf/src/class_factory.cpp",
    "tsf/src/text_service.cpp",
    "tsf/src/key_event_sink.cpp",
    "tsf/src/key_policy.cpp",
    "tsf/src/composition_mgr.cpp",
    "tsf/src/edit_session.cpp",
    "tsf/src/candidate_window.cpp",
    "tsf/src/register.cpp"
)

$allSources = $engineSources + $tsfSources
$objs = @($rcObj)

foreach ($relSrc in $allSources) {
    $fullSrc = "$rootDir/$relSrc"
    $name = [System.IO.Path]::GetFileNameWithoutExtension($relSrc)
    $outObj = "$objDir/$name.obj"
    $objs += $outObj
    
    if (!(Test-Path $outObj) -or ((Get-Item $fullSrc).LastWriteTime -gt (Get-Item $outObj).LastWriteTime)) {
        Write-Host "Compiling $relSrc (32-bit)..."
        & $cxx @flags @includes -c $fullSrc -o $outObj
        if ($LASTEXITCODE -ne 0) {
            Write-Error "Compilation failed for $relSrc"
            exit 1
        }
    } else {
        Write-Host "Up-to-date: $relSrc"
    }
}

$defFile = "$rootDir/tsf/src/bangla_tsf.def"
$outDll = "$buildDir/bangla_tsf32.dll"

Write-Host "Linking $outDll ..." -ForegroundColor Cyan
& $cxx -shared -O3 -static -static-libgcc -static-libstdc++ "-Wl,--kill-at" "-Wl,--enable-stdcall-fixup" $defFile @objs -o $outDll -lole32 -loleaut32 -luuid -luser32 -lgdi32 -lcomctl32 -ladvapi32 -lshell32
if ($LASTEXITCODE -ne 0) {
    Write-Error "Link failed for $outDll"
    exit 1
}

Write-Host "SUCCESS: Built $outDll" -ForegroundColor Green
