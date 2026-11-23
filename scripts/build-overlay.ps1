# LEX CLIENT — Build del Overlay Engine (C++, MSVC). Fallback: MiniGW.
Write-Host "=== build-overlay ==="
Push-Location $PSScriptRoot/../native/overlay-engine
try {
  $cmake = Get-Command cmake -ErrorAction SilentlyContinue
  if ($cmake) {
    Write-Host "-> cmake -S . -B build"
    cmake -S . -B build
    Write-Host "-> cmake --build build --config Release"
    cmake --build build --config Release
  } else {
    Write-Host "x cmake no encontrado"
  }
} finally {
  Pop-Location
}
