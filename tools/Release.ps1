#Requires -Version 7.0
[CmdletBinding()]
param([switch]$InitializeKeyOnly, [switch]$SkipBuild,
    [string]$BuildDirectory = 'out/build/aurora', [switch]$CiSigningKey)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
Set-Location $projectRoot
. (Join-Path $PSScriptRoot 'ReleaseCrypto.ps1')
if ($CiSigningKey) {
    if ($InitializeKeyOnly) { throw 'The CI key must match the existing public key.' }
    . (Join-Path $PSScriptRoot 'CiSigning.ps1')
    $rsa = Get-EternalCiSigningKey
} else {
    $rsa = Get-EternalSigningKey -Initialize:$InitializeKeyOnly
}
try {
    $blob = Get-EternalPublicBlob $rsa
    $headerPath = Join-Path $projectRoot 'src/config/UpdatePublicKey.h'
    $hex = ($blob | ForEach-Object { '0x{0:x2}' -f $_ }) -join ','
    $header = "#pragma once`n#include <array>`nnamespace LauncherUpdateTrust {`ninline constexpr std::array<unsigned char, $($blob.Length)> PublicKey = {$hex};`n}`n"
    $publicHeaderMatches = $false
    if (Test-Path -LiteralPath $headerPath) {
        [byte[]]$storedBlob = [regex]::Matches([IO.File]::ReadAllText($headerPath), '0x([0-9a-fA-F]{2})') |
            ForEach-Object { [Convert]::ToByte($_.Groups[1].Value, 16) }
        $publicHeaderMatches = [Convert]::ToBase64String($storedBlob) -ceq [Convert]::ToBase64String($blob)
    }
    if ($InitializeKeyOnly) {
        if ((Test-Path $headerPath) -and !$publicHeaderMatches) {
            throw 'The existing public key differs from this private key. Replacing it silently is not allowed.'
        }
        [IO.File]::WriteAllText($headerPath, $header)
        Write-Host 'Local key protected by DPAPI; only the public key was written to the project.'
        return
    }
    if (!$publicHeaderMatches) {
        throw 'The private key does not match the compiled public key.'
    }
    $cmakeText = Get-Content CMakeLists.txt -Raw
    $version = [regex]::Match($cmakeText, 'project\(EternalLauncher VERSION ([0-9]+\.[0-9]+\.[0-9]+)').Groups[1].Value
    $codename = [regex]::Match($cmakeText, 'set\(ETERNAL_CODENAME "([A-Za-z0-9]+)"').Groups[1].Value
    if (!$version -or !$codename) { throw 'Invalid version or name in CMakeLists.txt.' }
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    $vs = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if (!$vs) { throw 'Visual Studio C++ tools not found.' }
    Import-Module (Join-Path $vs 'Common7/Tools/Microsoft.VisualStudio.DevShell.dll')
    Enter-VsDevShell -VsInstallPath $vs -SkipAutomaticLocation -DevCmdArguments '-arch=x64 -host_arch=x64' | Out-Null
    $cmake = Join-Path $vs 'Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe'
    $ctest = Join-Path (Split-Path $cmake) 'ctest.exe'
    if (!$SkipBuild) {
        $releasePowerShell = Join-Path $PSHOME 'pwsh.exe'
        & $cmake -S . -B $BuildDirectory -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON `
            -DETERNAL_REQUIRE_UPDATE_TESTS=ON "-DETERNAL_POWERSHELL:FILEPATH=$releasePowerShell"
        if ($LASTEXITCODE) { throw 'CMake configuration failed.' }
        & $cmake --build $BuildDirectory
        if ($LASTEXITCODE) { throw 'Build failed.' }
        & $ctest --test-dir $BuildDirectory --output-on-failure
        if ($LASTEXITCODE) { throw 'Tests failed; release not generated.' }
    }
    $executable = Join-Path $BuildDirectory 'EternalLauncher.exe'
    $info = [Diagnostics.FileVersionInfo]::GetVersionInfo((Resolve-Path $executable))
    if ($info.ProductVersion -ne "$version-$codename") { throw 'The executable version does not match the code.' }
    $pe = [IO.File]::ReadAllBytes((Resolve-Path $executable))
    $peOffset = [BitConverter]::ToInt32($pe, 0x3c)
    if ([BitConverter]::ToUInt16($pe, $peOffset+4) -ne 0x8664) { throw 'The distribution must be x64.' }
    $package = Join-Path $projectRoot "out/releases/$version"
    $artifactFolder = Join-Path $package "launcher/releases/$version"
    [IO.Directory]::CreateDirectory($artifactFolder) | Out-Null
    $artifact = Join-Path $artifactFolder 'EternalLauncher.exe'
    Copy-Item -LiteralPath $executable -Destination $artifact -Force
    $payload = [ordered]@{ version=$version; codename=$codename; architecture='x64';
        path="/launcher/releases/$version/EternalLauncher.exe"; size=(Get-Item $artifact).Length;
        sha256=(Get-FileHash $artifact -Algorithm SHA256).Hash.ToLowerInvariant() }
    $manifest = New-EternalEnvelope $rsa $payload
    [IO.File]::WriteAllText((Join-Path $package 'launcher/latest.json'), $manifest + "`n")
    [IO.File]::WriteAllText((Join-Path $package 'SHA256SUMS.txt'), "$($payload.sha256)  launcher/releases/$version/EternalLauncher.exe`n")
    Write-Host "Package for SFTP/FileZilla: $package"
} finally { $rsa.Dispose() }
