# WhiteHoleFX bauen und starten

Diese Anleitung bringt dich von einem frischen Rechner bis zum laufenden Programm. Es gibt drei
Ausbaustufen; jede enthält die vorige.

| Stufe | Enthält | Braucht zusätzlich |
|---|---|---|
| **Kern** | alle Bibliotheken, Tests, Konsolen-Runner mit Prüfung (`--check`) und Vorschaubild | nichts |
| **+ GPU** | Effekte wirklich rendern und per Art-Net/sACN/DDP senden (`whitehole_cli`) | wgpu-native, Grafiktreiber mit Vulkan (Linux) bzw. Direct3D 12 / Vulkan (Windows) |
| **+ Oberfläche** | Fenster mit Vorschau, Reglern, Node-Editor, Layout-Editor (`whitehole_studio`) | Qt 6 (Widgets), dazu die GPU-Stufe |

**Windows ohne Bauen:** Die CI baut bei jedem Push ein fertiges Paket (Artefakt
„WhiteHoleFX-Windows“ unter *Actions → letzter Lauf*). Entpacken, `whitehole_studio.exe`
starten. Der Rest dieser Anleitung ist für alle, die selbst bauen wollen.

## Voraussetzungen

- **CMake 3.25 oder neuer**
- **C++20-Compiler:** GCC 13+, Clang 16+ oder Visual Studio 2022 (MSVC 19.3x)
- Internet beim ersten Konfigurieren (Catch2 für die Tests wird geladen) und für das Skript, das
  wgpu-native holt

## Linux (Ubuntu 24.04 / Debian 13 und ähnlich)

```bash
sudo apt install build-essential cmake git curl unzip \
    qt6-base-dev libgl1-mesa-dev libxkbcommon-dev \
    libvulkan1 mesa-vulkan-drivers vulkan-tools
```

Fedora: `sudo dnf install gcc-c++ cmake git curl unzip qt6-qtbase-devel mesa-vulkan-drivers vulkan-loader`.
Arch: `sudo pacman -S base-devel cmake git curl unzip qt6-base vulkan-icd-loader`.
Ubuntu 22.04 liefert nur GCC 11; installiere `g++-13` und setze `CXX=g++-13`.

Für eine NVIDIA-, AMD- oder Intel-GPU brauchst du deren Vulkan-Treiber (meist schon da). Ohne
GPU geht es mit dem Software-Rasterizer `mesa-vulkan-drivers` (langsam, aber lauffähig).

**Nur Kern** (Tests und Prüfung, keine GPU nötig):

```bash
cmake --preset dev
cmake --build --preset dev --parallel
ctest --test-dir build/dev --output-on-failure
```

**Mit GPU und Oberfläche:**

```bash
scripts/fetch-wgpu-native.sh            # lädt wgpu-native nach third_party/
cmake --preset studio-release
cmake --build --preset studio-release --parallel
./build/studio-release/apps/whitehole_studio/whitehole_studio examples/regler.whfx
```

Hast du Qt an einem anderen Ort als dem Systempfad (z. B. aus dem Qt-Installer), gib ihn mit:
`cmake --preset studio-release -DCMAKE_PREFIX_PATH=$HOME/Qt/6.8.3/gcc_64`.

## Linux-Paket (.deb) installieren

Für Ubuntu 24.04 / Debian 13 und neuer (64 Bit) gibt es ein fertiges Paket. Es steckt als Artefakt
`WhiteHoleFX-Linux-deb` in jedem CI-Lauf (GitHub, Reiter „Actions“). Installieren:

```bash
sudo apt install ./whiteholefx_0.12.0-1_amd64.deb
```

`apt` holt Qt 6 und Vulkan automatisch. Danach gibt es `whitehole_studio` (auch im Anwendungsmenü,
Dateityp `.whfx`) und `whitehole_cli`; Beispielprojekte liegen unter `/usr/share/whiteholefx/examples`
und im Studio-Menü Datei → Beispiele. Ohne Grafikkarte hilft `mesa-vulkan-drivers` (Software-Vulkan).
Entfernen: `sudo apt remove whiteholefx`.

Selbst bauen:

```bash
scripts/fetch-wgpu-native.sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr \
  -DWHITEHOLEFX_BUILD_STUDIO=ON -DWHITEHOLEFX_WEBGPU=ON -DWGPU_NATIVE_ROOT="$PWD/third_party/wgpu-native"
cmake --build build --parallel
(cd build && cpack -G DEB)
```

Die CI installiert das Paket in einem sauberen Ubuntu-24.04-Container und startet es dort.

## Windows 10/11

