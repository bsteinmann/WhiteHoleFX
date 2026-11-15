# Ausgabe (wh_output)

Dieses Modul macht aus den Farben der Abtast-Stage Netzwerkpakete. Fast alles ist reine
Rechenlogik und ohne Netzwerk testbar; nur `udp_sender.cpp` berührt das Betriebssystem.
Kein Qt.

## Weg einer Farbe

```
SamplingRenderStage (GPU)  ->  16 Byte pro LED, linear RGBA, f32
        |  Readback (RenderEngine) -> IFrameSink
        v
LatestFrameMailbox  (OutputWorker, eigener Thread; alte Frames werden übersprungen)
        v
OutputPipeline::processBytes(bytes, channels)
   OutputPlan::render      ->  ColorEncoder (Alpha, Helligkeit, Gamma, Reihenfolge)
   applyChannels           ->  DMX-Output-Nodes (ChannelWrite) in Fixture- oder eigene Puffer eintragen
   encodeArtDmx / encodeSacn / encodeDdpStream
        v
IPacketSink::send  (UdpSender im Betrieb, RecordingSink im Test)
```

`channels` kommt aus `PixelFrame::channels`: Die Session liest nach jedem gerenderten Frame die Senken-Nodes
`wh.output.dmx`/`wh.output.dmxcolor` aus dem `ValueStore` (`allChannels()`) und hängt sie an den Frame, bevor er
zum Ausgabe-Thread geht (`libs/wh_runtime/src/session.cpp`).

## Bausteine

| Datei | Aufgabe |
|---|---|
| `color_encoder` | Float-RGBA -> 3 Bytes; Alpha über Schwarz, Helligkeit, Gamma (`v^gamma`), Kanalreihenfolge (RGB, GRB, ...) |
| `output_plan` | Patch-Prüfung, Aufteilung der LEDs auf Universen, Füllen der Puffer |
| `artnet` | ArtDmx (Port 6454) |
| `sacn` | E1.31-Datenpaket (Port 5568, Multicast 239.255.x.y) |
| `ddp` | DDP-Pakete zu höchstens 1440 Bytes (Port 4048) |
| `packet_sink` | Schnittstelle `IPacketSink` und `Endpoint` (Adresse + Port) |
| `output_pipeline` | pro Frame: rendern, Pakete bauen, senden; Sequenznummern, Zähler |
| `output_pipeline_channels` | DMX-Output-Nodes (`ChannelWrite`) in Fixture- oder eigene Puffer eintragen |
| `output_worker` | Thread, der Frames aus dem Briefkasten holt und die Pipeline ausführt |
| `udp_sender` | nicht blockierender IPv4-UDP-Socket (POSIX und Winsock) |

## Patchen

Ein `FixturePatch` sagt, wohin ein Fixture geht: Protokoll, Ziel-Nummer, Universe,
Startkanal (1..510), Farbeinstellungen. Regeln:

- Eine LED belegt 3 Kanäle und wird nie auf zwei Universen geteilt: höchstens 170 LEDs
  je Universe. Der Rest läuft ab Kanal 1 im nächsten Universe weiter.
- Mehrere Fixtures dürfen sich ein Universe teilen, solange ihre Kanäle frei sind.
- DDP kennt keine Universen: ein Fixture ist ein Strom ab `ddpOffset`.
- Übersprungene Pixel (`skipFirst`, `skipEvery`/`skipCount`): Pixel am Kabel, die nicht leuchten, zum Beispiel
  ein Pixel als Signalverstärker vor dem Strip oder nach jeder Reihe. Sie bekommen Schwarz, belegen aber ihre
  Kanäle (auch beim Prüfen auf Überlappung), und die Adressen der LEDs dahinter rücken weiter. Die Rechnung steht
  einmal in `dataSlot()` (output_plan.hpp) und gilt für den Plan, `ledAddress()` und `channelBlocks()`.
