param(
    [Parameter(Mandatory = $true)]
    [ValidatePattern('^COM[0-9]+$')]
    [string]$Port,

    [Parameter(Mandatory = $true)]
    [string]$Image
)

$firmware = Get-Item -LiteralPath $Image -ErrorAction Stop
if ($firmware.Name -notmatch '\.merged\.bin$') {
    throw 'Use the Arduino ESP32-C3 *.merged.bin image, not the app-only *.bin.'
}

$toolRoot = Join-Path $env:LOCALAPPDATA 'Arduino15/packages/esp32/tools/esptool_py'
$tool = Get-ChildItem -LiteralPath $toolRoot -Filter esptool.exe -Recurse -ErrorAction Stop |
        Sort-Object FullName -Descending | Select-Object -First 1
if ($null -eq $tool) { throw 'esptool.exe was not found in the Arduino ESP32 package.' }

Write-Host "Checking ESP32-C3 through STM32 bridge on $Port ..."
& $tool.FullName --chip esp32c3 --port $Port --baud 115200 --before no-reset --after no-reset flash-id
if ($LASTEXITCODE -ne 0) { throw 'Flash-ID probe failed. Check H3, ESP reset, H1 wiring and board power.' }

Write-Host "Writing $($firmware.FullName) at 0x0 ..."
& $tool.FullName --chip esp32c3 --port $Port --baud 115200 --before no-reset --after no-reset write-flash 0x0 $firmware.FullName
if ($LASTEXITCODE -ne 0) { throw 'ESP flashing failed. Leave H3 connected and check the serial path.' }

Write-Host 'ESP image verified. Remove the H3 IO9-GND jumper, then restart the board.'
