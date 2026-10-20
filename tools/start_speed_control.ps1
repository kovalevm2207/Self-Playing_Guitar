param([switch]$Calibration)
$ErrorActionPreference = 'Stop'
$controllerPython = Join-Path $env:LOCALAPPDATA 'Programs\Python\Python313\python.exe'
$serialPackages = Join-Path $env:USERPROFILE '.platformio\penv\Lib\site-packages'
if (-not (Test-Path -LiteralPath $controllerPython)) {
    $controllerPython = (Get-Command python -ErrorAction Stop).Source
}
$controllerMode = if ($Calibration) { 'calibration' } else { 'speed' }
& $controllerPython -c 'import sys, runpy; sys.path.append(sys.argv[1]); runpy.run_path(sys.argv[2], run_name=sys.argv[3])' $serialPackages (Join-Path $PSScriptRoot 'tmc2209_speed_control.py') '__main__' $controllerMode
