param (
    $Prefix = "/usr"
)

$ErrorActionPreference = "Stop"

$qtVersion = [version](qmake -query QT_VERSION)
Write-Host "Detected Qt version $qtVersion"

$os = $PSVersionTable.Platform
if ($os -eq 'Win32NT' -or $env:OS -eq 'Windows_NT') {
    dist/scripts/vcvars.ps1

    # Workaround for https://developercommunity.visualstudio.com/t/10664660
    $argVcrMutexWorkaround = 'DEFINES+=_DISABLE_CONSTEXPR_MUTEX_CONSTRUCTOR'
}
qmake PREFIX="$Prefix" DEFINES+="$env:nightlyDefines" $argVcrMutexWorkaround

if ($LASTEXITCODE -ne 0) {
    Write-Error "qmake failed"
    exit 1
}

if ($os -eq 'Win32NT' -or $env:OS -eq 'Windows_NT') {
    nmake
} else {
    make
}

if ($LASTEXITCODE -ne 0) {
    Write-Error "Build failed"
    exit 1
}

# Explorer thumbnail provider: a separate COM DLL with no Qt dependency
if ($os -eq 'Win32NT' -or $env:OS -eq 'Windows_NT') {
    New-Item -ItemType Directory -Path "build/thumbnail" -Force | Out-Null
    Push-Location "build/thumbnail"
    try {
        qmake ../../thumbnail/mithenviewthumbnail.pro
        if ($LASTEXITCODE -ne 0) {
            Write-Error "qmake failed for the thumbnail provider"
            exit 1
        }
        nmake
        if ($LASTEXITCODE -ne 0) {
            Write-Error "Thumbnail provider build failed"
            exit 1
        }
    } finally {
        Pop-Location
    }
    # The import library is only needed at link time
    Remove-Item "bin/MithenViewThumbnail.lib","bin/MithenViewThumbnail.exp" -Force -ErrorAction SilentlyContinue
}

# Stage translation catalogs for the installer. Only the language chosen during installation
# is copied into the install directory, so nothing is embedded in the executable.
$translationsDir = "dist/win/translations"
Remove-Item $translationsDir -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Path $translationsDir -Force | Out-Null

$mithenViewCatalogs = Get-ChildItem -Path ".qm" -Filter "mithen-view_*.qm" -ErrorAction SilentlyContinue
if (-not $mithenViewCatalogs) {
    Write-Error "No translation catalogs were built"
    exit 1
}
$mithenViewCatalogs | Copy-Item -Destination $translationsDir -Force

$qtTranslationsDir = (qmake -query QT_INSTALL_TRANSLATIONS).Trim()
foreach ($language in @('de','es','fr','ja','ko','ru','zh_CN')) {
    $qtCatalog = Join-Path $qtTranslationsDir "qtbase_$language.qm"
    if (Test-Path $qtCatalog) {
        Copy-Item $qtCatalog -Destination $translationsDir -Force
    } else {
        Write-Warning "Missing Qt catalog $qtCatalog"
    }
}

Write-Host "Staged translations in $translationsDir"
