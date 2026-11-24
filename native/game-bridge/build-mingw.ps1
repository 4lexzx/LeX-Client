param(
  [string]$MinGW = (Join-Path $env:TEMP 'opencode\winlibs\x\mingw64\bin')
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$gcc = Join-Path $MinGW 'gcc.exe'
$gpp = Join-Path $MinGW 'g++.exe'
if (-not (Test-Path $gcc)) { throw "No se encontró $gcc" }

$outDir = Join-Path $root 'dist'
New-Item -ItemType Directory -Force -Path $outDir | Out-Null
$objDir = Join-Path $outDir 'obj'
New-Item -ItemType Directory -Force -Path $objDir | Out-Null

$inc = @(
  '-I', (Join-Path $root 'third_party\minhook\include'),
  '-I', (Join-Path $root 'third_party\minhook\src')
)
$defs = @('-DWIN32', '-D_WIN32_WINNT=0x0A00', '-DNOMINMAX', '-DWIN32_LEAN_AND_MEAN')
$objs = @()

$cSources = Get-ChildItem (Join-Path $root 'third_party\minhook\src') -Recurse -Filter '*.c'
foreach ($src in $cSources) {
  $obj = Join-Path $objDir ($src.BaseName + '_' + ([guid]::NewGuid().ToString('N').Substring(0, 6)) + '.o')
  & $gcc -m64 -std=c11 -O2 -masm=intel -c @inc @defs -o $obj $src.FullName
  if ($LASTEXITCODE -ne 0) { throw "Falló MinHook: $($src.FullName)" }
  $objs += $obj
}

$dllObj = Join-Path $objDir 'dll.o'
& $gpp -m64 -std=c++20 -O2 -Wall -Wno-unused-function -masm=intel -c @inc @defs -o $dllObj (Join-Path $root 'src\dll.cpp')
if ($LASTEXITCODE -ne 0) { throw 'Falló la compilación de dll.cpp' }

# En UWP un DLL con statically-linked CRT inicializa antes de que kernelbase
# tenga el heap listo y LoadLibrary falla con 126. El CRT dinámico evita eso.
$dll = Join-Path $outDir 'lex-game-bridge.dll'
& $gpp -m64 -shared -o $dll $dllObj @objs `
  -ld3d11 -ldxgi -ld3dcompiler -luser32 -lkernel32 -ladvapi32 -lshell32 -lstdc++
if ($LASTEXITCODE -ne 0) { throw 'Falló el enlazado del DLL' }

# Cargador mínimo (sólo kernel32/msvcrt): se inyecta primero dentro del UWP.
$loader = Join-Path $outDir 'lex-bridge-loader.dll'
& $gcc -m64 -shared -O2 -o $loader (Join-Path $root 'src\bootstrap.c') -lkernel32
if ($LASTEXITCODE -ne 0) { throw 'Falló el cargador' }

Write-Host "OK: $dll"
Write-Host "OK: $loader"
