# Build Skia for cpp-ui-toolkit on Windows.
#
#   powershell -ExecutionPolicy Bypass -File tools\build_skia.ps1 cpu   -> third_party\skia\out\cpu  (raster only)
#   powershell -ExecutionPolicy Bypass -File tools\build_skia.ps1 gpu   -> third_party\skia\out\gpu  (Ganesh + WGL/OpenGL)
#
# Same pinned branch and module set as tools/build_skia.sh. Differences from Linux:
#   - Fonts come from DirectWrite (no fontconfig/FreeType).
#   - HarfBuzz, ICU, libpng, libjpeg-turbo, libwebp, zlib and expat are built from Skia's bundled
#     copies (there are no system packages). ICU reads icudtl.dat next to the .exe at runtime;
#     CMake copies it there.
#   - Compiled with clang-cl (Skia is much faster with it than with cl.exe) against the dynamic
#     release CRT (/MD) and with RTTI on, so it links with MSVC-built apps in any configuration.
#
# Requires: Visual Studio 2022 (or Build Tools) with "Desktop development with C++",
#           LLVM (clang-cl; "C++ Clang tools" VS component or https://llvm.org), git, Python 3.
# Run it from any shell; it finds the tools itself. Takes 15-30 minutes per configuration.
param(
    [Parameter(Mandatory = $true)][ValidateSet("cpu", "gpu")][string]$Config,
    [string]$ClangWin = ""
)
$ErrorActionPreference = "Stop"

$SkiaBranch = "chrome/m157"
$Root = Resolve-Path (Join-Path $PSScriptRoot "..")
$SkiaDir = Join-Path $Root "third_party\skia"
$Src = Join-Path $SkiaDir "src"
$Out = Join-Path $SkiaDir "out\$Config"

function Need($cmd, $hint) {
    if (-not (Get-Command $cmd -ErrorAction SilentlyContinue)) { throw "'$cmd' not found. $hint" }
}
Need git "Install Git for Windows."
$Python = if (Get-Command python3 -ErrorAction SilentlyContinue) { "python3" } else { "python" }
Need $Python "Install Python 3 and add it to PATH."

# LLVM: an explicit path, the VS bundled copy, or a standalone install.
if (-not $ClangWin) {
    $candidates = @("C:\Program Files\LLVM")
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vswhere) {
        $vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
        if ($vs) { $candidates = @("$vs\VC\Tools\Llvm\x64") + $candidates }
    }
    foreach ($c in $candidates) { if (Test-Path "$c\bin\clang-cl.exe") { $ClangWin = $c; break } }
}
if (-not $ClangWin) { throw "clang-cl not found. Install LLVM or the VS 'C++ Clang tools' component, or pass -ClangWin <dir>." }
Write-Host "==> using clang-cl from $ClangWin"

if (-not (Test-Path (Join-Path $Src ".git"))) {
    Write-Host "==> cloning Skia ($SkiaBranch)"
    git clone --depth 1 --branch $SkiaBranch https://skia.googlesource.com/skia.git $Src
}

Push-Location $Src
try {
    if (-not (Test-Path ".uitk-deps-synced")) {
        Write-Host "==> syncing Skia third-party deps"
        & $Python tools/git-sync-deps
        if ($LASTEXITCODE) { throw "git-sync-deps failed" }
        New-Item -ItemType File ".uitk-deps-synced" | Out-Null
    }
    if (-not (Test-Path "bin\gn.exe")) { & $Python bin/fetch-gn; if ($LASTEXITCODE) { throw "fetch-gn failed" } }
    $Ninja = (Get-Command ninja -ErrorAction SilentlyContinue).Source
    if (-not $Ninja) {
        if (-not (Test-Path "bin\ninja.exe")) { & $Python bin/fetch-ninja; if ($LASTEXITCODE) { throw "fetch-ninja failed" } }
        $Ninja = Join-Path $Src "bin\ninja.exe"
    }

    $clang = $ClangWin -replace '\\', '/'
    $gnArgs = @"
clang_win="$clang"
is_official_build=true
is_debug=false
target_cpu="x64"
extra_cflags=["/MD"]
extra_cflags_cc=["/GR"]
skia_use_system_expat=false
skia_use_system_harfbuzz=false
skia_use_system_icu=false
skia_use_system_libjpeg_turbo=false
skia_use_system_libpng=false
skia_use_system_libwebp=false
skia_use_system_zlib=false
skia_use_fontconfig=false
skia_use_freetype=false
skia_use_icu=true
skia_use_harfbuzz=true
skia_enable_fontmgr_win=true
skia_enable_svg=true
skia_enable_skshaper=true
skia_enable_skparagraph=true
skia_enable_pdf=false
skia_use_xps=false
skia_use_dng_sdk=false
skia_use_piex=false
skia_enable_graphite=false
skia_use_vulkan=false
skia_use_direct3d=false
"@
    if ($Config -eq "cpu") { $gnArgs += "`nskia_enable_ganesh=false`nskia_use_gl=false" }
    else { $gnArgs += "`nskia_enable_ganesh=true`nskia_use_gl=true" }

    Write-Host "==> gn gen $Out"
    & bin\gn.exe gen $Out "--args=$($gnArgs -replace "`r?`n", ' ')"
    if ($LASTEXITCODE) { throw "gn gen failed" }

    Write-Host "==> ninja ($Config)"
    & $Ninja -C $Out skia skshaper skparagraph skunicode_core skunicode_icu svg
    if ($LASTEXITCODE) { throw "ninja failed" }
} finally {
    Pop-Location
}

Write-Host "==> done: $Out"
Get-ChildItem "$Out\*.lib", "$Out\icudtl.dat" | Format-Table Name, Length
