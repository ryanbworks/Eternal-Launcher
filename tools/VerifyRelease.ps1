#Requires -Version 7.0
[CmdletBinding()]
param([string]$PackageDirectory, [switch]$Public, [switch]$Uploaded, [string]$ExpectedVersion)
$ErrorActionPreference = 'Stop'
if ((!$PackageDirectory -and !$Public) -or ($PackageDirectory -and $Public)) {
    throw 'Use -PackageDirectory <folder> or -Public.'
}
if ($Uploaded -and !$PackageDirectory) { throw '-Uploaded requires -PackageDirectory to verify the upload before announcing the release.' }
$projectRoot = Split-Path $PSScriptRoot -Parent
$header = [IO.File]::ReadAllText((Join-Path $projectRoot 'src/config/UpdatePublicKey.h'))
[byte[]]$blob = [regex]::Matches($header, '0x([0-9a-fA-F]{2})') | ForEach-Object { [Convert]::ToByte($_.Groups[1].Value,16) }
$exponentSize = [BitConverter]::ToUInt32($blob,8)
$modulusSize = [BitConverter]::ToUInt32($blob,12)
if ([BitConverter]::ToUInt32($blob,0) -ne 0x31415352 -or $blob.Length -ne 24+$exponentSize+$modulusSize) {throw 'Invalid public key.'}
$parameters = [Security.Cryptography.RSAParameters]::new()
$parameters.Exponent = $blob[24..(23+$exponentSize)]
$parameters.Modulus = $blob[(24+$exponentSize)..($blob.Length-1)]
$rsa = [Security.Cryptography.RSA]::Create()
$rsa.ImportParameters($parameters)
$http = [Net.Http.HttpClient]::new()
$http.Timeout = [TimeSpan]::FromSeconds(30)
$http.MaxResponseContentBufferSize = 65536
$http.DefaultRequestHeaders.CacheControl = [Net.Http.Headers.CacheControlHeaderValue]::new()
$http.DefaultRequestHeaders.CacheControl.NoCache = $true
try {
    if ($Public) {
        $response = $http.GetAsync('https://eternalfast.aplot.cloud/launcher/latest.json').GetAwaiter().GetResult()
        $response.EnsureSuccessStatusCode() | Out-Null
        if (!$response.Headers.CacheControl.NoStore) {throw 'latest.json requires Cache-Control: no-store.'}
        $envelopeText = $response.Content.ReadAsStringAsync().GetAwaiter().GetResult()
    } else {
        $file = Join-Path $PackageDirectory 'launcher/latest.json'
        if ((Get-Item $file).Length -gt 65536) {throw 'Manifest exceeds 64 KiB.'}
        $envelopeText = [IO.File]::ReadAllText($file)
    }
    $envelope = $envelopeText | ConvertFrom-Json
    if ($envelope.schemaVersion -ne 1) {throw 'Incompatible schema.'}
    $payloadBytes = [Convert]::FromBase64String($envelope.payload)
    if (!$rsa.VerifyData($payloadBytes, [Convert]::FromBase64String($envelope.signature),
        [Security.Cryptography.HashAlgorithmName]::SHA256, [Security.Cryptography.RSASignaturePadding]::Pss)) {throw 'Invalid signature.'}
    $payload = [Text.Encoding]::UTF8.GetString($payloadBytes) | ConvertFrom-Json
    if ($ExpectedVersion -and $payload.version -cne $ExpectedVersion) {throw "The manifest announces $($payload.version), but version $ExpectedVersion was expected."}
    if ($payload.version -notmatch '^(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)$' -or
        $payload.path -cne "/launcher/releases/$($payload.version)/EternalLauncher.exe" -or
        $payload.architecture -ne 'x64' -or $payload.size -lt 1 -or $payload.size -gt 268435456) {throw 'Invalid payload.'}
    if ($Public -or $Uploaded) {
        $stream = $http.GetStreamAsync('https://eternalfast.aplot.cloud' + $payload.path).GetAwaiter().GetResult()
        $digest = [Security.Cryptography.IncrementalHash]::CreateHash([Security.Cryptography.HashAlgorithmName]::SHA256)
        try {
            $buffer = [byte[]]::new(65536); [long]$size=0
            while (($count=$stream.Read($buffer,0,$buffer.Length)) -gt 0) {
                $size += $count
                if ($size -gt $payload.size) {throw 'Download is larger than the manifest states.'}
                $digest.AppendData($buffer,0,$count)
            }
            $hash = [Convert]::ToHexString($digest.GetHashAndReset()).ToLowerInvariant()
        } finally { $stream.Dispose(); $digest.Dispose() }
    } else {
        $file = Join-Path $PackageDirectory $payload.path.TrimStart('/')
        $size=(Get-Item $file).Length
        $hash=(Get-FileHash $file -Algorithm SHA256).Hash.ToLowerInvariant()
    }
    if ($size -ne $payload.size -or $hash -cne $payload.sha256) {throw 'Size or SHA-256 does not match.'}
    [pscustomobject]@{Version=$payload.version;Name=$payload.codename;Architecture=$payload.architecture;
        Bytes=$size;SHA256=$hash;Signature='Valid';Public=[bool]$Public;Uploaded=[bool]$Uploaded;
        Download='https://eternalfast.aplot.cloud'+$payload.path}
} finally { $rsa.Dispose(); $http.Dispose() }
