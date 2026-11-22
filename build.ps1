param(
    [ValidateSet('x64','x86')][string]$Platform = 'x64',
    [ValidateSet('Debug','Release')][string]$Configuration = 'Release',
    [switch]$Package
)
$ErrorActionPreference = 'Stop'
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (!(Test-Path $vswhere)) { throw 'Install Visual Studio with Desktop development with C++.' }
$vs = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (!$vs) { throw 'Install the Desktop development with C++ workload and Windows SDK.' }
$tools = Get-ChildItem "$vs\VC\Tools\MSVC" -Directory | Sort-Object Name -Descending | Select-Object -First 1
$env:Path = "$($tools.FullName)\bin\Hostx64\x64;" + $env:Path
& "$vs\MSBuild\Current\Bin\MSBuild.exe" "$PSScriptRoot\analogClockProject.slnx" "/p:Configuration=$Configuration" "/p:Platform=$Platform" /v:minimal /nologo
if ($LASTEXITCODE -ne 0) { throw 'Build failed; see errors above.' }
if ($Package) {
    if ($Configuration -ne 'Release') { throw 'Only Release builds should be packaged.' }
    $arch = if ($Platform -eq 'x86') {'Win32'} else {'x64'}
    $destination = Join-Path $PSScriptRoot "dist\AnalogClock-$Platform"
    New-Item -ItemType Directory -Force $destination | Out-Null
    Copy-Item "$PSScriptRoot\bin\$arch\Release\analogClockProject.exe", "$PSScriptRoot\bin\$arch\Release\glfw3.dll", "$PSScriptRoot\bin\$arch\Release\logo.jpeg" $destination -Force
    Copy-Item "$PSScriptRoot\third_party\glfw\LICENSE.md" "$destination\GLFW-LICENSE.md" -Force
    'Extract the entire folder, then double-click analogClockProject.exe. Keep glfw3.dll and logo.jpeg beside it. Press Escape or Q to close. Requires Windows with an OpenGL-capable graphics driver.' | Set-Content "$destination\START-HERE.txt"
    Compress-Archive "$destination\*" "$PSScriptRoot\dist\AnalogClock-$Platform.zip" -Force
    Write-Host "Package: $PSScriptRoot\dist\AnalogClock-$Platform.zip"
}
