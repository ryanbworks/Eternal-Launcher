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
    if (!$powerShellPath) { throw 'Install PowerShell 7 on this computer to build releases. Players do not need it.' }
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
            throw 'The private key found does not match the Aurora public key.'
        }
        Write-Host "Key validated for $([Security.Principal.WindowsIdentity]::GetCurrent().Name): $keyPath"
    } finally { $releaseKey.Dispose() }
    return
}
$cmakePath = Join-Path $projectRoot 'CMakeLists.txt'
$cmakeText = [IO.File]::ReadAllText($cmakePath)
$versionPattern = 'project\(EternalLauncher VERSION ([0-9]+\.[0-9]+\.[0-9]+)'
$namePattern = 'set\(ETERNAL_CODENAME "([A-Za-z0-9]+)"\)'
$currentVersion = [regex]::Match($cmakeText, $versionPattern).Groups[1].Value
$currentName = [regex]::Match($cmakeText, $namePattern).Groups[1].Value
if (!$currentVersion -or !$currentName) { throw 'Version or name not found in CMakeLists.txt.' }
Write-Host "Eternal Launcher - prepare update (current: $currentVersion-$currentName)"
Write-Host 'The wizard builds the local package. You upload it yourself with FileZilla.'
if (!$Version) {
    if ($Preview) { throw 'Pass -Version when using -Preview.' }
    $parts = $currentVersion.Split('.')
    $suggested = if ([int]$parts[2] -lt 65535) { "$($parts[0]).$($parts[1]).$([int]$parts[2]+1)" } else { '' }
    $Version = Read-Host "New numeric version [$suggested]"
    if (!$Version) { $Version = $suggested }
}
if ($Version -notmatch '^(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)$' -or
    @($Version.Split('.') | Where-Object { [decimal]$_ -gt 65535 }).Count -gt 0) {
    throw 'Use a version such as 1.0.1, with each number between 0 and 65535.'
}
if ([version]$Version -lt [version]$currentVersion) { throw 'The new version cannot be lower than the current one.' }
if (!$Codename) {
    if (!$Preview) { $Codename = Read-Host "Version name [$currentName]" }
    if (!$Codename) { $Codename = $currentName }
}
if ($Codename -cnotmatch '^[A-Za-z0-9]+$') { throw 'Use a name with letters and digits only, without spaces or accents.' }
$package = Join-Path $projectRoot "out/releases/$Version"
if (Test-Path -LiteralPath $package) {
    throw "A $Version package already exists at $package. Choose a new number to preserve the existing release."
}
Write-Host "Package: $Version-$Codename"
Write-Host "Local destination: $package"
$keyPath = Get-EternalSigningKeyPath
$releaseKey = Get-EternalSigningKey
try {
    $actualPublicKey = Get-EternalPublicBlob $releaseKey
    $publicHeader = [IO.File]::ReadAllText((Join-Path $projectRoot 'src/config/UpdatePublicKey.h'))
    [byte[]]$expectedPublicKey = [regex]::Matches($publicHeader, '0x([0-9a-fA-F]{2})') |
        ForEach-Object { [Convert]::ToByte($_.Groups[1].Value, 16) }
    if ([Convert]::ToBase64String($actualPublicKey) -cne [Convert]::ToBase64String($expectedPublicKey)) {
        throw 'The private key found does not match the Aurora public key. The version was not changed.'
    }
} finally {
    $releaseKey.Dispose()
}
Write-Host "AutoUpdate key validated: $keyPath"
if ($Preview) {
    Write-Host 'PREVIEW: no files changed, nothing built or published.'
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
UPDATE $Version-$Codename

1. In FileZilla, connect over SFTP and open /srv/eternal-launcher/files.
2. Upload the launcher folder inside FILEZILLA to that destination.
   Wait for the queue to finish without errors. The manifest is still latest.json.upload.
3. Go back to the wizard and press Enter. It verifies the executable over HTTPS.
4. After the confirmation, rename launcher/latest.json.upload on the server to
   launcher/latest.json, replacing the old manifest.
5. Go back to the wizard and press Enter to verify the final publication.

Do not upload this whole package folder: only FILEZILLA/launcher.
Do not edit the JSON: it holds this version's executable signature.
Do not delete previous releases. The private key stays on this computer.

If you closed the wizard, verify before renaming, in PowerShell 7 at the project root:
  ./tools/VerifyRelease.ps1 -PackageDirectory ./out/releases/$Version -Uploaded
After renaming:
  ./tools/VerifyRelease.ps1 -Public -ExpectedVersion $Version
"@
[IO.File]::WriteAllText((Join-Path $package 'HOW-TO-UPLOAD.txt'), $guide)
Write-Host $guide
if ($NoUploadGuide) { return }
Start-Process -FilePath explorer.exe -ArgumentList ('"{0}"' -f $fileZilla)
[void](Read-Host 'After finishing the upload in FileZilla, press Enter to verify')
& (Join-Path $PSScriptRoot 'VerifyRelease.ps1') -PackageDirectory $package -Uploaded | Format-List
Write-Host 'Executable verified. Now rename latest.json.upload to latest.json on the server.'
[void](Read-Host 'After renaming, press Enter to verify the publication')
& (Join-Path $PSScriptRoot 'VerifyRelease.ps1') -Public -ExpectedVersion $Version | Format-List
Write-Host "Published and verified: $Version-$Codename. Players will update on their next launch."
