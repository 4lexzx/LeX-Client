# Construye LeX-Client-Android.apk sin Gradle: NDK para la parte nativa,
# javac + d8 para Java, aapt2 para el manifiesto y los assets, y apksigner.
# Los avisos en stderr no son errores: se mira el codigo de salida de cada paso.
$ErrorActionPreference = 'Continue'
$root = $PSScriptRoot
$sdk = "$env:LOCALAPPDATA\Android\Sdk"
$ndk = Get-ChildItem "$sdk\ndk" | Sort-Object Name -Descending | Select-Object -First 1
$cmake = Get-ChildItem "$sdk\cmake" | Sort-Object Name -Descending | Select-Object -First 1
$tools = Get-ChildItem "$sdk\build-tools" | Sort-Object Name -Descending | Select-Object -First 1
$platform = Get-ChildItem "$sdk\platforms" | Sort-Object Name -Descending | Select-Object -First 1
$jdk = 'C:\Program Files\Android\Android Studio\jbr'
$out = Join-Path $root 'build'
$stage = Join-Path $out 'apk'
Remove-Item $stage -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force "$stage\lib\arm64-v8a", "$out\classes" | Out-Null

# 1. Assets: el pack de LeX y los cosmeticos, generados con el codigo del launcher.
& node (Join-Path $root 'assets.mjs') (Join-Path $out 'assets'); if ($LASTEXITCODE) { throw 'Fallo al generar los assets' }

# 2. Nativo (arm64-v8a).
$native = Join-Path $out 'native'
& "$($cmake.FullName)\bin\cmake.exe" -S "$root\cpp" -B $native -G Ninja "-DCMAKE_MAKE_PROGRAM=$($cmake.FullName)\bin\ninja.exe" `
  "-DCMAKE_TOOLCHAIN_FILE=$($ndk.FullName)\build\cmake\android.toolchain.cmake" -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-29 `
  -DANDROID_STL=c++_static -DCMAKE_BUILD_TYPE=Release | Out-Null
if ($LASTEXITCODE) { throw 'Fallo la configuracion nativa' }
& "$($cmake.FullName)\bin\cmake.exe" --build $native; if ($LASTEXITCODE) { throw 'Fallo la compilacion nativa' }
Copy-Item "$native\libminecraftpe.so" "$stage\lib\arm64-v8a\libminecraftpe.so"

# 3. Java.
$android = "$($platform.FullName)\android.jar"
$sources = Get-ChildItem "$root\java" -Recurse -Filter *.java | ForEach-Object { $_.FullName }
# Java 8 sobre android.jar (el que usa el propio Android para sus clases).
& "$jdk\bin\javac.exe" -source 8 -target 8 -Xlint:-options -encoding UTF-8 -bootclasspath $android -d "$out\classes" @sources
if ($LASTEXITCODE) { throw 'Fallo javac' }
$classes = Get-ChildItem "$out\classes" -Recurse -Filter *.class | ForEach-Object { $_.FullName }
& "$($tools.FullName)\d8.bat" --release --min-api 29 --lib $android --output $stage @classes
if ($LASTEXITCODE) { throw 'Fallo d8' }

# 4. Manifiesto, icono y assets.
$res = Join-Path $out 'res'
& "$($tools.FullName)\aapt2.exe" compile --dir "$root\res" -o "$out\res.zip"; if ($LASTEXITCODE) { throw 'Fallo aapt2 compile' }
& "$($tools.FullName)\aapt2.exe" link -o "$out\base.apk" -I $android --manifest "$root\AndroidManifest.xml" "$out\res.zip" --auto-add-overlay
if ($LASTEXITCODE) { throw 'Fallo aapt2 link' }

# 5. Clases y librerias dentro del APK, alineado y firmado.
Copy-Item "$out\base.apk" "$out\unsigned.apk" -Force
# Los assets con jar: aapt2 en Windows guarda las rutas con "\" y Android no las encuentra.
& "$jdk\bin\jar.exe" -uf "$out\unsigned.apk" -C $stage classes.dex -C $stage lib -C $out assets
if ($LASTEXITCODE) { throw 'Fallo al empaquetar' }
& "$($tools.FullName)\zipalign.exe" -f -p 4 "$out\unsigned.apk" "$out\aligned.apk"; if ($LASTEXITCODE) { throw 'Fallo zipalign' }
$key = Join-Path $root 'lex-debug.keystore'
if (-not (Test-Path $key)) {
  & "$jdk\bin\keytool.exe" -genkeypair -keystore $key -storepass lexclient -keypass lexclient -alias lex -keyalg RSA -keysize 2048 -validity 10000 -dname "CN=LeX Client" | Out-Null
}
$apk = Join-Path $out 'LeX-Client-Android.apk'
& "$($tools.FullName)\apksigner.bat" sign --ks $key --ks-pass pass:lexclient --key-pass pass:lexclient --out $apk "$out\aligned.apk"
if ($LASTEXITCODE) { throw 'Fallo la firma' }
& "$($tools.FullName)\apksigner.bat" verify $apk; if ($LASTEXITCODE) { throw 'Firma no valida' }
Write-Host "Listo: $apk"



