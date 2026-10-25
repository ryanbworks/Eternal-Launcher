# Import the existing release key into memory; never create or replace a trust key.
function Get-EternalCiSigningKey {
    $encoded = [Environment]::GetEnvironmentVariable('ETERNAL_SIGNING_KEY_PKCS8')
    [Environment]::SetEnvironmentVariable('ETERNAL_SIGNING_KEY_PKCS8', $null)
    if ([string]::IsNullOrWhiteSpace($encoded)) { throw 'Secret ETERNAL_SIGNING_KEY_PKCS8 ausente.' }
    $rsa = [Security.Cryptography.RSA]::Create()
    $private = $null
    try {
        $private = [Convert]::FromBase64String($encoded)
        $read = 0
        $rsa.ImportPkcs8PrivateKey($private, [ref]$read)
        if ($read -ne $private.Length -or $rsa.KeySize -lt 3072) { throw 'Chave de assinatura inválida.' }
        return $rsa
    } catch {
        $rsa.Dispose()
        throw 'O secret de assinatura deve conter a chave PKCS#8 original em Base64 (RSA de pelo menos 3072 bits).'
    } finally {
        $encoded = $null
        if ($private) { [Array]::Clear($private, 0, $private.Length) }
    }
}
