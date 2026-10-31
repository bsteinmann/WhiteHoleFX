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
OutputPipeline::processBytes
   OutputPlan::render      ->  ColorEncoder (Alpha, Helligkeit, Gamma, Reihenfolge)
   encodeArtDmx / encodeSacn / encodeDdpStream
        v
IPacketSink::send  (UdpSender im Betrieb, RecordingSink im Test)
```

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
| `output_worker` | Thread, der Frames aus dem Briefkasten holt und die Pipeline ausführt |
| `udp_sender` | nicht blockierender IPv4-UDP-Socket (POSIX) |

## Patchen

Ein `FixturePatch` sagt, wohin ein Fixture geht: Protokoll, Ziel-Nummer, Universe,
Startkanal (1..510), Farbeinstellungen. Regeln:

- Eine LED belegt 3 Kanäle und wird nie auf zwei Universen geteilt: höchstens 170 LEDs
  je Universe. Der Rest läuft ab Kanal 1 im nächsten Universe weiter.
- Mehrere Fixtures dürfen sich ein Universe teilen, solange ihre Kanäle frei sind.
- DDP kennt keine Universen: ein Fixture ist ein Strom ab `ddpOffset`.
- Fixtures ohne Patch werden nicht ausgegeben.
- Alle Fehler (unbekanntes Fixture, doppelter Patch, Überlappung, Universe außerhalb
  des Bereichs) fallen beim Erstellen des Plans auf, nicht im laufenden Betrieb.

## Ziele

`OutputConfig::destinations[n]` gibt Adresse und Port für Ziel `n` an (Port 0 = Standard
des Protokolls). Adressen müssen IPv4-Zahlen sein; Rechnernamen werden bewusst nicht
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

## Bekannte Grenzen

- Nur RGB, kein Weißkanal (RGBW), kein Dithering, keine Kalibrierung je Kanal.
- UDP-Senden gibt es nur für Linux und macOS; unter Windows meldet `UdpSender::create()`
  `Unsupported`.
- Pro Paket entsteht eine kleine Allokation (der Paketpuffer); bei mehreren hundert
  Universen und 60 Bildern pro Sekunde ist das unkritisch, lässt sich aber später
  durch wiederverwendete Puffer ersetzen.
- Überlappende DDP-Ströme desselben Ziels werden nicht geprüft.
- Die Paketformate sind aus der öffentlichen Spezifikation nachgebaut und per
  Byte-Vergleich getestet, aber noch nicht gegen echte Geräte oder Wireshark geprüft.
- Nicht enthalten: Art-Net-Discovery (ArtPoll), sACN-Synchronisation, Serial, MQTT,
  Hyperion, MIDI; WLED wird über DDP oder Art-Net angesprochen, eine eigene
  WLED-Erkennung gibt es nicht.
