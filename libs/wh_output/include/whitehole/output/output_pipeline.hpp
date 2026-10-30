// output_pipeline.hpp: vom Farbpuffer eines Frames bis zum letzten gesendeten Paket.
//
// Warum es diese Datei gibt: Die Bausteine (ColorEncoder, OutputPlan, Encoder) kennen
// einander nicht. Die Pipeline setzt sie pro Frame zusammen und verwaltet, was dabei
// Zustand hat: Sequenznummern je Universe, Zieladressen und Zähler. Sie ist absichtlich
// synchron und ohne Thread, damit sie vollständig testbar ist; den Thread liefert
// output_worker.hpp.
//
// Aufgabe pro Frame (process):
//   1. Farben prüfen (Größe), in ColorRgba lesen,
//   2. OutputPlan::render füllt die Slot-Puffer,
//   3. je Slot ein Paket (Art-Net, sACN) oder ein Paketstrom (DDP) bauen,
//   4. jedes Paket an den IPacketSink schicken.
//
// Ziele: `destinations[patch.destination]` liefert Adresse und Port. Ein Port 0 bedeutet
// den Standardport des Protokolls. Bei sACN bedeutet eine leere Adresse Multicast an
// 239.255.<Universe>; Art-Net und DDP brauchen eine Adresse (auch eine Broadcast-Adresse
// ist möglich). Adressen müssen IPv4-Zahlen sein; Rechnernamen löst die Pipeline bewusst
// nicht auf, weil eine DNS-Anfrage mitten im Frame blockieren würde.
//
// Fehlerverhalten: Ein Paket, das sich nicht senden lässt, wird gezählt (sendErrors) und
// gemerkt (lastSendError); alle anderen Pakete gehen trotzdem raus. Ein ausgefallener
// Controller darf die übrigen nicht stoppen.
//
// Sequenznummern: Art-Net 1..255 (0 wird übersprungen, es hieße "aus"), sACN 0..255,
// DDP 1..15; jeweils getrennt je Slot.
//
// Threads: process() wird von genau einem Thread aufgerufen. stats() und lastSendError()
// dürfen von anderen Threads gelesen werden.
#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "whitehole/core/result.hpp"
#include "whitehole/core/types.hpp"
#include "whitehole/output/output_plan.hpp"
#include "whitehole/output/packet_sink.hpp"

namespace whitehole::output {

/// Bytes je LED im Ausgabepuffer der Abtast-Stage (lineares RGBA, vier f32).
inline constexpr std::size_t kInputBytesPerLed = sizeof(ColorRgba);

struct Destination {
    std::string address;       // IPv4-Zahl; bei sACN leer = Multicast
    std::uint16_t port{0};     // 0 = Standardport des Protokolls
};

struct OutputConfig {
    std::vector<Destination> destinations;
    std::array<std::uint8_t, 16> cid{};     // sACN: eindeutige Sender-ID
    std::string sourceName{"WhiteHoleFX"};  // sACN
    std::uint8_t sacnPriority{100};
};

struct OutputStats {
    std::uint64_t framesProcessed{0};
    std::uint64_t packetsSent{0};
    std::uint64_t sendErrors{0};
};

class OutputPipeline {
public:
    /// Fehler: InvalidArgument (Ziel-Nummer unbekannt, Adresse fehlt oder ist keine
    /// IPv4-Zahl, sACN-Priorität über 200). `sink` muss die Pipeline überleben.
    [[nodiscard]] static Result<std::unique_ptr<OutputPipeline>> create(OutputPlan plan,
                                                                        OutputConfig config,
                                                                        IPacketSink& sink);

    /// Verarbeitet einen Frame aus Rohbytes der Abtast-Stage (16 Byte je LED). Mehr Bytes
    /// als nötig sind erlaubt (Puffer mit Reserve), weniger sind ein Fehler.
    [[nodiscard]] Result<void> processBytes(std::span<const std::byte> bytes);

    /// Verarbeitet einen Frame aus fertigen Farben (genau ledCount Einträge).
    [[nodiscard]] Result<void> process(std::span<const ColorRgba> colors);

    [[nodiscard]] OutputStats stats() const noexcept;
    [[nodiscard]] std::optional<Error> lastSendError() const;
    [[nodiscard]] const OutputPlan& plan() const noexcept { return plan_; }

private:
    OutputPipeline(OutputPlan plan, OutputConfig config, IPacketSink& sink,
                   std::vector<Endpoint> endpoints);

    void sendSlot(std::size_t index);
    void deliver(const Endpoint& to, std::span<const std::uint8_t> packet);
    void noteError(const Error& error);
    std::uint8_t nextSequence(std::size_t slot);

    OutputPlan plan_;
    OutputConfig config_;
    IPacketSink& sink_;
    std::vector<Endpoint> endpoints_;      // je Slot, zur Erstellung aufgelöst
    std::vector<std::uint8_t> sequences_;  // je Slot
    std::vector<ColorRgba> colors_;        // wiederverwendet
    std::vector<std::vector<std::uint8_t>> slotData_;

    std::atomic<std::uint64_t> frames_{0};
    std::atomic<std::uint64_t> packets_{0};
    std::atomic<std::uint64_t> errors_{0};
    mutable std::mutex errorMutex_;
    std::optional<Error> lastError_;
};

/// Prüft, ob der Text eine IPv4-Adresse in Punktschreibweise ist ("a.b.c.d", je 0..255).
[[nodiscard]] bool isIpv4(const std::string& text) noexcept;

}  // namespace whitehole::output
