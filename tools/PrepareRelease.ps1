#Requires -Version 5.1
[CmdletBinding()]
param(
    [ValidatePattern('^(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)$')][string]$Version,
    [ValidatePattern('^[A-Za-z0-9]+$')][string]$Codename,
    [switch]$Preview,
    [switch]$NoUploadGuide,
    [switch]$CheckSigningKey
)
$ErrorActionPreference = 'Stop'

# The double-click entry point works even when the IDE has no pwsh in PATH.
if ($PSVersionTable.PSVersion.Major -lt 7) {
    $command = Get-Command pwsh.exe -CommandType Application -ErrorAction SilentlyContinue | Select-Object -First 1
    $candidates = @(
        $(if ($command) { $command.Source }),
        "$env:ProgramW6432/PowerShell/7/pwsh.exe",
        "$env:ProgramFiles/PowerShell/7/pwsh.exe",
        "$env:LOCALAPPDATA/Microsoft/PowerShell/7/pwsh.exe",
        "$env:USERPROFILE/.cache/codex-runtimes/codex-primary-runtime/dependencies/native/powershell/pwsh.exe"
    )
    $powerShellPath = $candidates | Where-Object { $_ -and (Test-Path -LiteralPath $_) } | Select-Object -First 1
    if (!$powerShellPath) { throw 'Instale o PowerShell 7 neste computador para gerar releases. Jogadores nao precisam dele.' }
    $forward = @('-NoLogo', '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', $PSCommandPath)
    foreach ($name in @('Version', 'Codename')) {
        if ($PSBoundParameters.ContainsKey($name)) { $forward += @("-$name", $PSBoundParameters[$name]) }
    }
    if ($Preview) { $forward += '-Preview' }
    if ($NoUploadGuide) { $forward += '-NoUploadGuide' }
    if ($CheckSigningKey) { $forward += '-CheckSigningKey' }
    & $powerShellPath @forward
    exit $LASTEXITCODE
}

$projectRoot = Split-Path $PSScriptRoot -Parent
Set-Location -LiteralPath $projectRoot
$cryptoTools = Join-Path $PSScriptRoot 'ReleaseCrypto.ps1'
. $cryptoTools
if ($CheckSigningKey) {
    $keyPath = Get-EternalSigningKeyPath
    $releaseKey = Get-EternalSigningKey
    try {
        $actualPublicKey = Get-EternalPublicBlob $releaseKey
        $publicHeader = [IO.File]::ReadAllText((Join-Path $projectRoot 'src/config/UpdatePublicKey.h'))
        [byte[]]$expectedPublicKey = [regex]::Matches($publicHeader, '0x([0-9a-fA-F]{2})') |
            ForEach-Object { [Convert]::ToByte($_.Groups[1].Value, 16) }
        if ([Convert]::ToBase64String($actualPublicKey) -cne [Convert]::ToBase64String($expectedPublicKey)) {
            throw 'A chave privada encontrada nao corresponde a chave publica da Aurora.'
        }
        Write-Host "Chave validada para $([Security.Principal.WindowsIdentity]::GetCurrent().Name): $keyPath"
    } finally { $releaseKey.Dispose() }
    return
}
$cmakePath = Join-Path $projectRoot 'CMakeLists.txt'
$cmakeText = [IO.File]::ReadAllText($cmakePath)
$versionPattern = 'project\(EternalLauncher VERSION ([0-9]+\.[0-9]+\.[0-9]+)'
$namePattern = 'set\(ETERNAL_CODENAME "([A-Za-z0-9]+)"\)'
$currentVersion = [regex]::Match($cmakeText, $versionPattern).Groups[1].Value
$currentName = [regex]::Match($cmakeText, $namePattern).Groups[1].Value
if (!$currentVersion -or !$currentName) { throw 'Versao ou nome nao encontrado no CMakeLists.txt.' }
Write-Host "Eternal Launcher - preparar atualizacao (atual: $currentVersion-$currentName)"
Write-Host 'O assistente gera o pacote local. O envio sera feito por voce no FileZilla.'
if (!$Version) {
    if ($Preview) { throw 'Informe -Version ao usar -Preview.' }
    $parts = $currentVersion.Split('.')
    $suggested = if ([int]$parts[2] -lt 65535) { "$($parts[0]).$($parts[1]).$([int]$parts[2]+1)" } else { '' }
    $Version = Read-Host "Nova versao numerica [$suggested]"
    if (!$Version) { $Version = $suggested }
}
if ($Version -notmatch '^(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)$' -or
    @($Version.Split('.') | Where-Object { [decimal]$_ -gt 65535 }).Count -gt 0) {
    throw 'Use uma versao como 1.0.1, com cada numero entre 0 e 65535.'
}
if ([version]$Version -lt [version]$currentVersion) { throw 'A versao nova nao pode ser menor que a atual.' }
if (!$Codename) {
    if (!$Preview) { $Codename = Read-Host "Nome da versao [$currentName]" }
    if (!$Codename) { $Codename = $currentName }
}
if ($Codename -cnotmatch '^[A-Za-z0-9]+$') { throw 'Use um nome com letras e numeros, sem espacos ou acentos.' }
$package = Join-Path $projectRoot "out/releases/$Version"
if (Test-Path -LiteralPath $package) {
    throw "Ja existe um pacote $Version em $package. Escolha um numero novo para preservar a release existente."
}
Write-Host "Pacote: $Version-$Codename"
Write-Host "Destino local: $package"
$keyPath = Get-EternalSigningKeyPath
$releaseKey = Get-EternalSigningKey
try {
    $actualPublicKey = Get-EternalPublicBlob $releaseKey
    $publicHeader = [IO.File]::ReadAllText((Join-Path $projectRoot 'src/config/UpdatePublicKey.h'))
    [byte[]]$expectedPublicKey = [regex]::Matches($publicHeader, '0x([0-9a-fA-F]{2})') |
        ForEach-Object { [Convert]::ToByte($_.Groups[1].Value, 16) }
    if ([Convert]::ToBase64String($actualPublicKey) -cne [Convert]::ToBase64String($expectedPublicKey)) {
        throw 'A chave privada encontrada nao corresponde a chave publica da Aurora. A versao nao foi alterada.'
    }
} finally {
    $releaseKey.Dispose()
}
Write-Host "Chave de AutoUpdate validada: $keyPath"
if ($Preview) {
    Write-Host 'PREVIEW: nenhum arquivo alterado, nenhuma compilacao ou publicacao executada.'
    return
}

