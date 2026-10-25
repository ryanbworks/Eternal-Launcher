#Requires -Version 7.0
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '../tools/CiSigning.ps1')
$original = [Security.Cryptography.RSA]::Create(3072)
try {
    $bytes = $original.ExportPkcs8PrivateKey()
    try { $env:ETERNAL_SIGNING_KEY_PKCS8 = [Convert]::ToBase64String($bytes) }
    finally { [Array]::Clear($bytes, 0, $bytes.Length) }
    $imported = Get-EternalCiSigningKey
    try {
        if ($env:ETERNAL_SIGNING_KEY_PKCS8) { throw 'A chave permaneceu no ambiente dos processos filhos.' }
        if ($imported.ExportSubjectPublicKeyInfoPem() -cne $original.ExportSubjectPublicKeyInfoPem()) {
            throw 'A chave importada não corresponde à original.'
        }
        $message = [Text.Encoding]::UTF8.GetBytes('eternal-ci-test')
        $signature = $imported.SignData($message, [Security.Cryptography.HashAlgorithmName]::SHA256,
            [Security.Cryptography.RSASignaturePadding]::Pss)
        if (!$original.VerifyData($message, $signature, [Security.Cryptography.HashAlgorithmName]::SHA256,
            [Security.Cryptography.RSASignaturePadding]::Pss)) { throw 'Assinatura de CI inválida.' }
    } finally { $imported.Dispose() }
    $bytes = $original.ExportPkcs8PrivateKey()
    try { $env:ETERNAL_SIGNING_KEY_PKCS8 = [Convert]::ToBase64String($bytes) }
    finally { [Array]::Clear($bytes, 0, $bytes.Length) }
    $mismatchRejected = $false
    try { & (Join-Path $PSScriptRoot '../tools/Release.ps1') -SkipBuild -CiSigningKey }
    catch {
        if ($_.Exception.Message -notlike '*não corresponde*') { throw }
        $mismatchRejected = $true
    }
    if (!$mismatchRejected) { throw 'Uma chave diferente da chave pública de produção foi aceita.' }
    foreach ($invalid in @('', 'not-base64', [Convert]::ToBase64String([byte[]]@(1,2,3)))) {
        $env:ETERNAL_SIGNING_KEY_PKCS8 = $invalid
        $rejected = $false
        try { $unexpected = Get-EternalCiSigningKey; $unexpected.Dispose() }
        catch { $rejected = $true }
        if (!$rejected) { throw 'Uma chave ausente ou inválida foi aceita.' }
        if ($env:ETERNAL_SIGNING_KEY_PKCS8) { throw 'O secret inválido permaneceu no ambiente.' }
    }
    Write-Host 'Assinatura de CI validada: importação, assinatura, limpeza de ambiente e rejeição de chave diferente ou inválida.'
} finally {
    $env:ETERNAL_SIGNING_KEY_PKCS8 = $null
    $original.Dispose()
}
