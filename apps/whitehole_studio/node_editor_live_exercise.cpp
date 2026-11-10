// node_editor_live_exercise.cpp: Rauchtest der Live-Anzeige für die CI (--exercise).
//
// Setzt Werte von Hand an einen Ausgang (ohne laufende Wiedergabe), prüft, dass das Kabel folgt, die Spitze
// stehen bleibt, alle drei Zeichenarten malen und das Aufräumen alles zurücknimmt; danach der Anzeigestreifen
// der Audio-Nodes (exerciseMonitor).
#include <QGraphicsScene>
#include <QImage>
#include <QPainter>

#include <cmath>

#include "node_editor.hpp"

namespace whitehole::studio {

QString NodeEditor::exerciseLive() {
    WireItem* wire = wires_.empty() ? nullptr : wires_.front();
    if (wire == nullptr) {
        return QStringLiteral("Live-Anzeige: kein Kabel zum Prüfen");
    }
    NodeItem* source = nullptr;
    for (const auto& entry : nodes_) {
        if (entry.second == wire->fromNode()) {
            source = entry.second;
        }
    }
    if (source == nullptr) {
        return QStringLiteral("Live-Anzeige: Ursprung des Kabels nicht gefunden");
    }
    const std::string key = wire->fromKey();
    const auto render = [this]() {
        QImage image{480, 320, QImage::Format_ARGB32};
        image.fill(Qt::black);
        QPainter painter{&image};
        scene_.render(&painter, QRectF{0, 0, 480, 320}, scene_.itemsBoundingRect());
        return image;
    };
    clearLive();
    const QImage plain = render();
    if (wire->liveLevel() >= 0.0F || source->liveLevel(key) >= 0.0F) {
        return QStringLiteral("Live-Anzeige: ohne Werte darf nichts leuchten");
    }

    LiveDisplay level;
    level.kind = LiveDisplay::Kind::Level;
    level.level = 0.8F;
    source->setLive(key, level);
    wire->syncLive();
    if (std::abs(wire->liveLevel() - 0.8F) > 0.01F) {
        return QStringLiteral("Live-Anzeige: Kabel folgt dem Pegel nicht (%1)").arg(static_cast<double>(wire->liveLevel()));
    }
    const QImage withBar = render();
    if (withBar == plain) {
        return QStringLiteral("Live-Anzeige: Balken und Kabel ändern das Bild nicht");
    }
    // Spitze bleibt kurz stehen: nach Pegel 0 liegt der Wert noch zwischen 0 und 0,8.
    level.level = 0.0F;
    source->setLive(key, level);
    wire->syncLive();
    if (wire->liveLevel() < 0.5F || wire->liveLevel() > 0.79F) {
        return QStringLiteral("Live-Anzeige: Spitze fällt falsch ab (%1)").arg(static_cast<double>(wire->liveLevel()));
    }

    LiveDisplay bars;
    bars.kind = LiveDisplay::Kind::Bars;
    for (std::size_t i = 0; i < bars.bars.size(); ++i) {
        bars.bars[i] = static_cast<float>(i) / static_cast<float>(bars.bars.size());
    }
    bars.level = 0.97F;
    source->setLive(key, bars);
    wire->syncLive();
    if (wire->liveLevel() < 0.9F) {
        return QStringLiteral("Live-Anzeige: Spektrum färbt das Kabel nicht (%1)").arg(static_cast<double>(wire->liveLevel()));
    }
    LiveDisplay color;
    color.kind = LiveDisplay::Kind::Color;
    color.color = QColor{255, 80, 20};
    color.level = 1.0F;
    source->setLive(key, color);
    wire->syncLive();
    if (render() == plain) {
        return QStringLiteral("Live-Anzeige: Farbe malt nichts");
    }

    refreshLive();  // ohne laufende Wiedergabe (oder mit echten Werten): darf nicht abstürzen
    liveShown_ = true;
    clearLive();
    if (wire->liveLevel() >= 0.0F || source->liveLevel(key) >= 0.0F) {
        return QStringLiteral("Live-Anzeige: Aufräumen lässt Reste stehen");
    }
    if (render() != plain) {
        return QStringLiteral("Live-Anzeige: nach dem Aufräumen sieht das Bild anders aus");
    }
    return exerciseMonitor();
}

/// Anzeigestreifen: der Node wird höher, der Streifen malt, der markierte Bereich ist sichtbar und Aufräumen nimmt alles zurück.
QString NodeEditor::exerciseMonitor() {
    graph::NodeDescriptor plainType;
    plainType.typeId = "test.plain";
    graph::NodeDescriptor stripType = plainType;
    stripType.typeId = "test.strip";
    stripType.liveView = graph::LiveView::Spectrum;
    if (NodeItem::sizeFor(stripType).height() <= NodeItem::sizeFor(plainType).height()) {
        return QStringLiteral("Anzeigestreifen: der Node wird nicht höher");
    }
    const auto render = [](NodeItem& item) {
        QImage image{200, 140, QImage::Format_ARGB32};
        image.fill(Qt::black);
        QPainter painter{&image};
        QGraphicsScene scene;
        scene.addItem(&item);
        scene.render(&painter, QRectF{0, 0, 200, 140}, item.sceneBoundingRect());
        scene.removeItem(&item);
        return image;
    };
    NodeItem item{NodeId{1}, stripType, QStringLiteral("Test")};
    if (!item.hasMonitor()) {
        return QStringLiteral("Anzeigestreifen: der Node kennt seinen Streifen nicht");
    }
    const QImage empty = render(item);
    MonitorDisplay display;
    display.active = true;
    for (std::size_t i = 0; i < display.bars.size(); ++i) {
        display.bars[i] = static_cast<float>(i % 8) / 8.0F;
    }
    item.setMonitor(display);
    const QImage bars = render(item);
    if (bars == empty) {
        return QStringLiteral("Anzeigestreifen: Balken malen nichts");
    }
    display.first = 8;
    display.last = 14;
    item.setMonitor(display);
    if (render(item) == bars) {
        return QStringLiteral("Anzeigestreifen: der markierte Bereich ist nicht zu sehen");
    }
    item.clearLive();
    if (item.monitor().active || render(item) != empty) {
        return QStringLiteral("Anzeigestreifen: Aufräumen lässt Reste stehen");
    }
    stripType.liveView = graph::LiveView::Wave;
    NodeItem wave{NodeId{2}, stripType, QStringLiteral("Welle")};
    const QImage still = render(wave);
    display.first = -1;
    display.last = -1;
    display.bipolar = true;
    for (std::size_t i = 0; i < display.bars.size(); ++i) {
        display.bars[i] = std::sin(static_cast<float>(i) * 0.5F);
    }
    wave.setMonitor(display);
    if (render(wave) == still) {
        return QStringLiteral("Anzeigestreifen: die Wellenform malt nichts");
    }
    return {};
}

}  // namespace whitehole::studio
