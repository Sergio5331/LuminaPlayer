param([string[]]$BinaryPaths = @((Join-Path $PSScriptRoot 'build/Release/LuminaPlayer.exe'), (Join-Path $PSScriptRoot 'build/Release/LuminaThumbnail.exe')))
# Firma local de desarrollo. No instala confianza ni cambia Smart App Control.
$ErrorActionPreference = 'Stop'
$luminaSigningDir = Join-Path $PSScriptRoot '.tools\signing'
$luminaPublicDir = Join-Path $PSScriptRoot 'certificates'
New-Item -ItemType Directory -Force -Path $luminaSigningDir, $luminaPublicDir | Out-Null
$luminaPfxPath = Join-Path $luminaSigningDir 'LuminaPlayer-Development.pfx'
$luminaPasswordPath = Join-Path $luminaSigningDir 'password.dpapi'

# La clave queda fuera del paquete de fuentes, con acceso limitado al usuario y SYSTEM.
$luminaIdentity = [Security.Principal.WindowsIdentity]::GetCurrent().User
$luminaAcl = [Security.AccessControl.DirectorySecurity]::new()
$luminaAcl.SetOwner($luminaIdentity)
$luminaAcl.SetAccessRuleProtection($true, $false)
foreach ($luminaSid in @($luminaIdentity, [Security.Principal.SecurityIdentifier]::new('S-1-5-18'))) {
    $luminaRule = [Security.AccessControl.FileSystemAccessRule]::new($luminaSid, 'FullControl', 'ContainerInherit,ObjectInherit', 'None', 'Allow')
    $luminaAcl.AddAccessRule($luminaRule)
}
if (-not (Test-Path -LiteralPath $luminaPfxPath)) { Set-Acl -LiteralPath $luminaSigningDir -AclObject $luminaAcl }
if ((Test-Path $luminaPfxPath) -ne (Test-Path $luminaPasswordPath)) { throw 'El certificado y su contraseña deben conservarse juntos.' }
if (-not (Test-Path $luminaPfxPath)) {
    $luminaPassword = [Convert]::ToBase64String([Security.Cryptography.RandomNumberGenerator]::GetBytes(32))
    $luminaRsa = [Security.Cryptography.RSA]::Create(3072)
    try {
        $luminaRequest = [Security.Cryptography.X509Certificates.CertificateRequest]::new(
            'CN=LuminaPlayer Development', $luminaRsa,
            [Security.Cryptography.HashAlgorithmName]::SHA256, [Security.Cryptography.RSASignaturePadding]::Pkcs1)
        $luminaRequest.CertificateExtensions.Add([Security.Cryptography.X509Certificates.X509BasicConstraintsExtension]::new($false,$false,0,$true))
        $luminaRequest.CertificateExtensions.Add([Security.Cryptography.X509Certificates.X509KeyUsageExtension]::new([Security.Cryptography.X509Certificates.X509KeyUsageFlags]::DigitalSignature,$true))
        $luminaOids = [Security.Cryptography.OidCollection]::new()
        $luminaOids.Add([Security.Cryptography.Oid]::new('1.3.6.1.5.5.7.3.3','Code Signing')) | Out-Null
        $luminaRequest.CertificateExtensions.Add([Security.Cryptography.X509Certificates.X509EnhancedKeyUsageExtension]::new($luminaOids,$true))
        $luminaCertificate = $luminaRequest.CreateSelfSigned([DateTimeOffset]::Now.AddMinutes(-5), [DateTimeOffset]::Now.AddYears(2))
        try {
            [IO.File]::WriteAllBytes($luminaPfxPath, $luminaCertificate.Export([Security.Cryptography.X509Certificates.X509ContentType]::Pfx,$luminaPassword))
            [IO.File]::WriteAllBytes((Join-Path $luminaPublicDir 'LuminaPlayer-Development.cer'), $luminaCertificate.Export([Security.Cryptography.X509Certificates.X509ContentType]::Cert))
            $luminaPassword | ConvertTo-SecureString -AsPlainText -Force | ConvertFrom-SecureString | Set-Content -LiteralPath $luminaPasswordPath
        } finally { $luminaCertificate.Dispose() }
    } finally { $luminaRsa.Dispose() }
} else {
    $luminaSecure = (Get-Content -LiteralPath $luminaPasswordPath -Raw).Trim() | ConvertTo-SecureString
    $luminaPassword = [Net.NetworkCredential]::new('', $luminaSecure).Password
}
$luminaSignTool = Get-ChildItem "${env:ProgramFiles(x86)}\Windows Kits\10\bin" -Filter signtool.exe -Recurse |
    Where-Object FullName -Match '\\x64\\' | Sort-Object FullName | Select-Object -Last 1 -ExpandProperty FullName
if (-not $luminaSignTool) { throw 'No se encontró SignTool del Windows SDK.' }
try {
    foreach ($luminaBinary in $BinaryPaths) {
        $luminaName = Split-Path $luminaBinary -Leaf
        & $luminaSignTool sign /fd SHA256 /f $luminaPfxPath /p $luminaPassword /d 'LuminaPlayer (desarrollo local)' $luminaBinary
        if ($LASTEXITCODE -ne 0) { throw "No se pudo firmar $luminaName." }
        $luminaSignature = Get-AuthenticodeSignature -LiteralPath $luminaBinary
        $luminaPublic = [Security.Cryptography.X509Certificates.X509Certificate2]::new((Join-Path $luminaPublicDir 'LuminaPlayer-Development.cer'))
        try {
            if (-not $luminaSignature.SignerCertificate -or $luminaSignature.SignerCertificate.Thumbprint -ne $luminaPublic.Thumbprint) { throw 'El firmante no coincide con el certificado creado.' }
            if ($luminaSignature.Status -eq 'HashMismatch') { throw 'La comprobación de integridad de la firma falló.' }
            [PSCustomObject]@{Archivo=$luminaName; Firmante=$luminaSignature.SignerCertificate.Subject; Huella=$luminaSignature.SignerCertificate.Thumbprint; EstadoConfianza=$luminaSignature.Status.ToString(); Caduca=$luminaSignature.SignerCertificate.NotAfter}
        } finally { $luminaPublic.Dispose() }
    }
} finally { $luminaPassword = $null; $luminaSecure = $null }


