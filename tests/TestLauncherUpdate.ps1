#Requires -Version 7.0
param([Parameter(Mandatory)][string]$OldExecutable,
    [Parameter(Mandatory)][string]$NewExecutable,
    [Parameter(Mandatory)][string]$NewVersion,
    [Parameter(Mandatory)][string]$WorkingDirectory)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '../tools/ReleaseCrypto.ps1')
$testKey = [Security.Cryptography.RSA]::Create()
$testKey.ImportFromPem([IO.File]::ReadAllText((Join-Path $PSScriptRoot 'update-trust/TEST-ONLY-private.pem')))
$testRoot = Join-Path ([IO.Path]::GetFullPath($WorkingDirectory)) ('update-tests-' + [guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($testRoot) | Out-Null
$script:checks = 0
function Assert-True([bool]$Condition, [string]$Message) {
    if (!$Condition) { throw "FAIL: $Message ($testRoot)" }
    $script:checks++
}
function Make-Payload([string]$Executable, [string]$Version = $NewVersion) {
    return [ordered]@{version=$Version;codename='Fixture';architecture='x64';
        path="/launcher/releases/$Version/EternalLauncher.exe";size=(Get-Item $Executable).Length;
        sha256=(Get-FileHash $Executable -Algorithm SHA256).Hash.ToLowerInvariant()}
}
function New-Case([string]$Name) {
    $dir = Join-Path $testRoot ($Name + ' João com espaços')
    [IO.Directory]::CreateDirectory($dir) | Out-Null
    Copy-Item -LiteralPath $OldExecutable -Destination (Join-Path $dir 'Launcher á.exe')
    Copy-Item -LiteralPath $NewExecutable -Destination (Join-Path $dir 'artifact.exe')
    [IO.File]::WriteAllText((Join-Path $dir 'input.json'), (New-EternalEnvelope $testKey (Make-Payload $NewExecutable)))
    return $dir
}
function Start-Case([string]$Dir, [string]$Argument = '--run') {
    return Start-Process -FilePath (Join-Path $Dir 'Launcher á.exe') -ArgumentList $Argument -WindowStyle Hidden -PassThru
}
function Wait-Result([string]$Dir, [string]$Expected, [int]$Seconds = 20) {
    $deadline = [datetime]::UtcNow.AddSeconds($Seconds)
    $file = Join-Path $Dir 'result.txt'
    while (!(Test-Path $file) -and [datetime]::UtcNow -lt $deadline) { Start-Sleep -Milliseconds 100 }
    Assert-True (Test-Path $file) "missing result: $Expected"
    Assert-True ([IO.File]::ReadAllText($file) -eq $Expected) "expected result: $Expected"
    Start-Sleep -Milliseconds 350
}
function Verify-Case([string]$Name, [string]$Envelope, [bool]$Valid) {
    $file = Join-Path $testRoot "$Name.json"
    [IO.File]::WriteAllText($file, $Envelope)
    & $OldExecutable --verify $file | Out-Null
    Assert-True (($LASTEXITCODE -eq 0) -eq $Valid) "manifest $Name"
}
try {
    & $OldExecutable --unit
    Assert-True ($LASTEXITCODE -eq 0) 'numeric version comparison'
    $payload = Make-Payload $NewExecutable
    $valid = New-EternalEnvelope $testKey $payload
    Verify-Case 'valid' $valid $true
    Verify-Case 'malformed' '{broken' $false
    Verify-Case 'oversize' (' ' * 65537 + $valid) $false
    $altered = $valid | ConvertFrom-Json
    $altered.signature = ('A' * 512)
    Verify-Case 'signature' ($altered | ConvertTo-Json -Compress) $false
    $altered = $valid | ConvertFrom-Json
    $altered.payload = [Convert]::ToBase64String([Text.Encoding]::UTF8.GetBytes('{}'))
    Verify-Case 'payload-tamper' ($altered | ConvertTo-Json -Compress) $false
    foreach ($change in @(@{architecture='x86'}, @{path='/launcher/../evil.exe'},
        @{path='https://evil.example/launcher.exe'}, @{size=268435457}, @{size=1.5},
        @{sha256='bad'}, @{version='1.0.0-Aurora'})) {
        $bad = Make-Payload $NewExecutable
        foreach($key in $change.Keys) {$bad[$key]=$change[$key]}
        Verify-Case ('invalid-' + $script:checks) (New-EternalEnvelope $testKey $bad) $false
    }
    $foreign = [Security.Cryptography.RSA]::Create(3072)
    try { Verify-Case 'foreign-key' (New-EternalEnvelope $foreign $payload) $false } finally {$foreign.Dispose()}

    $case = New-Case 'success'; $process = Start-Case $case
    Wait-Result $case 'updated'
    Assert-True ((Get-FileHash (Join-Path $case 'Launcher á.exe')).Hash -eq (Get-FileHash $NewExecutable).Hash) 'new executable applied'
    $process.WaitForExit(5000) | Out-Null
    $second = Start-Case $case '--recover'; $second.WaitForExit(5000) | Out-Null
    Assert-True (!(Test-Path (Join-Path $case 'Launcher á.exe.update'))) 'cleanup after confirmation'

    foreach ($marker in @('fail-child', 'hang-child')) {
        $case = New-Case $marker
        [IO.File]::WriteAllText((Join-Path $case $marker), '1')
        $process = Start-Case $case
        Wait-Result $case 'rolledback' 40
        Assert-True ((Get-FileHash (Join-Path $case 'Launcher á.exe')).Hash -eq (Get-FileHash $OldExecutable).Hash) "rollback $marker"
    }
    $case = New-Case 'locked'
    $lockedFile = [IO.File]::Open((Join-Path $case 'Launcher á.exe'), 'Open', 'Read', 'Read')
    try { $process = Start-Case $case; Wait-Result $case 'rolledback' } finally { $lockedFile.Dispose() }

    $case = New-Case 'denied'
    $originalAcl = Get-Acl -LiteralPath $case
    $denyAcl = Get-Acl -LiteralPath $case
    $sid = [Security.Principal.WindowsIdentity]::GetCurrent().User
    $deny = [Security.AccessControl.FileSystemAccessRule]::new($sid,
        [Security.AccessControl.FileSystemRights]'CreateFiles,CreateDirectories', 'None', 'None', 'Deny')
    $denyAcl.AddAccessRule($deny)
    # Pre-create outputs so the fixture can report while directory creation is denied.
    [IO.File]::WriteAllText((Join-Path $case 'parent.txt'), '')
    [IO.File]::WriteAllText((Join-Path $case 'result.txt'), '')
    try {
        Set-Acl -LiteralPath $case -AclObject $denyAcl
        $process = Start-Case $case; $process.WaitForExit(10000) | Out-Null
        Assert-True ([IO.File]::ReadAllText((Join-Path $case 'result.txt')) -eq 'failed') 'folder without permission'
    } finally { Set-Acl -LiteralPath $case -AclObject $originalAcl }

    foreach ($scenario in @('offline','cancel','corrupt','older','equal')) {
        $case = New-Case $scenario
        $expected = 'failed'
        switch ($scenario) {
            'offline' {[IO.File]::WriteAllText((Join-Path $case 'offline'), '1')}
            'cancel' {[IO.File]::WriteAllText((Join-Path $case 'cancel'), '1'); $expected='cancelled'}
            'corrupt' {[IO.File]::WriteAllText((Join-Path $case 'artifact.exe'), 'corrupt')}
            'older' {[IO.File]::WriteAllText((Join-Path $case 'input.json'), (New-EternalEnvelope $testKey (Make-Payload $NewExecutable '0.0.1'))); $expected='unchanged'}
            'equal' {
                $parts=$NewVersion.Split('.'); $oldVersion="$($parts[0]).$($parts[1]).$([int]$parts[2]-1)"
                [IO.File]::WriteAllText((Join-Path $case 'input.json'), (New-EternalEnvelope $testKey (Make-Payload $NewExecutable $oldVersion))); $expected='unchanged'
            }
        }
        $process = Start-Case $case; Wait-Result $case $expected
        Assert-True ((Get-FileHash (Join-Path $case 'Launcher á.exe')).Hash -eq (Get-FileHash $OldExecutable).Hash) "preservation $scenario"
    }
    $case = New-Case 'concurrent'
    $holder = Start-Case $case '--hold-update'
    $until = [datetime]::UtcNow.AddSeconds(5)
    while (!(Test-Path (Join-Path $case 'holding.txt')) -and [datetime]::UtcNow -lt $until) {Start-Sleep -Milliseconds 50}
    $process = Start-Case $case; Wait-Result $case 'busy'
    $holder.WaitForExit(6000) | Out-Null

    foreach ($phase in @('prepared','applying','applied')) {
        $case = New-Case "recover-$phase"
        $tx = Join-Path $case 'Launcher á.exe.update'
        [IO.Directory]::CreateDirectory($tx) | Out-Null
        $oldHash = (Get-FileHash $OldExecutable).Hash.ToLowerInvariant()
        [IO.File]::WriteAllText((Join-Path $tx 'transaction.json'), (@{phase=$phase;nonce=('a'*32);originalHash=$oldHash} | ConvertTo-Json -Compress))
        Copy-Item -LiteralPath (Join-Path $case 'input.json') -Destination (Join-Path $tx 'manifest.json')
        if ($phase -eq 'applied') {
            Copy-Item -LiteralPath $OldExecutable -Destination (Join-Path $tx 'previous.exe')
            Copy-Item -LiteralPath $NewExecutable -Destination (Join-Path $case 'Launcher á.exe') -Force
        }
        $process = Start-Case $case '--recover'; Wait-Result $case 'recovered'
        Assert-True (!(Test-Path $tx)) "interruption recovery $phase"
    }
    Write-Host "AutoUpdate: $script:checks checks passed. Evidence: $testRoot"
} finally { $testKey.Dispose() }
