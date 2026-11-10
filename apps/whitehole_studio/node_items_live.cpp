// node_items_live.cpp: Live-Anzeige in Nodes und Kabeln (Pegelbalken, Spektrum, Farbstreifen, leuchtende Kabel).
//
// Warum es das gibt: Wer einen Regler oder Audio-Eingang einstellt, sieht sonst nicht, was am Ausgang ankommt.
// Die Balken sitzen unter dem Namen des Ausgangs (kein Platz im Layout nötig), das Kabel wird heller und dicker,
// je höher der Pegel ist. Ausgänge ohne Live-Wert (Bildknoten auf der GPU) bleiben unverändert.
// Die Daten kommen vom Editor (node_editor_live.cpp); hier wird nur gespeichert und gezeichnet.
#include <algorithm>
#include <cmath>

#include "node_items.hpp"

namespace whitehole::studio {

namespace {

// Wie schnell eine Spitze abfällt (Faktor je Aktualisierung; bei 60 Frames/s etwa 0,1 s bis zur Hälfte).
constexpr float kRelease = 0.88F;
constexpr float kMinStep = 0.004F;  // kleinere Änderungen zeichnen nicht neu

float held(float now, float before) { return std::max(now, before * kRelease); }

QColor mixed(const QColor& a, const QColor& b, double t) {
    return QColor::fromRgbF(a.redF() + (b.redF() - a.redF()) * t, a.greenF() + (b.greenF() - a.greenF()) * t,
                            a.blueF() + (b.blueF() - a.blueF()) * t, a.alphaF());
}

bool differs(const LiveDisplay& a, const LiveDisplay& b) {
    if (a.kind != b.kind || a.color != b.color || std::abs(a.level - b.level) > kMinStep) {
        return true;
    }
    for (std::size_t i = 0; i < a.bars.size(); ++i) {
        if (std::abs(a.bars[i] - b.bars[i]) > kMinStep) {
            return true;
        }
    }
    return false;
}

}  // namespace

void NodeItem::setLive(const std::string& portKey, LiveDisplay display) {
    for (PortSlot& port : ports_) {
        if (port.key != portKey) {
            continue;
        }
        const LiveDisplay& before = port.live;
        if (before.kind == display.kind) {
            display.level = held(display.level, before.level);
            for (std::size_t i = 0; i < display.bars.size(); ++i) {
                display.bars[i] = held(display.bars[i], before.bars[i]);
            }
        }
        if (differs(before, display)) {
            port.live = display;
            update();
        }
        return;
    }
}

void NodeItem::clearLive() {
    bool changed = false;
    for (PortSlot& port : ports_) {
        if (port.live.kind != LiveDisplay::Kind::None) {
            port.live = LiveDisplay{};
            changed = true;
        }
    }
    if (changed) {
        update();
    }
}

float NodeItem::liveLevel(const std::string& portKey) const {
    const PortSlot* port = findPort(portKey);
    return port != nullptr && port->live.kind != LiveDisplay::Kind::None ? port->live.level : -1.0F;
}

void drawPortLive(QPainter* painter, const PortSlot& port, double width) {
    const LiveDisplay& live = port.live;
    if (!port.output || live.kind == LiveDisplay::Kind::None) {
        return;
    }
    const double right = port.anchor.x() - 10.0;
    const double left = right - width;
    const double base = port.anchor.y() + 8.5;  // unter dem Namen des Ausgangs
    const QColor accent = portColor(port.type);
    painter->save();
    painter->setPen(Qt::NoPen);
    switch (live.kind) {
        case LiveDisplay::Kind::Level: {
            painter->setBrush(QColor{28, 30, 36});
            painter->drawRoundedRect(QRectF{left, base, width, 3.0}, 1.5, 1.5);
            const double fill = width * std::clamp(static_cast<double>(live.level), 0.0, 1.0);
            if (fill > 0.5) {
                painter->setBrush(mixed(accent, QColor{255, 255, 255}, 0.25 * live.level));
                painter->drawRoundedRect(QRectF{left, base, fill, 3.0}, 1.5, 1.5);
            }
            break;
        }
        case LiveDisplay::Kind::Bars: {
            // Hinter dem Namen: 32 Balken, von unten wachsend (Wellen: Betrag).
            const double step = width / static_cast<double>(live.bars.size());
            const double top = port.anchor.y() - 9.0;
            const double full = 18.0;
            QColor bar = accent;
            bar.setAlpha(120);
            painter->setBrush(bar);
            for (std::size_t i = 0; i < live.bars.size(); ++i) {
                const double value = std::clamp(static_cast<double>(std::abs(live.bars[i])), 0.0, 1.0);
                if (value > 0.02) {
                    painter->drawRect(QRectF{left + static_cast<double>(i) * step, top + full * (1.0 - value),
                                             std::max(1.0, step - 0.6), full * value});
                }
            }
            break;
        }
        case LiveDisplay::Kind::Color: {
            painter->setBrush(live.color);
            painter->drawRoundedRect(QRectF{left, base - 0.5, width, 4.0}, 2.0, 2.0);
            break;
        }
        case LiveDisplay::Kind::None: break;
    }
    painter->restore();
}

void WireItem::syncLive() {
    const float level = from_->liveLevel(fromKey_);
    if (std::abs(level - live_) > kMinStep || (level < 0.0F) != (live_ < 0.0F)) {
        live_ = level;
        update();
    }
}

void WireItem::paintLive(QPainter* painter) const {
    // Ohne Pegel dunkler und dünn, mit Pegel heller, dicker und mit Schein: der Fluss ist am Kabel zu sehen.
    const double level = std::clamp(static_cast<double>(live_), 0.0, 1.0);
    if (level > 0.05) {
        QColor glow = color_;
        glow.setAlphaF(0.35 * level);
        painter->setPen(QPen{glow, 3.0 + 7.0 * level, Qt::SolidLine, Qt::RoundCap});
        painter->drawPath(path());
    }
    QColor line = mixed(color_, QColor{255, 255, 255}, 0.55 * level);
    line.setAlphaF(0.40 + 0.60 * level);
    painter->setPen(QPen{line, 2.0 + 1.8 * level, Qt::SolidLine, Qt::RoundCap});
    painter->drawPath(path());
}

}  // namespace whitehole::studio
