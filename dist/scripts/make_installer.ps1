<#
    Builds the complete MithenView release: bin\mithen-view.exe (plus the Explorer thumbnail provider)
    and the one-file installer res\MithenView-setup.exe.

    Prerequisites: Qt on PATH (6.8.2 msvc2022_64 with the qtimageformats module),
    Visual Studio 2022 Build Tools, NSIS.

    Run from the repository root:
        powershell -File dist/scripts/make_installer.ps1 [-WithPlugins]
#>
param (
    # Also download the optional codec plugins (needs network access)
    [switch]$WithPlugins
)

$ErrorActionPreference = "Stop"

# Default to the host architecture unless the caller (or CI) already chose one
if (-not $env:buildArch) {
    $env:buildArch = 'X64'
}

# --- Prerequisites -----------------------------------------------------------

if (-not (Get-Command qmake -ErrorAction SilentlyContinue)) {
    Write-Error "qmake was not found on PATH; put the Qt bin directory on PATH first."
    exit 1
}

$makensis = (Get-Command makensis -ErrorAction SilentlyContinue).Source
if (-not $makensis) {
    foreach ($candidate in @("$env:ProgramFiles\NSIS\makensis.exe", "${env:ProgramFiles(x86)}\NSIS\makensis.exe")) {
        if (Test-Path $candidate) {
            $makensis = $candidate
            break
        }
    }
}
if (-not $makensis) {
    Write-Error "makensis was not found; install NSIS or put it on PATH."
    exit 1
}

# --- Build -------------------------------------------------------------------

Write-Host "`n[1/4] Building application, thumbnail provider and translation catalogs..." -ForegroundColor Yellow
dist/scripts/build.ps1
if ($LASTEXITCODE -ne 0) {
    Write-Error "Application build failed"
    exit 1
}

Write-Host "`n[2/4] Deploying the Qt runtime..." -ForegroundColor Yellow
dist/scripts/windeployqt.ps1
if ($LASTEXITCODE -ne 0) {
    Write-Error "Qt deployment failed"
    exit 1
}

if ($WithPlugins) {
    Write-Host "`n[3/4] Downloading optional codec plugins..." -ForegroundColor Yellow
    dist/scripts/download-plugins.ps1
    if ($LASTEXITCODE -ne 0) {
        Write-Error "Codec plugin download failed"
        exit 1
    }
} else {
    Write-Host "`n[3/4] Skipping optional codec plugins (pass -WithPlugins to include them)" -ForegroundColor Yellow
}

Write-Host "`n[4/4] Building the installer..." -ForegroundColor Yellow
& $makensis "dist/scripts/installer.nsi"
if ($LASTEXITCODE -ne 0) {
    Write-Error "Installer build failed"
    exit 1
}

# --- Result ------------------------------------------------------------------

$installer = Get-Item "res/MithenView-setup.exe"
$app = Get-Item "bin/mithen-view.exe"
Write-Host ("`nDone:`n  {0}  ({1:N0} bytes)`n  {2}  ({3:N0} bytes)" -f $installer.FullName, $installer.Length, $app.FullName, $app.Length) -ForegroundColor Green
