#!/usr/bin/env pwsh
param(
    # Re-download even when the plugins are already deployed
    [switch]$Force
)

$ErrorActionPreference = "Stop"

$qtVersion = [version](qmake -query QT_VERSION)
Write-Host "Detected Qt version $qtVersion"

$osName = 'Windows'
$pluginNames = @('QtApng', 'KImageFormats')

if (-not $env:buildArch) {
    $env:buildArch = 'X64'
}
$arch = $env:buildArch

$out_frm = "bin"
$out_imf = "bin/imageformats"

# Prefer the kimageformats TGA plugin over Qt's duplicate (same rule as windeployqt.ps1,
# which runs before this script so it cannot see the downloaded kimg_tga)
if ((Test-Path (Join-Path $out_imf "kimg_tga.dll")) -and (Test-Path (Join-Path $out_imf "qtga.dll"))) {
    Remove-Item (Join-Path $out_imf "qtga.dll") -Force
}

# Rebuilds would otherwise re-download every time; the deployed AVIF plugin marks a good run
if (-not $Force -and (Test-Path (Join-Path $out_imf "kimg_avif.dll"))) {
    Write-Host "Codec plugins already deployed; skipping (pass -Force to refresh)"
    return
}

# The release only carries selected Qt patch versions (e.g. 6.8.3 while the runtime is
# 6.8.2), so resolve the highest patch with the same major.minor instead of building the
# asset name from the exact runtime version.
Write-Host "Querying available bundles..."
$release = Invoke-RestMethod -Uri "https://api.github.com/repos/jdpurcell/kimageformats-binaries/releases/latest"

function Resolve-Asset($pluginName) {
    $pattern = "^$pluginName-$osName-(\d+\.\d+\.\d+)-$arch\.zip$"
    $candidates = @()
    foreach ($asset in $release.assets) {
        if ($asset.name -match $pattern) {
            $candidates += [pscustomobject]@{
                Name = $asset.name
                Url = $asset.browser_download_url
                Version = [version]$Matches[1]
            }
        }
    }
    if ($candidates.Count -eq 0) {
        throw "No $pluginName bundle found for $osName/$arch"
    }
    $sameMinor = @($candidates | Where-Object {
        $_.Version.Major -eq $qtVersion.Major -and $_.Version.Minor -eq $qtVersion.Minor
    } | Sort-Object Version -Descending)
    if ($sameMinor.Count -gt 0) {
        return $sameMinor[0]
    }
    $fallback = $candidates | Sort-Object Version -Descending | Select-Object -First 1
    Write-Warning "No $pluginName bundle for Qt $($qtVersion.Major).$($qtVersion.Minor); using $($fallback.Version)"
    return $fallback
}

foreach ($pluginName in $pluginNames) {
    $asset = Resolve-Asset $pluginName
    Write-Host "Downloading $($asset.Url)"
    Invoke-WebRequest -Uri $asset.Url -OutFile $asset.Name
    Expand-Archive $asset.Name -DestinationPath $pluginName -Force
    Remove-Item $asset.Name
}

New-Item -ItemType Directory -Path $out_frm -Force | Out-Null
New-Item -ItemType Directory -Path $out_imf -Force | Out-Null

function MoveLibraries($category, $destDir, $files) {
    foreach ($file in $files) {
        Write-Host "${category}: $($file.Name) ($($file.LastWriteTimeUtc.ToString("yyyy-MM-dd HH:mm:ss")))"
        Move-Item -Path $file.FullName -Destination $destDir -Force
    }
}

# Deploy QtApng
if ($pluginNames -contains 'QtApng') {
    Write-Host "`nDeploying QtApng:"
    MoveLibraries 'imf' $out_imf (Get-ChildItem "QtApng" -File)
}

# Deploy KImageFormats (kimg_* plugins + their dependency DLLs)
if ($pluginNames -contains 'KImageFormats') {
    Write-Host "`nDeploying KImageFormats:"
    MoveLibraries 'imf' $out_imf (Get-ChildItem "KImageFormats" -File -Filter "kimg_*")
    MoveLibraries 'frm' $out_frm (Get-ChildItem "KImageFormats" -File | Where-Object { $_.Name -notlike 'kimg_*' })
}

Remove-Item "QtApng", "KImageFormats" -Recurse -Force -ErrorAction SilentlyContinue

Write-Host ''
