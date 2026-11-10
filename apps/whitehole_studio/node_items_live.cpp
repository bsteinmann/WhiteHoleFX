// node_items_live.cpp: Live-Anzeige in Nodes und Kabeln (Pegelbalken, Spektrum, Farbstreifen, leuchtende Kabel).
//
// Warum es das gibt: Wer einen Regler oder Audio-Eingang einstellt, sieht sonst nicht, was am Ausgang ankommt.
// Die Balken sitzen unter dem Namen des Ausgangs (kein Platz im Layout nötig), das Kabel wird heller und dicker,
// je höher der Pegel ist. Ausgänge ohne Live-Wert (Bildknoten auf der GPU) bleiben unverändert.
// Die Daten kommen vom Editor (node_editor_live.cpp); hier wird nur gespeichert und gezeichnet.
#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <utility>

#include <QPainter>
#include <QPainterPath>

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
    if (monitor_.active) {
        monitor_ = MonitorDisplay{};
        changed = true;
    }
    if (changed) {
        update();
    }
}

void NodeItem::clearMonitor() {
    if (monitor_.active) {
        monitor_ = MonitorDisplay{};
        update();
    }
}

void NodeItem::setMonitor(MonitorDisplay display) {
    if (!hasMonitor()) {
        return;
    }
    display.active = true;
    if (monitor_.active) {
        for (std::size_t i = 0; i < display.bars.size(); ++i) {
            display.bars[i] = display.bipolar ? display.bars[i] : held(display.bars[i], monitor_.bars[i]);
        }
    }
    bool differ = !monitor_.active || monitor_.first != display.first || monitor_.last != display.last ||
                  monitor_.bipolar != display.bipolar;
    for (std::size_t i = 0; i < display.bars.size() && !differ; ++i) {
        differ = std::abs(display.bars[i] - monitor_.bars[i]) > kMinStep;
    }
    if (differ) {
        monitor_ = display;
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
        case LiveDisplay::Kind::Bars: break;  // Spektrum und Welle haben ihren Streifen im Node (drawMonitor)
        case LiveDisplay::Kind::Color: {
            painter->setBrush(live.color);
            painter->drawRoundedRect(QRectF{left, base - 0.5, width, 4.0}, 2.0, 2.0);
            break;
        }
        case LiveDisplay::Kind::None: break;
    }
    painter->restore();
}

namespace {

// Wo die Beschriftungen der Frequenzachse stehen: Band i liegt logarithmisch zwischen 40 Hz und 16 kHz.
double bandPosition(double hz) { return 32.0 * std::log(hz / 40.0) / std::log(16000.0 / 40.0); }

}  // namespace

void NodeItem::drawMonitor(QPainter* painter) const {
    const QRectF strip{10.0, monitorTop_, kNodeWidth - 20.0, kMonitorHeight};
    const bool wave = view_ == graph::LiveView::Wave;
    const double labelRow = wave ? 0.0 : 10.0;
    const QRectF plot{strip.left(), strip.top(), strip.width(), strip.height() - labelRow};
    const QColor accent = portColor(wave ? graph::PortType::AudioWave : graph::PortType::AudioSpectrum);
    painter->save();
    painter->setPen(Qt::NoPen);
    painter->setBrush(QColor{26, 28, 34});
    painter->drawRoundedRect(plot, 3.0, 3.0);

    const double step = plot.width() / 32.0;
    const bool ranged = monitor_.first >= 0 && monitor_.last >= monitor_.first;
    if (ranged) {
        // Der ausgewertete Bereich: getönter Hintergrund über die ganze Höhe, auch bei Stille zu sehen.
        QColor tint = accent;
        tint.setAlpha(55);
        painter->setBrush(tint);
        painter->drawRect(QRectF{plot.left() + monitor_.first * step, plot.top(),
                                 (monitor_.last - monitor_.first + 1) * step, plot.height()});
    }
    if (monitor_.active && !wave) {
        const QColor dim{92, 100, 118};
        QColor bright = accent;
        bright = mixed(bright, QColor{255, 255, 255}, 0.2);
        for (std::size_t i = 0; i < monitor_.bars.size(); ++i) {
            const double value = std::clamp(static_cast<double>(monitor_.bars[i]), 0.0, 1.0);
            if (value <= 0.01) {
                continue;
            }
            const bool inside = !ranged || (static_cast<int>(i) >= monitor_.first && static_cast<int>(i) <= monitor_.last);
            painter->setBrush(inside ? bright : dim);
            const double h = (plot.height() - 2.0) * value;
            painter->drawRect(QRectF{plot.left() + static_cast<double>(i) * step + 0.5, plot.bottom() - 1.0 - h,
                                     std::max(1.0, step - 1.0), h});
        }
    }
    if (wave) {
        painter->setPen(QPen{QColor{70, 74, 84}, 1.0});
        painter->drawLine(QPointF{plot.left(), plot.center().y()}, QPointF{plot.right(), plot.center().y()});
        if (monitor_.active) {
            QPainterPath curve;
            for (std::size_t i = 0; i < monitor_.bars.size(); ++i) {
                const double x = plot.left() + plot.width() * static_cast<double>(i) / 31.0;
                const double v = std::clamp(static_cast<double>(monitor_.bars[i]), -1.0, 1.0);
                const QPointF point{x, plot.center().y() - v * (plot.height() / 2.0 - 2.0)};
                if (i == 0) {
                    curve.moveTo(point);
                } else {
                    curve.lineTo(point);
                }
            }
            painter->setPen(QPen{mixed(accent, QColor{255, 255, 255}, 0.2), 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin});
            painter->setBrush(Qt::NoBrush);
            painter->drawPath(curve);
        }
    } else {
        // Frequenzachse: drei Marken, damit man den Bereich zuordnen kann.
        QFont font = painter->font();
        font.setPixelSize(8);
        painter->setFont(font);
        painter->setPen(QColor{120, 126, 140});
        for (const auto& [hz, text] : {std::pair{100.0, "100"}, std::pair{1000.0, "1k"}, std::pair{10000.0, "10k"}}) {
            const double x = plot.left() + bandPosition(hz) * step;
            painter->drawLine(QPointF{x, plot.bottom()}, QPointF{x, plot.bottom() + 2.0});
            painter->drawText(QRectF{x - 14.0, plot.bottom() + 1.0, 28.0, labelRow - 1.0}, Qt::AlignHCenter | Qt::AlignVCenter,
                              QString::fromLatin1(text));
        }
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