1. **Visual Studio 2022** (Community reicht) mit der Workload „Desktopentwicklung mit C++“
   installieren. Sie bringt CMake mit.
2. **Qt 6.8** über den [Qt Online Installer](https://www.qt.io/download-qt-installer-oss)
   installieren, Komponente **„MSVC 2022 64-bit“** (Pfad danach z. B. `C:\Qt\6.8.3\msvc2022_64`).
3. In einer **PowerShell** im Projektordner:

```powershell
powershell -ExecutionPolicy Bypass -File scripts\fetch-wgpu-native.ps1
cmake --preset studio-release -DCMAKE_PREFIX_PATH=C:/Qt/6.8.3/msvc2022_64
cmake --build --preset studio-release --parallel
.\build\studio-release\apps\whitehole_studio\Release\whitehole_studio.exe examples\regler.whfx
```

`wgpu_native.dll` kopiert CMake neben das Programm. Die Qt-DLLs fehlen dort noch; zum Weitergeben
oder Starten per Doppelklick einmal ausführen:

```powershell
C:\Qt\6.8.3\msvc2022_64\bin\windeployqt.exe --release .\build\studio-release\apps\whitehole_studio\Release\whitehole_studio.exe
```

Tests unter Windows: `.\build\studio-release\tests\Release\wh_tests.exe` direkt starten
(`ctest` kommt mit den deutschen Umlauten in den Testnamen nicht zurecht).

## Erste Schritte nach dem Bauen

Ohne GPU, ohne Netzwerk, nur Prüfung und Bild des Layouts:

```bash
whitehole_cli examples/wand.whfx --check --snapshot layout.png
```

Mit GPU, 60 Frames rendern und das Ergebnis als Bild speichern:

```bash
whitehole_cli examples/wand.whfx --frames 60 --snapshot vorschau.png
```

Oberfläche: `whitehole_studio examples/regler.whfx`. **Start/Stopp** mit der Leertaste. Die
**Netzwerkausgabe ist nach dem Start aus**; erst „Ausgabe senden“ schickt Pakete an die Controller.
Eigene Projekte: Ziele (Adressen deiner Controller), Fixtures und Patches im Dock „Layout“
einstellen, Effekte im Dock „Nodes“ bauen. Details: [Oberfläche](docs/studio.md),
[Projektdatei](docs/project-format.md).

## Fehlersuche

| Meldung / Beobachtung | Ursache und Abhilfe |
|---|---|
| `Could NOT find Qt6` | Qt-Pfad angeben: `-DCMAKE_PREFIX_PATH=<Qt-Ordner>` (Ordner, der `lib/cmake/Qt6` enthält) |
| `wgpu-native wurde nicht gefunden` | `scripts/fetch-wgpu-native.sh` bzw. `.ps1` ausführen oder `-DWGPU_NATIVE_ROOT=<Ordner mit include/ und lib/>` |
| Fehler in `webgpu.h` beim Kompilieren | Header und Bibliothek stammen aus verschiedenen Versionen; Paket neu laden. Die getestete Version steht in `scripts/wgpu-native-version.txt` |
| „Kein GPU-Gerät“ | Treiber mit Vulkan (Linux) bzw. aktuellen Grafiktreiber (Windows) installieren; Test mit `--software` (Software-Adapter) |
| Fenster bleibt schwarz | Projekt läuft nicht: Leertaste. Mit `Ausgabe aus` ist das normal für das Netz, die Vorschau läuft trotzdem |
| Qt-Fenster startet auf Linux nicht | `QT_QPA_PLATFORM=xcb` bzw. `wayland` setzen; auf Servern ohne Bildschirm `QT_QPA_PLATFORM=offscreen` |
| `whitehole_studio.exe` meldet fehlende Qt-DLLs | `windeployqt` ausführen (siehe oben) |
| Windows: „No test cases matched“ bei ctest | Umlaute in Testnamen: `wh_tests.exe` direkt starten |

## Für Entwickler

- `cmake --preset dev` baut mit **Warnungen als Fehler** wie die CI (GCC mit Address- und
  Undefined-Behavior-Sanitizer: `-DCMAKE_CXX_FLAGS="-fsanitize=address,undefined"`).
- Eigene Einstellungen (Qt-Pfad, Generator) gehören in `CMakeUserPresets.json`; die Datei steht in
  der `.gitignore`.
- Die CI (`.github/workflows/ci.yml`) baut GCC, Clang, Windows, das GPU-Backend auf Linux und
  Windows, die Oberfläche auf Linux (Rauchtest im unsichtbaren Fenster) und Windows (fertiges Paket).
