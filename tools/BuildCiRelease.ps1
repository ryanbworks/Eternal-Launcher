#Requires -Version 7.0
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$Version,
    [Parameter(Mandatory)][string]$Codename,
    [string]$BuildDirectory = 'out/build/ci'
)
$ErrorActionPreference = 'Stop'
if ($Version -cnotmatch '^(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)$' -or
    @($Version.Split('.') | Where-Object { [decimal]$_ -gt 65535 }).Count) {
    throw 'Informe uma versão como 1.0.12, com cada componente entre 0 e 65535.'
}
if ($Codename -cnotmatch '^[A-Za-z0-9]{1,40}$') { throw 'Nome da versão: use de 1 a 40 letras ou números.' }
$projectRoot = Split-Path $PSScriptRoot -Parent
Set-Location -LiteralPath $projectRoot
$cmakePath = Join-Path $projectRoot 'CMakeLists.txt'
$text = [IO.File]::ReadAllText($cmakePath)
$pattern = 'project\(EternalLauncher VERSION ([0-9]+\.[0-9]+\.[0-9]+)'
$current = [regex]::Match($text, $pattern).Groups[1].Value
if (!$current -or [version]$Version -lt [version]$current) { throw 'A versão não pode ser menor que a versão do código.' }
$text = [regex]::Replace($text, $pattern, "project(EternalLauncher VERSION $Version")
$text = [regex]::Replace($text, 'set\(ETERNAL_CODENAME "[A-Za-z0-9]+"\)', "set(ETERNAL_CODENAME `"$Codename`")")
[IO.File]::WriteAllText($cmakePath, $text)
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$vs = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (!$vs) { throw 'Visual Studio C++ não encontrado.' }
Import-Module (Join-Path $vs 'Common7/Tools/Microsoft.VisualStudio.DevShell.dll')
Enter-VsDevShell -VsInstallPath $vs -SkipAutomaticLocation -DevCmdArguments '-arch=x64 -host_arch=x64' | Out-Null
$cmake = Join-Path $vs 'Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe'
$ctest = Join-Path (Split-Path $cmake) 'ctest.exe'
& $cmake -S . -B $BuildDirectory -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON `
    -DETERNAL_REQUIRE_UPDATE_TESTS=ON "-DETERNAL_POWERSHELL:FILEPATH=$(Join-Path $PSHOME 'pwsh.exe')"
if ($LASTEXITCODE) { throw 'Falha na configuração.' }
& $cmake --build $BuildDirectory
if ($LASTEXITCODE) { throw 'Falha na compilação.' }
& $ctest --test-dir $BuildDirectory --output-on-failure
if ($LASTEXITCODE) { throw 'Testes reprovados; publicação bloqueada.' }