- Fixtures ohne Patch werden nicht ausgegeben.
- Alle Fehler (unbekanntes Fixture, doppelter Patch, Überlappung, Universe außerhalb
  des Bereichs) fallen beim Erstellen des Plans auf, nicht im laufenden Betrieb.

## Ziele (Controller)

`OutputConfig::destinations[n]` gibt Adresse, Port und **Protokoll** für Ziel `n` an (Port 0 = Standard
des Protokolls; der Patch folgt dem Protokoll seines Ziels, siehe `output/patch_protocol.hpp`). Adressen müssen IPv4-Zahlen sein; Rechnernamen werden bewusst nicht
aufgelöst, weil eine DNS-Anfrage mitten im Frame den Ausgabe-Thread blockieren würde.
Bei sACN bedeutet eine leere Adresse Multicast an `239.255.<Universe>`; Art-Net und DDP
brauchen eine Adresse (Broadcast-Adressen funktionieren).

## Sequenznummern

Je Slot (also je Universe bzw. DDP-Strom) getrennt: Art-Net 1..255 (die 0 hieße "aus"),
sACN 0..255, DDP 1..15. Alle Pakete eines DDP-Frames teilen eine Nummer.

## Fehlerverhalten

Ein Paket, das nicht gesendet werden kann, wird in `stats().sendErrors` gezählt und in
`lastSendError()` gemerkt; die übrigen Pakete gehen trotzdem raus. Ein Frame mit zu
wenigen Bytes wird verworfen (`OutputWorker::frameErrors()`), der Thread läuft weiter.
Ist der Ausgang langsamer als die Engine, überspringt er alte Frames (`droppedFrames()`).

## Gamma

Effekt-Werte sind so gemeint, wie man sie auf einem Bildschirm sieht. Die LED strahlt
linear, daher wird potenziert: `out = v^gamma`. Gamma 1 lässt alles unverändert, 2,2 ist
ein üblicher Startwert. Das ist eine Konvention; wer lineares Licht will, nimmt Gamma 1.
Die Vorschau im Studio rechnet genauso (`sRGB(v^gamma)` mit dem Gamma des Patches je Fixture), damit
sie zeigt, was die LEDs ausgeben. Farbwähler speichern den Wert, den man sieht, ohne Umrechnung.

## DMX-Ausgabe aus dem Graph

