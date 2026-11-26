<#
.SYNOPSIS
    Compila el proyecto usando el make y el GCC de S32 Design Studio.

.DESCRIPTION
    S32DS es el dueno del proyecto: este script NO modifica .cproject,
    .settings, makefile, subdir.mk ni .args. Solo ejecuta el makefile que
    S32DS genero en la carpeta de configuracion (Debug_FLASH por defecto)
    y, en Clean, borra unicamente productos de compilacion.

    Si se agregan archivos .c nuevos o se cambian opciones del proyecto,
    hay que compilar una vez desde S32DS para que regenere el makefile.

.EXAMPLE
    .\scripts\build.ps1
    .\scripts\build.ps1 -Clean
    .\scripts\build.ps1 -Rebuild -Config Debug_RAM
#>

param(
    [string]$Config = "Debug_FLASH",
    [switch]$Clean,
    [switch]$Rebuild,
    [int]$Jobs = 4,
    [string]$S32dsRoot = "C:\NXP\S32DS.3.6.10",
    # Vacio = se toma del .cproject (el mismo GCC que usa S32DS)
    [string]$GccVersion = ""
)

$ErrorActionPreference = "Stop"

$projectRoot = Split-Path -Parent $PSScriptRoot
$buildDir    = Join-Path $projectRoot $Config
$cproject    = Join-Path $projectRoot ".cproject"
$makefile    = Join-Path $buildDir "makefile"
$buildTools  = Join-Path $S32dsRoot "S32DS\build_tools"

# Usar la misma version de GCC configurada en S32DS
if ([string]::IsNullOrEmpty($GccVersion))
{
    $match = Select-String -Path $cproject -Pattern 'arm32\.bare\.gnu\.(\d+\.\d+)' | Select-Object -First 1
    if ($match) { $GccVersion = $match.Matches[0].Groups[1].Value } else { $GccVersion = "10.2" }
}
$GccVersion = $GccVersion -replace '^gcc_v', ''

$gccBin  = Join-Path $buildTools "gcc_v$GccVersion\gcc-$GccVersion-arm32-eabi\bin"
$makeBin = Join-Path $buildTools "msys32\usr\bin"

# Validar herramientas
foreach ($tool in @((Join-Path $gccBin "arm-none-eabi-gcc.exe"), (Join-Path $makeBin "make.exe")))
{
    if (-not (Test-Path $tool))
    {
        Write-Error "No se encontro: $tool`nRevisa -S32dsRoot / -GccVersion."
    }
}

# Validar makefile generado por S32DS
if (-not (Test-Path $makefile))
{
    Write-Error "No existe $Config\makefile. Compila una vez desde S32DS para generarlo."
}

if (-not $Clean)
{
    # Si S32DS no cargo el SDK, las rutas de include quedan como -I"/header/"
    $badArgs = Get-ChildItem -Path $buildDir -Recurse -File -Filter *.args |
        Select-String -Pattern '^-I"/' -List
    if ($badArgs)
    {
        Write-Error ("Los .args tienen rutas del SDK sin resolver (ej. $($badArgs[0].Filename): $($badArgs[0].Line)).`n" +
                     "S32DS no cargo el PlatformSDK. Revisa Properties > SDKs en S32DS y compila ahi.")
    }

    # Aviso si la configuracion del proyecto cambio despues de generar el makefile
    if ((Get-Item $cproject).LastWriteTime -gt (Get-Item $makefile).LastWriteTime)
    {
        Write-Warning ".cproject es mas nuevo que $Config\makefile. Si cambiaste opciones o agregaste archivos en S32DS, compila una vez alla."
    }
}

$env:PATH = "$gccBin;$makeBin;$env:PATH"

Push-Location $buildDir
try
{
    if ($Clean -or $Rebuild)
    {
        # No se usa "make clean": el makefile de S32DS hace "rm -rf ./*" y
        # borra tambien el makefile y los .args. Solo se borran productos.
        Write-Host "==> clean ($Config)" -ForegroundColor Cyan
        Get-ChildItem -Path . -Recurse -File -Include *.o, *.d, *.elf, *.map, *.siz |
            Remove-Item -Force
    }

    if (-not $Clean)
    {
        Write-Host "==> make all ($Config, -j$Jobs, GCC $GccVersion)" -ForegroundColor Cyan
        & make "-j$Jobs" all
        if ($LASTEXITCODE -ne 0)
        {
            Write-Host "==> Build FALLO" -ForegroundColor Red
            exit $LASTEXITCODE
        }
        Write-Host "==> Build OK" -ForegroundColor Green
    }
}
finally
{
    Pop-Location
}
