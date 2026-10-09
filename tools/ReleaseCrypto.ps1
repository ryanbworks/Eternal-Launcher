# Requires PowerShell 7. The private key never enters the checkout or web root.
function Get-EternalSigningKeyPath {
    $localAppData = [Environment]::GetFolderPath([Environment+SpecialFolder]::LocalApplicationData)
    if ([string]::IsNullOrWhiteSpace($localAppData)) { $localAppData = $env:LOCALAPPDATA }
    if ([string]::IsNullOrWhiteSpace($localAppData)) {
        throw 'O Windows nao informou a pasta LocalAppData da conta atual.'
    }
    $primary = Join-Path $localAppData 'EternalLauncher/Signing/release-key.dpapi'
    $candidates = [System.Collections.Generic.List[string]]::new()
    [void]$candidates.Add($primary)
    # The Codex desktop sandbox can redirect LocalAppData into its package
    # cache. Reuse the same per-user key when that is the context launching
    # the release helper, instead of creating a second trust root.
    $packages = Join-Path $localAppData 'Packages'
    if ([IO.Directory]::Exists($packages)) {
        Get-ChildItem -LiteralPath $packages -Directory -Filter 'OpenAI.Codex_*' -ErrorAction SilentlyContinue |
            ForEach-Object {
                [void]$candidates.Add((Join-Path $_.FullName 'LocalCache/Local/EternalLauncher/Signing/release-key.dpapi'))
            }
    }
    $existing = $candidates | Where-Object { [IO.File]::Exists($_) } | Select-Object -First 1
    if ($existing) { return $existing }
    return $primary
}

function Get-EternalSigningKey {
    param([switch]$Initialize)
    $keyFile = Get-EternalSigningKeyPath
    $keyFolder = Split-Path $keyFile -Parent
    $identity = [Security.Principal.WindowsIdentity]::GetCurrent().Name
    $rsa = [System.Security.Cryptography.RSA]::Create(3072)
    $encrypted = $null
    $missing = $false
    try {
        # Read the file directly so a transient PowerShell provider/path result
        # cannot be mistaken for a missing key when the file is actually present.
        $encrypted = [IO.File]::ReadAllBytes($keyFile)
    } catch [IO.FileNotFoundException] {
        $missing = $true
    } catch [IO.DirectoryNotFoundException] {
        $missing = $true
    } catch {
        $rsa.Dispose()
        throw "A chave foi localizada no caminho '$keyFile', mas nao pode ser lida pela conta '$identity'. Detalhe: $($_.Exception.Message)"
    }
    if (!$missing) {
        try {
            $private = [Security.Cryptography.ProtectedData]::Unprotect($encrypted, $null, 'CurrentUser')
            try { $read = 0; $rsa.ImportPkcs8PrivateKey($private, [ref]$read) }
            finally { if ($private) { [Array]::Clear($private, 0, $private.Length) } }
        } catch {
            $rsa.Dispose()
            throw "A chave existe, mas a conta '$identity' nao conseguiu abri-la com o DPAPI. Caminho: $keyFile. Use a mesma conta Windows que publicou a Aurora. Detalhe: $($_.Exception.Message)"
        }
    } elseif ($Initialize) {
        [IO.Directory]::CreateDirectory($keyFolder) | Out-Null
        $private = $rsa.ExportPkcs8PrivateKey()
        try {
            $encrypted = [Security.Cryptography.ProtectedData]::Protect($private, $null, 'CurrentUser')
            [IO.File]::WriteAllBytes($keyFile, $encrypted)
        } finally { [Array]::Clear($private, 0, $private.Length) }
    } else {
        $rsa.Dispose()
        $directory = if ([IO.Directory]::Exists($keyFolder)) {
            (Get-ChildItem -LiteralPath $keyFolder -Force -ErrorAction SilentlyContinue | ForEach-Object Name) -join ', '
        } else { 'pasta Signing ausente' }
        throw "Chave de release realmente ausente para a conta '$identity'. Caminho procurado: $keyFile. Conteudo da pasta: $directory. Use a mesma conta Windows que publicou a Aurora; nao gere outra chave."
    }
    return $rsa
}

function Get-EternalPublicBlob {
    param([Security.Cryptography.RSA]$Rsa)
    $p = $Rsa.ExportParameters($false)
    $stream = [IO.MemoryStream]::new()
    $writer = [IO.BinaryWriter]::new($stream)
    foreach ($n in @(0x31415352, $Rsa.KeySize, $p.Exponent.Length, $p.Modulus.Length, 0, 0)) {
        $writer.Write([uint32]$n)
    }
    $writer.Write($p.Exponent); $writer.Write($p.Modulus)
    $blob = $stream.ToArray()
    $writer.Dispose(); $stream.Dispose()
    return ,$blob
}

function New-EternalEnvelope {
    param([Security.Cryptography.RSA]$Rsa, [System.Collections.IDictionary]$Payload)
    $bytes = [Text.Encoding]::UTF8.GetBytes(($Payload | ConvertTo-Json -Depth 6 -Compress))
    $signature = $Rsa.SignData($bytes, [Security.Cryptography.HashAlgorithmName]::SHA256,
        [Security.Cryptography.RSASignaturePadding]::Pss)
    return ([ordered]@{schemaVersion=1; payload=[Convert]::ToBase64String($bytes);
        signature=[Convert]::ToBase64String($signature)} | ConvertTo-Json -Compress)
}