Die Nodes `wh.output.dmx` und `wh.output.dmxcolor` (Kategorie „Output", siehe `docs/value-graph.md`,
Abschnitt „DMX-Ausgabe") schreiben Werte aus dem Node-Graph auf DMX-Kanäle, unabhängig von LEDs. Ein Node
kennt dabei (seit 0.91.0) keinen Controller und kein Universe mehr, nur einen internen, projektweiten
Kanal-Pool (1..512, siehe `channel_write.hpp`): Je Frame legt die Session einen `ChannelWrite{poolChannel,
data}` je Node in `PixelFrame::channels`.

`OutputPipeline::applyChannels` (`output_pipeline_channels.cpp`) übersetzt das über `config.dmxPatches`
(`DmxChannelPatch`, Projektdatei-Zeile `dmxpatch`, siehe `docs/value-graph.md`) in echte Ziele, bevor es
einträgt:

- Jeder Write wird gegen alle `dmxPatches`-Einträge geprüft; ein Write kann über mehrere Patches gestreut
  sein, wenn der Nutzer den Pool aufgeteilt hat (jeder überlappende Bereich wird einzeln aufgelöst und
  eingetragen).
- Pool-Kanäle, die kein `dmxpatch` abdeckt, werden stillschweigend nicht gesendet (wie ein ungepatchtes
  Fixture).
- Für jeden aufgelösten Block gilt wie zuvor: Liegt das Ziel-Universe schon im Plan (ein Fixture ist dort
  gepatcht), landen die Bytes in dessen Puffer, außer sie überschneiden sich mit den LEDs des Fixtures —
  dann wird der Eintrag verworfen und als Sendefehler gezählt; die LED-Kanäle werden nie überschrieben.
  Sonst bekommt das Universe einen eigenen `ChannelSlot`, der zwischen den Frames bestehen bleibt (für
  laufende Sequenznummern) und nur gesendet wird, wenn ihn im aktuellen Frame jemand beschrieben hat.
  Schreibt kein Node mehr in ein solches Universe, verstummt es.
- Mehrere Nodes im selben freien Universe dürfen sich ergänzen; beschreiben zwei Nodes denselben Kanal,
  gewinnt der zuletzt verarbeitete.
- `dmxpatch`-Einträge mit DDP-Ziel oder außerhalb des gültigen Bereichs (Universe, Kanal, Pool-Kanal) sowie
  zwei Einträge mit überschneidendem Pool-Bereich lässt `OutputPipeline::create` gar nicht erst zu
  (`ErrorCode::InvalidArgument`/`AlreadyExists`) — anders als früher ist das keine Laufzeitprüfung mehr pro
  Frame, sondern eine einmalige Prüfung beim Erstellen der Ausgabe.
- **Live-Wertanzeige im Node-Editor (0.92.0):** Beide Nodes haben keinen eigenen Ausgang, darum nutzen sie
  (anders als z. B. die Audio-Nodes) nicht den Pegelbalken unter einem Ausgangsport, sondern den
  Anzeigestreifen im Node-Körper selbst (`KernelOutputs::setMonitor`, Descriptor `liveView =
  graph::LiveView::Channels`, siehe `builtin_dmx_out.cpp`): ein Balken je gerade gesendetem Byte (0..255 auf
  0..1 skaliert), mit Kanalnummern statt der sonst üblichen Frequenzachse. Reine Anzeige, ohne Einfluss auf
  die gesendeten Werte.
- **Eigene Tabelle für den Kanal-Pool im Studio (0.92.0):** Die Fixture-Patch-Tabelle (`patch_table.cpp`)
  zeigt weiterhin nur Fixtures — der interne DMX-Kanal-Pool ist nicht an ein Fixture gebunden und bekommt
  eine eigene Tabelle direkt darunter (`dmx_patch_table.hpp/cpp`, in `output_page.cpp` eingebunden). Anders
  als bei Fixtures (eine Zeile je Fixture, immer vorhanden) legt der Nutzer hier beliebig viele, beliebig
  geschnittene Pool-Bereiche per „Patch +"/„Gewählten Patch −" selbst an; jede Änderung schickt die ganze
  Liste über `Player::setDmxPatches` (validiert wie beim Laden über `OutputPipeline::create`, siehe oben).
  Spalten: Pool-Kanal (von), Anzahl, Controller, Protokoll (folgt dem gewählten Controller, nicht editierbar
  — wie bei der Fixture-Patch-Tabelle), Universe, Erster Kanal.

## Bekannte Grenzen

- Nur RGB, kein Weißkanal (RGBW), kein Dithering, keine Kalibrierung je Kanal.
- Der Windows-Zweig von `UdpSender` (Winsock) ist geschrieben und wird von der CI übersetzt und
  mit Loopback getestet, aber nicht mit echten Controllern im Netz.
- Die Gegenrichtung (DMX empfangen) steht in `wh_input` und ist in `docs/value-graph.md`
  (Abschnitt „DMX-Eingang“) und `docs/studio.md` beschrieben.
- Pro Paket entsteht eine kleine Allokation (der Paketpuffer); bei mehreren hundert
  Universen und 60 Bildern pro Sekunde ist das unkritisch, lässt sich aber später
  durch wiederverwendete Puffer ersetzen.
- Überlappende DDP-Ströme desselben Ziels werden nicht geprüft.
- Die Paketformate sind aus der öffentlichen Spezifikation nachgebaut und per
  Byte-Vergleich getestet, aber noch nicht gegen echte Geräte oder Wireshark geprüft.
- Nicht enthalten: Art-Net-Discovery (ArtPoll), sACN-Synchronisation, Serial, MQTT,
  Hyperion, TPM2.net, MIDI, OSC; WLED wird über DDP oder Art-Net angesprochen, eine eigene
  WLED-Erkennung gibt es nicht.
