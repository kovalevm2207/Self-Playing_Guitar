$ErrorActionPreference = 'Stop'
$softwareRoot = Split-Path $PSScriptRoot -Parent
$executable = Join-Path $softwareRoot 'build/windows/package/SelfGuitar.exe'
if (!(Test-Path -LiteralPath $executable)) {
    & (Join-Path $PSScriptRoot 'build-windows.ps1') -Package
}
& $executable
