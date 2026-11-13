# Testy na PC z PowerShell (Windows): make test z gcc i make z msys64.
#   powershell -ExecutionPolicy Bypass -File tools\test.ps1          pełny wynik w build\test.log, na ekranie podsumowanie i FAIL-e
#   powershell -ExecutionPolicy Bypass -File tools\test.ps1 clean    najpierw make clean
param([string]$Action)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$msys = if ($env:MSYS64) { $env:MSYS64 } else { 'C:\msys64' }
$env:Path = "$msys\mingw64\bin;$msys\usr\bin;$env:Path"
# Makefile i tak ustawia TMP na build/tmp; to jest na wypadek wywołań spoza niego.
$env:TMP = "$env:USERPROFILE\AppData\Local\Temp"
$env:TEMP = $env:TMP

Push-Location $root
try {
    # cmd.exe zbiera stdout i stderr make do pliku (PowerShell 5.1 traktuje stderr gcc jak błąd i przerywa skrypt).
    if ($Action -eq 'clean') { & cmd.exe /c "make clean >nul 2>&1" }
    New-Item -ItemType Directory -Force build | Out-Null
    $log = Join-Path $root 'build\test.log'
    & cmd.exe /c "make test > `"$log`" 2>&1"
    $code = $LASTEXITCODE
    Get-Content $log | Where-Object { $_ -match '^(FAIL|passed:)|error' }
    if ($code -ne 0) { Write-Host "make test: kod $code (szczegoly: build\test.log)" }
    exit $code
} finally { Pop-Location }
