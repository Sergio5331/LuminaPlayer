param([string]$SevenZip)
$ErrorActionPreference = 'Stop'
$luminaRoot = $PSScriptRoot
$luminaCache = Join-Path $luminaRoot '.tools/installer'
New-Item -ItemType Directory -Path $luminaCache -Force | Out-Null
$luminaLock = Get-Content -LiteralPath (Join-Path $luminaRoot 'packaging/dependencies.json') -Raw | ConvertFrom-Json
$luminaBundle = Join-Path $luminaCache 'LuminaPlayer-Dependencies.zip'
if (-not (Test-Path -LiteralPath $luminaBundle)) {
    Invoke-WebRequest -Uri $luminaLock.url -OutFile $luminaBundle
}
if ((Get-FileHash -LiteralPath $luminaBundle -Algorithm SHA256).Hash.ToLower() -ne $luminaLock.sha256) {
    throw 'El paquete de dependencias no coincide con el SHA256 esperado. No se utilizará.'
}
Expand-Archive -LiteralPath $luminaBundle -DestinationPath $luminaCache -Force
if (-not $SevenZip) {
    $luminaSevenZipCandidates = @((Join-Path $luminaRoot '.tools/7zr.exe'), "$env:ProgramFiles/7-Zip/7z.exe")
    $luminaSevenZipCommand = Get-Command 7z.exe -ErrorAction SilentlyContinue
    if ($luminaSevenZipCommand) { $luminaSevenZipCandidates += $luminaSevenZipCommand.Source }
    $SevenZip = $luminaSevenZipCandidates | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
}
if (-not $SevenZip -or -not (Test-Path -LiteralPath $SevenZip)) { throw 'Instala 7-Zip o indica -SevenZip con la ruta de 7z.exe.' }
$luminaSdk = Join-Path $luminaRoot 'third_party/mpv-lgpl'
New-Item -ItemType Directory -Path $luminaSdk -Force | Out-Null
& $SevenZip x (Join-Path $luminaCache 'mpv-lgpl.7z') "-o$luminaSdk" -y
if ($LASTEXITCODE -ne 0) { throw 'No se pudo extraer el SDK de libmpv.' }
$luminaVswhere = "${env:ProgramFiles(x86)}/Microsoft Visual Studio/Installer/vswhere.exe"
$luminaVs = & $luminaVswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $luminaVs) { throw 'No se encontró Visual Studio Build Tools con C++.' }
$luminaToolset = Get-ChildItem -LiteralPath (Join-Path $luminaVs 'VC/Tools/MSVC') -Directory |
    Where-Object Name -Match '^\d+\.\d+\.\d+$' | Sort-Object { [version]$_.Name } -Descending | Select-Object -First 1
if (-not $luminaToolset) { throw 'No se encontró el toolset de MSVC.' }
$luminaToolBin = Join-Path $luminaToolset.FullName 'bin/Hostx64/x64'
$luminaExportOutput = & (Join-Path $luminaToolBin 'dumpbin.exe') /exports (Join-Path $luminaSdk 'libmpv-2.dll')
if ($LASTEXITCODE -ne 0) { throw 'No se pudo leer la API exportada por libmpv.' }
$luminaExports = @($luminaExportOutput | ForEach-Object {
    if ($_ -match '^\s+\d+\s+[0-9A-Fa-f]+\s+[0-9A-Fa-f]+\s+(mpv_[A-Za-z0-9_]+)') { $Matches[1] }
})
if ($luminaExports.Count -lt 30) { throw 'La DLL no contiene la API esperada de libmpv.' }
$luminaLibDir = Join-Path $luminaSdk 'lib'
New-Item -ItemType Directory -Path $luminaLibDir -Force | Out-Null
$luminaDef = Join-Path $luminaLibDir 'mpv.def'
@('LIBRARY libmpv-2.dll', 'EXPORTS') + $luminaExports | Set-Content -LiteralPath $luminaDef -Encoding ascii
& (Join-Path $luminaToolBin 'lib.exe') /machine:x64 "/def:$luminaDef" "/out:$(Join-Path $luminaLibDir 'mpv.lib')"
if ($LASTEXITCODE -ne 0) { throw 'No se pudo crear la biblioteca de importación MSVC.' }
Write-Host 'Dependencias verificadas y SDK de libmpv preparado.'
