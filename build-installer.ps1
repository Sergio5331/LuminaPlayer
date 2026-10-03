param(
    [string]$InnoCompiler = (Join-Path $env:LOCALAPPDATA 'Programs/Inno Setup 6/ISCC.exe'),
    [switch]$SkipBuild,
    [switch]$Sign
)
$ErrorActionPreference = 'Stop'
$luminaRoot = $PSScriptRoot
$luminaSdk = Join-Path $luminaRoot 'third_party/mpv-lgpl'
if (-not (Test-Path -LiteralPath $InnoCompiler)) { throw 'Instala Inno Setup 6.6 o posterior o indica -InnoCompiler.' }
if (-not $SkipBuild) { & (Join-Path $luminaRoot 'build.ps1') -Tests -MpvRoot $luminaSdk }
$luminaRelease = Join-Path $luminaRoot 'build/Release'
if ($Sign) { & (Join-Path $luminaRoot 'sign-local.ps1') }
$luminaStage = Join-Path $luminaRoot ('.tools/installer/stage-' + [Guid]::NewGuid().ToString('N'))
$luminaPayload = Join-Path $luminaStage 'app'
$luminaOutput = Join-Path $luminaRoot 'dist'
New-Item -ItemType Directory -Force -Path $luminaPayload, $luminaOutput | Out-Null
foreach ($luminaFile in @('LuminaPlayer.exe', 'LuminaThumbnail.exe', 'libmpv-2.dll', 'Qt6Core.dll', 'Qt6Gui.dll', 'Qt6Widgets.dll', 'Qt6Network.dll', 'D3Dcompiler_47.dll')) {
    Copy-Item -LiteralPath (Join-Path $luminaRelease $luminaFile) -Destination $luminaPayload
}
if ((Get-FileHash (Join-Path $luminaPayload 'libmpv-2.dll')).Hash -ne (Get-FileHash (Join-Path $luminaSdk 'libmpv-2.dll')).Hash) {
    throw 'La compilación no utiliza la biblioteca LGPL preparada para distribución.'
}
foreach ($luminaPlugin in @('platforms', 'styles', 'imageformats', 'networkinformation', 'tls')) {
    Copy-Item -LiteralPath (Join-Path $luminaRelease $luminaPlugin) -Destination $luminaPayload -Recurse
}
$luminaVswhere = "${env:ProgramFiles(x86)}/Microsoft Visual Studio/Installer/vswhere.exe"
$luminaVs = & $luminaVswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $luminaVs) { throw 'No se encontró Visual Studio con las bibliotecas redistribuibles de C++.' }
$luminaRedist = Get-ChildItem -LiteralPath (Join-Path $luminaVs 'VC/Redist/MSVC') -Directory |
    Where-Object Name -Match '^\d+\.\d+\.\d+$' | Sort-Object { [version]$_.Name } -Descending | Select-Object -First 1
if (-not $luminaRedist) { throw 'No se encontró el redistribuible de MSVC.' }
$luminaCrt = Join-Path $luminaRedist.FullName 'x64/Microsoft.VC143.CRT'
Get-ChildItem -LiteralPath $luminaCrt -Filter '*.dll' | Copy-Item -Destination $luminaPayload
Copy-Item -LiteralPath (Join-Path $luminaRoot 'packaging/AVISOS.txt') -Destination $luminaPayload
Copy-Item -LiteralPath (Join-Path $luminaRoot 'LICENSE') -Destination $luminaPayload
Copy-Item -LiteralPath (Join-Path $luminaRoot 'packaging/licenses') -Destination $luminaPayload -Recurse
$luminaSources = Join-Path $luminaPayload 'third-party-source'
New-Item -ItemType Directory -Path $luminaSources | Out-Null
foreach ($luminaSource in @('qtbase-6.8.3-source.tar.xz', 'mpv-source.zip', 'ffmpeg-source.zip', 'mpv-build-workflow-source.zip', 'lgpl-build-logs.zip')) {
    Copy-Item -LiteralPath (Join-Path $luminaRoot ".tools/installer/$luminaSource") -Destination $luminaSources
}
Copy-Item -LiteralPath (Join-Path $luminaRoot '.tools/installer/mpv-lgpl.json') -Destination $luminaSources
$luminaManifest = Get-ChildItem -LiteralPath $luminaPayload -File -Recurse | ForEach-Object {
    [pscustomobject]@{ File = [IO.Path]::GetRelativePath($luminaPayload, $_.FullName); SHA256 = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLower() }
}
$luminaManifest | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $luminaPayload 'manifest.json') -Encoding UTF8
& $InnoCompiler /Qp "/DPayloadDir=$luminaPayload" "/DOutputPath=$luminaOutput" (Join-Path $luminaRoot 'packaging/LuminaPlayer.iss')
if ($LASTEXITCODE -ne 0) { throw 'No se pudo compilar el instalador.' }
$luminaInstaller = Join-Path $luminaOutput 'LuminaPlayer-0.9.0-Beta-Setup-x64.exe'
if ($Sign) { & (Join-Path $luminaRoot 'sign-local.ps1') -BinaryPaths @($luminaInstaller) }
$luminaInstallerHash = (Get-FileHash -LiteralPath $luminaInstaller -Algorithm SHA256).Hash.ToLower()
"$luminaInstallerHash  $([IO.Path]::GetFileName($luminaInstaller))" | Set-Content -LiteralPath (Join-Path $luminaOutput 'SHA256SUMS.txt') -Encoding ascii
Write-Host "Instalador: $luminaInstaller"
Write-Host "SHA256: $luminaInstallerHash"
