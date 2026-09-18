# Determine architecture argument
$argArch = $null
if ($env:buildArch -eq 'X64') {
    $argArch = 'x64'
} elseif ($env:buildArch -eq 'X86') {
    $argArch = 'x64_x86'
} elseif ($env:buildArch -eq 'Arm64') {
    $argArch = 'x64_arm64'
}
if (-not $argArch) {
    throw 'Unsupported build architecture. Set $env:buildArch to X64, X86, or Arm64.'
}

$programFilesX86 = [Environment]::GetFolderPath('ProgramFilesX86')
$vsWhere = Join-Path $programFilesX86 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path $vsWhere)) {
    throw 'vswhere.exe not found. Please install Visual Studio Build Tools.'
}

$vsDir = & $vsWhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vsDir) {
    throw 'Visual Studio with C++ tools not found.'
}

$vcvars = Join-Path $vsDir 'VC\Auxiliary\Build\vcvarsall.bat'
if (-not (Test-Path $vcvars)) {
    throw "vcvarsall.bat not found at: $vcvars"
}

Write-Host "Setting up $argArch build environment from: $vsDir"
$exclusions = @('VCPKG_ROOT')
cmd /c "`"$vcvars`" $argArch >nul 2>&1 && set" | ForEach-Object {
    $parts = $_ -Split '=', 2
    if ($parts[0] -and $parts.Count -eq 2 -and $parts[0] -notin $exclusions) {
        [Environment]::SetEnvironmentVariable($parts[0], $parts[1])
    }
}
Write-Host "Build environment ready."
