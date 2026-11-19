param([switch]$Check)
$ErrorActionPreference = 'Stop'
$benchDirectory = Split-Path -Parent $PSScriptRoot
$controllerPython = Join-Path $benchDirectory '.venv\Scripts\python.exe'
if (-not (Test-Path -LiteralPath $controllerPython)) {
    throw 'Create the solenoid .venv and install tools/requirements.txt first. See bench/solenoid/README.md.'
}
$scriptName = if ($Check) { 'check_control.py' } else { 'control.py' }
& $controllerPython (Join-Path $PSScriptRoot $scriptName)
exit $LASTEXITCODE
