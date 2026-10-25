# Import the existing release key into memory; never create or replace a trust key.
function Get-EternalCiSigningKey {
    $encoded = [Environment]::GetEnvironmentVariable('ETERNAL_SIGNING_KEY_PKCS8')
    [Environment]::SetEnvironmentVariable('ETERNAL_SIGNING_KEY_PKCS8', $null)
    if ([string]::IsNullOrWhiteSpace($encoded)) { throw 'Secret ETERNAL_SIGNING_KEY_PKCS8 is missing.' }
    $rsa = [Security.Cryptography.RSA]::Create()
    $private = $null
    try {
        $private = [Convert]::FromBase64String($encoded)
        $read = 0
        $rsa.ImportPkcs8PrivateKey($private, [ref]$read)
        if ($read -ne $private.Length -or $rsa.KeySize -lt 3072) { throw 'Invalid signing key.' }
        return $rsa
    } catch {
        $rsa.Dispose()
        throw 'The signing secret must contain the original PKCS#8 key in Base64 (RSA, at least 3072 bits).'
    } finally {
        $encoded = $null
        if ($private) { [Array]::Clear($private, 0, $private.Length) }
    }
}
