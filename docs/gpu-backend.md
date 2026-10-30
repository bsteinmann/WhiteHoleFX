# GPU-Backend (wh_gpu_webgpu)

`WebGpuDevice` setzt `IGpuDevice` auf der WebGPU-C-Schnittstelle (`webgpu.h`) um. Damit
laufen die erzeugten WGSL-Shader auf einer echten GPU oder, für Tests, auf einem
Software-Rasterizer (lavapipe). Kein Header des Moduls enthält `webgpu.h`; der Rest der
Engine kennt nur `IGpuDevice`.

**Stand:** läuft in der CI (Job „GPU-Backend“) gegen wgpu-native auf lavapipe, einem
Software-Vulkan. `tests/gpu_device/webgpu_device_test.cpp` prüft Puffer, Compute, Texturen
und Fehlerfälle (8 Fälle bestanden). `tests/gpu_device/stage_on_device_test.cpp` lässt die
vom Compiler erzeugten Shader der Canvas- und der Abtast-Stage laufen und vergleicht mit
der CPU-Rechnung. Auf einer echten GPU (nicht Software) wurde es noch nicht geprüft; das
geht nur auf Ihrem Rechner.

## Warum wgpu-native zuerst

Dawn und wgpu-native implementieren dieselbe C-Schnittstelle. wgpu-native gibt es als
fertige Pakete (Header und Bibliothek zusammen), Dawn muss mit seinen Abhängigkeiten
selbst gebaut werden. Der Code hängt nur an `webgpu.h`; Dawn kann später folgen, ohne dass
sich etwas an `IGpuDevice` oder den Stages ändert.

**Header und Bibliothek müssen zusammenpassen:** `webgpu.h` ändert sich zwischen Versionen
(Callback-Strukturen, Feldnamen). Darum nimmt die CI beides aus demselben Paket.

## Bauen

```bash
cmake -S . -B build -DWHITEHOLEFX_WEBGPU=ON -DWGPU_NATIVE_ROOT=/pfad/zu/wgpu-native
cmake --build build --target wh_gpu_tests
./build/tests/wh_gpu_tests
```

`WGPU_NATIVE_ROOT` enthält `include/webgpu/{webgpu.h,wgpu.h}` und `lib/libwgpu_native.*`.
Ohne Adapter werden die Tests übersprungen. Für einen Lauf ohne GPU unter Linux:
`sudo apt install mesa-vulkan-drivers`.

## Entwurfsentscheidungen

- **Layout aus dem WGSL-Text.** `readBindings()` liest alle `@group/@binding`-Deklarationen,
  das Backend baut daraus Bind-Group- und Pipeline-Layouts selbst. Ein automatisches
  Layout enthält nur Bindungen, die der Shader benutzt; ein konstanter Graph liest den
  Parameterpuffer nie, die Engine bindet ihn aber immer.
- **Strenge Bindungsprüfung.** Ein Dispatch läuft nur, wenn jede deklarierte Bindung
  gebunden ist, die Ressourcenart passt (Puffer/Textur), Usage und Texturformat stimmen und
  nichts Überzähliges gebunden ist. Sonst meldet `submit()` den Grund im Klartext.
- **Fehler kommen asynchron.** WebGPU meldet Validierungsfehler über einen Callback. Sie
  werden gesammelt; Anlegen und `submit()` prüfen danach, `takeErrors()` liefert den Rest.
- **Readback.** Der Bibliotheks-Callback setzt nur einen Status; die Nutzer-Callbacks laufen
  in `poll()`, außerhalb von Bibliotheksaufrufen. Ein Puffer mit laufendem Readback bleibt
  über eine eigene Referenz am Leben, auch wenn er inzwischen mit `destroy()` freigegeben wurde.
- **Zeilen mit 256 Byte Ausrichtung.** `copyTextureToBuffer` schreibt Zeilen mit
  `paddedBytesPerRow()` Bytes (Vorgabe von WebGPU). Wer ein Canvas zurückliest, muss das
  beim Auslesen berücksichtigen.
- **Puffergrößen** werden auf Vielfache von 4 aufgerundet; `writeBuffer` verlangt Offset
  und Länge als Vielfache von 4.

## Grenzen

- Nur 2D-Texturen, nur WGSL, nur Linux/macOS-Pfade getestet vorgesehen (Windows unbekannt).
- Es gelten die Standard-Limits des Adapters (z. B. 128 MiB je Storage-Puffer). Sehr große
  Layouts müssten höhere Limits anfordern.
- Bind-Groups entstehen pro Dispatch neu; das ist bei wenigen Dispatches pro Frame
  unkritisch und lässt sich später zwischenspeichern.
- Keine Timestamp-Queries, kein Multi-GPU, kein Surface: die Vorschau in der Oberfläche
  bekommt später eigene Anbindung.
