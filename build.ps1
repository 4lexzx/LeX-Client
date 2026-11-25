$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
$build = Join-Path $root 'client\build'
$ca = 'C:/Program Files/Git/mingw64/etc/ssl/certs/ca-bundle.crt'
if (-not (Get-Command g++ -ErrorAction SilentlyContinue)) {
  $gpp = Get-ChildItem "$env:LOCALAPPDATA\Microsoft\WinGet\Packages\BrechtSanders.WinLibs*\mingw64\bin\g++.exe" -ErrorAction SilentlyContinue | Select-Object -First 1
  if (-not $gpp) { throw 'No se encontro g++ (instala WinLibs).' }
  $env:PATH = "$($gpp.DirectoryName);$env:PATH"
}
$cmakeArgs = @('-S', "$root\client", '-B', $build, '-G', 'Ninja', '-DCMAKE_BUILD_TYPE=Release', '-DCMAKE_C_COMPILER=gcc', '-DCMAKE_CXX_COMPILER=g++')
# El CMake de WinLibs no trae certificados; se usan los de Git.
if (Test-Path $ca) { $cmakeArgs += "-DCMAKE_TLS_CAINFO=$ca" }
cmake @cmakeArgs; if ($LASTEXITCODE) { throw 'Fallo la configuracion' }
cmake --build $build; if ($LASTEXITCODE) { throw 'Fallo la compilacion' }
ctest --test-dir $build --output-on-failure; if ($LASTEXITCODE) { throw 'Fallaron las pruebas' }
$native = Join-Path $root 'launcher\native'
New-Item -ItemType Directory -Force $native | Out-Null
Copy-Item "$build\LeXClient.dll", "$build\lex-injector.exe" $native -Force
& "$build\lex-catalog.exe" "$root\launcher\shared\modules.json"; if ($LASTEXITCODE) { throw 'Fallo el catalogo' }
Write-Host 'Listo.'
