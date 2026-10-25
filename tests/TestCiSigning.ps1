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
        if ($env:ETERNAL_SIGNING_KEY_PKCS8) { throw 'The key remained in the environment of child processes.' }
        if ($imported.ExportSubjectPublicKeyInfoPem() -cne $original.ExportSubjectPublicKeyInfoPem()) {
            throw 'The imported key does not match the original.'
        }
        $message = [Text.Encoding]::UTF8.GetBytes('eternal-ci-test')
        $signature = $imported.SignData($message, [Security.Cryptography.HashAlgorithmName]::SHA256,
            [Security.Cryptography.RSASignaturePadding]::Pss)
        if (!$original.VerifyData($message, $signature, [Security.Cryptography.HashAlgorithmName]::SHA256,
            [Security.Cryptography.RSASignaturePadding]::Pss)) { throw 'Invalid CI signature.' }
    } finally { $imported.Dispose() }
    $bytes = $original.ExportPkcs8PrivateKey()
    try { $env:ETERNAL_SIGNING_KEY_PKCS8 = [Convert]::ToBase64String($bytes) }
    finally { [Array]::Clear($bytes, 0, $bytes.Length) }
    $mismatchRejected = $false
    try { & (Join-Path $PSScriptRoot '../tools/Release.ps1') -SkipBuild -CiSigningKey }
    catch {
        if ($_.Exception.Message -notlike '*does not match*') { throw }
        $mismatchRejected = $true
    }
    if (!$mismatchRejected) { throw 'A key different from the production public key was accepted.' }
    foreach ($invalid in @('', 'not-base64', [Convert]::ToBase64String([byte[]]@(1,2,3)))) {
        $env:ETERNAL_SIGNING_KEY_PKCS8 = $invalid
        $rejected = $false
        try { $unexpected = Get-EternalCiSigningKey; $unexpected.Dispose() }
        catch { $rejected = $true }
        if (!$rejected) { throw 'A missing or invalid key was accepted.' }
        if ($env:ETERNAL_SIGNING_KEY_PKCS8) { throw 'The invalid secret remained in the environment.' }
    }
    Write-Host 'CI signing validated: import, signing, environment cleanup and rejection of a different or invalid key.'
} finally {
    $env:ETERNAL_SIGNING_KEY_PKCS8 = $null
    $original.Dispose()
}
