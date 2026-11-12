param([string]$QtRoot = '', [string]$CompilerBin = '', [switch]$Package)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
if (!$QtRoot) { $QtRoot = Join-Path $projectRoot '.codex_tmp/Qt/6.8.3/mingw_64' }
if (!(Test-Path (Join-Path $QtRoot 'lib/cmake/Qt6/Qt6Config.cmake'))) {
    throw 'Qt SDK не найден. Укажите -QtRoot C:/Qt/6.8.3/mingw_64 (Qt Quick, Quick Controls 2, Serial Port).'
}
if (!$CompilerBin) {
    $localCompiler = Join-Path $projectRoot '.codex_tmp/Qt/Tools/mingw1310_64/bin'
    if (Test-Path -LiteralPath $localCompiler) { $CompilerBin = $localCompiler }
}
if ($CompilerBin) { $env:PATH = "$CompilerBin;$env:PATH" }
$env:PATH = "$(Join-Path $QtRoot 'bin');$env:PATH"
$buildPath = Join-Path $PSScriptRoot 'build'
cmake -S $PSScriptRoot -B $buildPath -G 'MinGW Makefiles' "-DCMAKE_PREFIX_PATH=$QtRoot" -DCMAKE_BUILD_TYPE=Debug
if ($LASTEXITCODE) { throw 'Ошибка конфигурации CMake' }
cmake --build $buildPath --parallel 4
if ($LASTEXITCODE) { throw 'Ошибка сборки' }
ctest --test-dir $buildPath --output-on-failure
if ($LASTEXITCODE) { throw 'Ошибка тестов' }
if ($Package) {
    $distPath = Join-Path $PSScriptRoot 'dist'
    New-Item -ItemType Directory -Force $distPath | Out-Null
    Copy-Item -LiteralPath (Join-Path $buildPath 'SelfGuitar.exe') -Destination $distPath
    & (Join-Path $QtRoot 'bin/windeployqt.exe') --qmldir (Join-Path $PSScriptRoot 'qml') --no-translations (Join-Path $distPath 'SelfGuitar.exe')
    if ($LASTEXITCODE) { throw 'Ошибка упаковки Qt' }
    Write-Output "Готово: $distPath/SelfGuitar.exe"
}
