param([switch]$Check)
$ErrorActionPreference = 'Stop'
$controllerPython = Join-Path $env:LOCALAPPDATA 'Programs\Python\Python313\python.exe'
$serialPackages = Join-Path $env:USERPROFILE '.platformio\penv\Lib\site-packages'
if (-not (Test-Path -LiteralPath $controllerPython)) {
    $controllerPython = (Get-Command python -ErrorAction Stop).Source
}
$scriptName = if ($Check) { 'check_control.py' } else { 'control.py' }
& $controllerPython -c 'import sys, runpy; sys.path.append(sys.argv[1]); sys.path.insert(0, sys.argv[2]); runpy.run_path(sys.argv[3], run_name=sys.argv[4])' $serialPackages $PSScriptRoot (Join-Path $PSScriptRoot $scriptName) '__main__'
exit $LASTEXITCODE