$cmakeText = [regex]::Replace($cmakeText, $versionPattern, "project(EternalLauncher VERSION $Version")
$cmakeText = [regex]::Replace($cmakeText, $namePattern, "set(ETERNAL_CODENAME `"$Codename`")")
[IO.File]::WriteAllText($cmakePath, $cmakeText)
& (Join-Path $PSScriptRoot 'Release.ps1')
& (Join-Path $PSScriptRoot 'VerifyRelease.ps1') -PackageDirectory $package -ExpectedVersion $Version | Format-List

# The staged manifest cannot announce an update until it is renamed on the server.
$fileZilla = Join-Path $package 'FILEZILLA'
$remoteFolder = Join-Path $fileZilla "launcher/releases/$Version"
[IO.Directory]::CreateDirectory($remoteFolder) | Out-Null
Copy-Item -LiteralPath (Join-Path $package "launcher/releases/$Version/EternalLauncher.exe") -Destination $remoteFolder
Copy-Item -LiteralPath (Join-Path $package 'launcher/latest.json') -Destination (Join-Path $fileZilla 'launcher/latest.json.upload')
$guide = @"
ATUALIZACAO $Version-$Codename

1. No FileZilla, conecte por SFTP e abra /srv/eternal-launcher/files.
2. Envie a pasta launcher que esta dentro de FILEZILLA para esse destino.
   Aguarde a fila terminar sem falhas. O manifesto ainda sera latest.json.upload.
3. Volte ao assistente e pressione Enter. Ele confere o executavel pelo HTTPS.
4. Depois da confirmacao, no servidor renomeie launcher/latest.json.upload para
   launcher/latest.json, substituindo o manifesto antigo.
5. Volte ao assistente e pressione Enter para conferir a publicacao final.

Nao envie a pasta inteira deste pacote: somente FILEZILLA/launcher.
Nao edite o JSON: ele contem a assinatura do executavel desta versao.
Nao apague releases anteriores. A chave privada fica neste computador.

Se fechou o assistente, confira antes de renomear, no PowerShell 7, na raiz do projeto:
  ./tools/VerifyRelease.ps1 -PackageDirectory ./out/releases/$Version -Uploaded
Depois de renomear:
  ./tools/VerifyRelease.ps1 -Public -ExpectedVersion $Version
"@
[IO.File]::WriteAllText((Join-Path $package 'COMO-ENVIAR.txt'), $guide)
Write-Host $guide
if ($NoUploadGuide) { return }
Start-Process -FilePath explorer.exe -ArgumentList ('"{0}"' -f $fileZilla)
[void](Read-Host 'Depois de concluir o envio no FileZilla, pressione Enter para conferir')
& (Join-Path $PSScriptRoot 'VerifyRelease.ps1') -PackageDirectory $package -Uploaded | Format-List
Write-Host 'Executavel conferido. Agora renomeie latest.json.upload para latest.json no servidor.'
[void](Read-Host 'Depois de renomear, pressione Enter para conferir a publicacao')
& (Join-Path $PSScriptRoot 'VerifyRelease.ps1') -Public -ExpectedVersion $Version | Format-List
Write-Host "Publicado e conferido: $Version-$Codename. Os jogadores atualizarao na proxima abertura."
