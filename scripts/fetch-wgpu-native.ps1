# Lädt wgpu-native (Header und Bibliothek aus demselben Paket) nach third_party\wgpu-native.
#
# Warum es dieses Skript gibt: Das GPU-Backend braucht wgpu-native, und webgpu.h muss zur
# Bibliothek passen (siehe docs/gpu-backend.md). Das Skript nimmt beides aus einem Paket, in der
# Version aus scripts\wgpu-native-version.txt ("latest" oder ein Tag wie v25.0.2.1).
#
# Aufruf (PowerShell):  .\scripts\fetch-wgpu-native.ps1
$ErrorActionPreference = "Stop"
$ProgressPreference = "SilentlyContinue"

$root = Split-Path -Parent $PSScriptRoot
$version = (Get-Content (Join-Path $root "scripts\wgpu-native-version.txt") -Raw).Trim()
$target = Join-Path $root "third_party\wgpu-native"
$asset = "wgpu-windows-x86_64-msvc-release.zip"

if ($version -eq "latest") {
    $url = "https://github.com/gfx-rs/wgpu-native/releases/latest/download/$asset"
} else {
    $url = "https://github.com/gfx-rs/wgpu-native/releases/download/$version/$asset"
}

$zip = Join-Path ([System.IO.Path]::GetTempPath()) $asset
Write-Output "Lade $url"
Invoke-WebRequest -Uri $url -OutFile $zip
if (Test-Path $target) { Remove-Item -Recurse -Force $target }
New-Item -ItemType Directory -Force -Path $target | Out-Null
Expand-Archive -Path $zip -DestinationPath $target
Remove-Item $zip

if (-not (Test-Path (Join-Path $target "include\webgpu\webgpu.h"))) {
    throw "include\webgpu\webgpu.h fehlt im Paket (Ordner $target)."
}
Write-Output "Fertig: $target  (Version: $version)"
Write-Output "Zum Konfigurieren: -DWGPU_NATIVE_ROOT=`"$target`"  oder  cmake --preset studio-release"
