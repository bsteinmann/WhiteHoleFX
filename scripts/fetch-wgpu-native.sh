#!/usr/bin/env bash
# Lädt wgpu-native (Header und Bibliothek aus demselben Paket) nach third_party/wgpu-native.
#
# Warum es dieses Skript gibt: Das GPU-Backend braucht wgpu-native, und webgpu.h muss zur
# Bibliothek passen (siehe docs/gpu-backend.md). Das Skript nimmt beides aus einem Paket, in der
# Version aus scripts/wgpu-native-version.txt ("latest" oder ein Tag wie v25.0.2.1).
#
# Aufruf:  scripts/fetch-wgpu-native.sh            (Linux, macOS)
# Danach:  cmake --preset studio-debug             (siehe BUILDING.md)
set -euo pipefail

root="$(cd "$(dirname "$0")/.." && pwd)"
version="$(tr -d '[:space:]' < "$root/scripts/wgpu-native-version.txt")"
target="$root/third_party/wgpu-native"

case "$(uname -s)-$(uname -m)" in
    Linux-x86_64)  asset="wgpu-linux-x86_64-release.zip" ;;
    Linux-aarch64) asset="wgpu-linux-aarch64-release.zip" ;;
    Darwin-arm64)  asset="wgpu-macos-aarch64-release.zip" ;;
    Darwin-x86_64) asset="wgpu-macos-x86_64-release.zip" ;;
    *) echo "Fehler: Für $(uname -s)-$(uname -m) gibt es kein fertiges Paket; siehe docs/gpu-backend.md." >&2; exit 1 ;;
esac

if [ "$version" = "latest" ]; then
    url="https://github.com/gfx-rs/wgpu-native/releases/latest/download/$asset"
else
    url="https://github.com/gfx-rs/wgpu-native/releases/download/$version/$asset"
fi

for tool in curl unzip; do
    command -v "$tool" > /dev/null || { echo "Fehler: \"$tool\" fehlt (z. B. sudo apt install $tool)." >&2; exit 1; }
done

tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT
echo "Lade $url"
curl -fL --retry 3 -o "$tmp/$asset" "$url"
rm -rf "$target"
mkdir -p "$target"
unzip -q "$tmp/$asset" -d "$target"

if [ ! -f "$target/include/webgpu/webgpu.h" ]; then
    echo "Fehler: include/webgpu/webgpu.h fehlt im Paket (Ordner $target)." >&2
    exit 1
fi
echo "Fertig: $target  (Version: $version)"
echo "Zum Konfigurieren: -DWGPU_NATIVE_ROOT=\"$target\"  oder  cmake --preset studio-debug"
