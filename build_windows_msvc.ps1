$ErrorActionPreference = "Stop"
$SourceDir = $PSScriptRoot

# --- Stale build directory guard ---
# CMakeCache.txt records the absolute source path, and QUANTILOOM_SDK_ROOT /
# Quantiloom_DIR are cached alongside it. After the tree is moved to another
# drive or folder the old cache still points at the previous location - and at
# the previous Quantiloom-SDK - so configure either fails oddly or silently
# links the SDK from the old drive. Drop any tree whose cache has drifted.
function Remove-StaleBuildDir([string]$Dir) {
    $cacheFile = Join-Path $Dir "CMakeCache.txt"
    if (-not (Test-Path $cacheFile)) { return }
    $line = Select-String -Path $cacheFile -Pattern "^CMAKE_HOME_DIRECTORY:INTERNAL=(.*)$" | Select-Object -First 1
    if (-not $line) { return }
    $cached = $line.Matches[0].Groups[1].Value.Replace("/", "\").TrimEnd("\")
    $actual = $SourceDir.Replace("/", "\").TrimEnd("\")
    if ($cached -ne $actual) {
        Write-Host "Stale build cache in $Dir ($cached != $actual) - removing." -ForegroundColor Yellow
        Remove-Item -LiteralPath $Dir -Recurse -Force
    }
}
Remove-StaleBuildDir (Join-Path $SourceDir "build-msvc")
Remove-StaleBuildDir (Join-Path $SourceDir "build-cdb")

cmake -B build-msvc -G "Visual Studio 18 2026" -A x64 -DCMAKE_PREFIX_PATH="C:\\Qt\\6.10.1\\msvc2022_64"

#cmake --build build --config Debug -j
cmake --build build-msvc --config Release -j