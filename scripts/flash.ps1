<#
.SYNOPSIS
    Graba el .elf en la tarjeta usando el P&E GDB Server que trae S32DS.

.DESCRIPTION
    Usa los mismos parametros que la configuracion de debug de S32DS
    (Practica5_FlexCAN_Debug_FLASH_PNE): PEMicro USB Multilink /
    Embedded OSBDM, puerto USB1, dispositivo NXP_S32K3xx_S32K312, SWD a 5000 KHz.

    No modifica ningun archivo del proyecto. Solo lee el .elf.
    Cierra cualquier sesion de debug en S32DS antes de grabar: la sonda
    solo puede usarla un programa a la vez.

.EXAMPLE
    .\scripts\flash.ps1
    .\scripts\flash.ps1 -Build
    .\scripts\flash.ps1 -Port PEM6BD1A7
#>

param(
    [string]$Config = "Debug_FLASH",
    [switch]$Build,
    [string]$Device = "NXP_S32K3xx_S32K312",
    [string]$Interface = "USBMULTILINK",
    [string]$Port = "USB1",
    [int]$Speed = 5000,
    [string]$S32dsRoot = "C:\NXP\S32DS.3.6.10"
)

$ErrorActionPreference = "Stop"

$projectRoot = Split-Path -Parent $PSScriptRoot
$buildDir    = Join-Path $projectRoot $Config

# Un .elf de configuracion RAM no se puede grabar en flash
if ($Config -notmatch 'FLASH')
{
    Write-Error "La configuracion $Config no es de FLASH. Usa Debug_FLASH o Release_FLASH."
}

if ($Build)
{
    & (Join-Path $PSScriptRoot "build.ps1") -Config $Config -S32dsRoot $S32dsRoot
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

# Buscar el .elf generado por S32DS
$elf = Get-ChildItem -Path $buildDir -Filter *.elf -File -ErrorAction SilentlyContinue | Select-Object -First 1
if (-not $elf)
{
    Write-Error "No hay .elf en $Config. Compila primero (build.ps1 o S32DS)."
}

# Buscar el P&E GDB Server (la carpeta del plugin cambia con cada version)
$server = Get-ChildItem -Path (Join-Path $S32dsRoot "eclipse\plugins") -Directory -Filter "com.pemicro.debug.gdbjtag.pne_*" |
    Sort-Object Name -Descending |
    ForEach-Object { Join-Path $_.FullName "win32\pegdbserver_console.exe" } |
    Where-Object { Test-Path $_ } |
    Select-Object -First 1
if (-not $server)
{
    Write-Error "No se encontro pegdbserver_console.exe en $S32dsRoot\eclipse\plugins. Revisa -S32dsRoot."
}

Write-Host "==> Grabando $($elf.Name) ($($elf.LastWriteTime))" -ForegroundColor Cyan
Write-Host "    Sonda: $Interface $Port | Dispositivo: $Device | $Speed KHz" -ForegroundColor Cyan

# programmingtype=0 -> erase/program/verify
$serverArgs = @(
    "-startserver",
    "-singlesession",
    "-device=$Device",
    "-interface=$Interface",
    "-port=$Port",
    "-speed=$Speed",
    "-flashobjectfile=$($elf.FullName)",
    "-programmingtype=0",
    "-quitafterprogramming",
    "-runafterprogramming"
)

& $server @serverArgs
if ($LASTEXITCODE -ne 0)
{
    Write-Host "==> Grabado FALLO (codigo $LASTEXITCODE)" -ForegroundColor Red
    exit $LASTEXITCODE
}
Write-Host "==> Grabado OK" -ForegroundColor Green
