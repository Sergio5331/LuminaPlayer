param([ValidateSet('Release', 'Debug')][string]$Configuration = 'Release', [switch]$Tests)
$ErrorActionPreference = 'Stop'
# Algunos hosts proporcionan Path y PATH a la vez; MSBuild exige una sola clave.
$luminaSearchPath = $env:PATH
[Environment]::SetEnvironmentVariable('PATH', $null, 'Process')
[Environment]::SetEnvironmentVariable('Path', $null, 'Process')
[Environment]::SetEnvironmentVariable('Path', $luminaSearchPath, 'Process')
$luminaRoot = $PSScriptRoot
$luminaVswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path -LiteralPath $luminaVswhere)) { throw 'No se encontró Visual Studio Build Tools.' }
$luminaVs = & $luminaVswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $luminaVs) { throw 'No se encontró una instalación completa de MSVC x64.' }
$env:VCINSTALLDIR = Join-Path $luminaVs 'VC\'
$luminaCmake = Join-Path $luminaVs 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
if (-not (Test-Path -LiteralPath $luminaCmake)) { throw 'Falta el componente CMake de Visual Studio.' }
& $luminaCmake -S $luminaRoot -B "$luminaRoot\build" -G 'Visual Studio 17 2022' -A x64 "-DCMAKE_PREFIX_PATH=$luminaRoot\third_party\Qt" "-DMPV_ROOT=$luminaRoot\third_party\mpv" "-DLUMINA_BUILD_TESTS=$($Tests.IsPresent)"
if ($LASTEXITCODE -ne 0) { throw 'Falló la configuración CMake.' }
& $luminaCmake --build "$luminaRoot\build" --config $Configuration --parallel
if ($LASTEXITCODE -ne 0) { throw 'Falló la compilación.' }
Write-Host "Ejecutable: $luminaRoot\build\$Configuration\LuminaPlayer.exe"
if ($Tests) {
    $luminaCtest = Join-Path (Split-Path $luminaCmake) 'ctest.exe'
    & $luminaCtest --test-dir "$luminaRoot\build" -C $Configuration --output-on-failure
    if ($LASTEXITCODE -ne 0) { throw 'Fallaron las pruebas de integración.' }
}
