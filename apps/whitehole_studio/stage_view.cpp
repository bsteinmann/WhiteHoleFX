// stage_view.cpp: Umsetzung zu stage_view.hpp. Die Bühnenansicht im Stage Config.
#include "stage_view.hpp"

#include <QCheckBox>
#include <QColor>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPolygonF>
#include <QPushButton>
#include <QShowEvent>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "form_helpers.hpp"
#include "preview_widget.hpp"
#include "whitehole/output/led_address.hpp"
#include "whitehole/project/project.hpp"

namespace whitehole::studio {

using form::fromStd;

namespace {

preview::Camera3d stageCamera(preview::ViewPreset preset) {
    preview::Camera3d camera = preview::cameraFor(preset);
    camera.offLeds = preview::OffLeds::Gray;  // alle LEDs sind an; Aus-LEDs kommen hier nicht vor
    camera.glow = true;
    camera.showBox = true;
    camera.showAxes = true;
    camera.depthCue = 0.2F;
    return camera;
}

}  // namespace

StageView::StageView(runtime::Player& player, bool compact, QWidget* parent)
    : QWidget{parent}, player_{player}, view_{new PreviewWidget{this}}, camera_{stageCamera(preview::ViewPreset::Perspective)} {
    auto* column = new QVBoxLayout{this};
    column->setContentsMargins(0, 0, 0, 0);
    view_->setView3d(true);
    view_->setCamera(camera_);
    column->addWidget(view_, 1);
    {
        auto* bar = new QHBoxLayout;
        wiringCheck_ = new QCheckBox{tr("Verkabelung")};
        wiringCheck_->setChecked(true);
        wiringCheck_->setToolTip(tr("Kabelweg mit Pfeilen, Start-LED und LED-Nummern ein- und ausblenden"));
        info_ = new QLabel{tr("Klicke eine LED, um ihre Adresse zu sehen.")};
        info_->setTextInteractionFlags(Qt::TextSelectableByMouse);
        info_->setWordWrap(true);
        bar->addWidget(wiringCheck_);
        bar->addWidget(info_, 1);
        column->addLayout(bar);
        connect(wiringCheck_, &QCheckBox::toggled, this, [this](bool on) { setWiringVisible(on); });
        connect(view_, &PreviewWidget::clicked, this, [this](QPointF position) { pickAt(position); });
        view_->setPaintHook([this](QPainter& painter) { paintWiring(painter); });
    }
    if (compact) {
        setMinimumHeight(200);
        setMaximumHeight(280);
        view_->setToolTip(tr("Ziehen dreht, Rad zoomt. Die erste LED am Kabel ist weiß; x rot, y grün, z blau."));
    } else {
        auto* row = new QHBoxLayout;
        const struct {
            const char* text;
            preview::ViewPreset preset;
        } presets[] = {{QT_TR_NOOP("Perspektive"), preview::ViewPreset::Perspective},
                       {QT_TR_NOOP("Front"), preview::ViewPreset::Front},
                       {QT_TR_NOOP("Seite"), preview::ViewPreset::Side},
                       {QT_TR_NOOP("Oben"), preview::ViewPreset::Top}};
        for (const auto& entry : presets) {
            auto* button = new QPushButton{tr(entry.text)};
            const preview::ViewPreset preset = entry.preset;
            connect(button, &QPushButton::clicked, this, [this, preset]() { setPreset(preset); });
            row->addWidget(button);
        }
        row->addStretch(1);
        column->addLayout(row);
    }
    connect(view_, &PreviewWidget::cameraChanged, this, [this](const preview::Camera3d& changed) {
        camera_ = changed;
        view_->setCamera(camera_);
        refresh();
    });
    connect(view_, &PreviewWidget::resized, this, [this](const QSize&) { refresh(); });
}

void StageView::setSelection(std::vector<mapping::FixtureId> ids) {
    if (ids == selection_) {
        return;
    }
    selection_ = std::move(ids);
    refresh();
}

void StageView::setPreset(preview::ViewPreset preset) {
    camera_ = stageCamera(preset);
    view_->setCamera(camera_);
    refresh();
}

void StageView::refresh() {
    if (isVisible()) {
        renderNow();
    }
}

void StageView::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    renderNow();
}

bool StageView::renderNow() {
    if (player_.project() == nullptr) {
        view_->setMessage(tr("Kein Projekt geladen."));
        return false;
    }
    preview::PreviewOptions options;
    options.width = static_cast<std::uint32_t>(std::clamp(view_->width(), 64, 4096));
    options.height = static_cast<std::uint32_t>(std::clamp(view_->height(), 64, 4096));
    imageOptions_ = options;
    const auto image = player_.stageImage3d(camera_, options, selection_);
    if (!image.ok()) {
        view_->setMessage(fromStd(image.error().message));
        return false;
    }
    const preview::Image& img = image.value();
    const QImage wrapped{img.rgba.data(), static_cast<int>(img.width), static_cast<int>(img.height),
                         static_cast<qsizetype>(img.width) * 4, QImage::Format_RGBA8888};
    last_ = wrapped.copy();  // eigener Speicher: das Bild überlebt den Rückgabewert
    wires_ = preview::projectWiring(player_.ledMap(), camera_, options.width, options.height, options.margin, {});
    view_->setMessage(QString{});
    view_->setImage(last_);
    return true;
}

void StageView::setWiringVisible(bool on) {
    showWiring_ = on;
    wiringCheck_->setChecked(on);
    view_->update();
}

QString StageView::infoText() const { return info_->text(); }

bool StageView::pickAt(QPointF position) {
    if (player_.project() == nullptr || last_.isNull()) {
        return false;
    }
    // Das Bild füllt das Widget; bei kurzem Größenunterschied wird auf Bildpunkte umgerechnet.
    const float sx = static_cast<float>(imageOptions_.width) / static_cast<float>(std::max(1, view_->width()));
    const float sy = static_cast<float>(imageOptions_.height) / static_cast<float>(std::max(1, view_->height()));
    picked_ = preview::pickLed(player_.ledMap(), camera_, imageOptions_.width, imageOptions_.height, imageOptions_.margin,
                               static_cast<float>(position.x()) * sx, static_cast<float>(position.y()) * sy, 14.0F, {});
    info_->setText(picked_ ? describeLed(*picked_) : tr("Klicke eine LED, um ihre Adresse zu sehen."));
    view_->update();
    return picked_.has_value();
}

QString StageView::describeLed(const preview::PickedLed& picked) const {
    const mapping::LedMap& map = player_.ledMap();
    const project::Project* proj = player_.project();
    if (proj == nullptr || picked.range >= map.ranges.size()) {
        return {};
    }
    const mapping::FixtureId id = map.ranges[picked.range].fixture;
    QString name;
    for (const mapping::Fixture& fixture : proj->layout.fixtures()) {
        if (fixture.id == id) {
            name = fromStd(fixture.name);
        }
    }
    QString text = tr("%1 · LED %2 von %3").arg(name).arg(picked.indexInFixture + 1).arg(map.ranges[picked.range].count);
    for (const output::FixturePatch& patch : proj->patches) {
        if (patch.fixture != id) {
            continue;
        }
        const auto address = output::ledAddress(patch, picked.indexInFixture);
        if (!address) {
            return text + tr(" · liegt hinter dem letzten Universe");
        }
        if (patch.protocol == output::Protocol::Ddp) {
            return text + tr(" · DDP Ziel %1 · Byte %2–%3").arg(address->destination).arg(address->firstChannel).arg(address->lastChannel);
        }
        const QString protocol = patch.protocol == output::Protocol::Sacn ? QStringLiteral("sACN") : QStringLiteral("Art-Net");
        return text + tr(" · %1 Ziel %2 · Universe %3 · Kanal %4–%5")
                          .arg(protocol).arg(address->destination).arg(address->universe)
                          .arg(address->firstChannel).arg(address->lastChannel);
    }
    return text + tr(" · nicht gepatcht");
}

void StageView::paintWiring(QPainter& painter) const {
    if (!showWiring_ || wires_.empty() || imageOptions_.width == 0 || imageOptions_.height == 0) {
        return;
    }
    const double sx = static_cast<double>(view_->width()) / imageOptions_.width;
    const double sy = static_cast<double>(view_->height()) / imageOptions_.height;
    const mapping::LedMap& map = player_.ledMap();
    painter.setRenderHint(QPainter::Antialiasing, true);
    for (const preview::WirePath& path : wires_) {
        const bool strong = selection_.empty() || (path.range < map.ranges.size() &&
            std::find(selection_.begin(), selection_.end(), map.ranges[path.range].fixture) != selection_.end());
        const int alpha = strong ? 230 : 70;
        QPolygonF line;
        for (const preview::WireDot& dot : path.dots) {
            line << QPointF{dot.x * sx, dot.y * sy};
        }
        if (line.size() < 2) {
            continue;
        }
        painter.setPen(QPen{QColor{255, 255, 255, alpha}, strong ? 1.6 : 1.0});
        painter.setBrush(Qt::NoBrush);
        painter.drawPolyline(line);
        // Pfeile: etwa acht Stück entlang des Kabels, Spitze in Laufrichtung.
        const int step = std::max(1, static_cast<int>(line.size()) / 8);
        painter.setBrush(QColor{255, 255, 255, alpha});
        painter.setPen(Qt::NoPen);
        for (int i = step; i < line.size(); i += step) {
            const QPointF a = line[i - 1];
            const QPointF b = line[i];
            const double len = std::hypot(b.x() - a.x(), b.y() - a.y());
            if (len < 1.0) {
                continue;
            }
            const QPointF dir{(b.x() - a.x()) / len, (b.y() - a.y()) / len};
            const QPointF side{-dir.y(), dir.x()};
            QPolygonF head;
            head << b << QPointF{b.x() - dir.x() * 8 + side.x() * 4, b.y() - dir.y() * 8 + side.y() * 4}
                 << QPointF{b.x() - dir.x() * 8 - side.x() * 4, b.y() - dir.y() * 8 - side.y() * 4};
            painter.drawPolygon(head);
        }
        if (!strong) {
            continue;
        }
        // Nummern, wenn die LEDs weit genug auseinander liegen.
        const double spacing = std::hypot(line[1].x() - line[0].x(), line[1].y() - line[0].y());
        if (line.size() <= 400 && spacing >= 16.0) {
            painter.setPen(QColor{255, 220, 120});
            for (int i = 0; i < line.size(); ++i) {
                painter.drawText(QPointF{line[i].x() + 3, line[i].y() - 3}, QString::number(i + 1));
            }
        }
        painter.setPen(QPen{QColor{60, 255, 120}, 2.0});
        painter.setBrush(Qt::NoBrush);
        painter.drawEllipse(line[0], 7.0, 7.0);
        painter.drawText(QPointF{line[0].x() + 9, line[0].y() + 4}, tr("Start"));
    }
    if (picked_) {
        for (const preview::WirePath& path : wires_) {
            for (const preview::WireDot& dot : path.dots) {
                if (dot.led == picked_->led) {
                    painter.setPen(QPen{QColor{255, 220, 0}, 2.0});
                    painter.setBrush(Qt::NoBrush);
                    painter.drawEllipse(QPointF{dot.x * sx, dot.y * sy}, 9.0, 9.0);
                }
            }
        }
    }
}

QString StageView::exercise() {
    if (player_.project() == nullptr || player_.project()->layout.fixtures().empty()) {
        return QStringLiteral("Bühnenansicht: kein Layout geladen");
    }
    resize(480, 320);
    if (!renderNow() || last_.isNull()) {
        return QStringLiteral("Bühnenansicht: es entsteht kein Bild");
    }
    const QImage first = last_;
    view_->orbitBy(60.0, 0.0);
    if (!renderNow() || last_ == first) {
        return QStringLiteral("Bühnenansicht: Drehen ändert das Bild nicht");
    }
    // Die Auswahl hebt Fixtures hervor: das Bild ändert sich, sobald nur eines gewählt ist und es mehrere gibt.
    const auto& fixtures = player_.project()->layout.fixtures();
    if (fixtures.size() > 1) {
        const QImage all = last_;
        setSelection({fixtures.front().id});
        if (!renderNow() || last_ == all) {
            return QStringLiteral("Bühnenansicht: die Auswahl wird nicht hervorgehoben");
        }
        setSelection({});
    }
    // Kabelweg und Klick: Die Wege liegen vor; ein Klick auf die erste LED des ersten Fixtures liefert dessen Adresse.
    if (wires_.empty() || wires_.front().dots.empty()) {
        return QStringLiteral("Bühnenansicht: kein Kabelweg berechnet");
    }
    const preview::WireDot first = wires_.front().dots.front();
    const double sx = static_cast<double>(view_->width()) / imageOptions_.width;
    const double sy = static_cast<double>(view_->height()) / imageOptions_.height;
    if (!pickAt(QPointF{first.x * sx, first.y * sy}) || !picked_ || infoText().isEmpty() || !infoText().contains(QStringLiteral("LED"))) {
        return QStringLiteral("Bühnenansicht: der Klick auf eine LED zeigt keine Adresse");
    }
    setWiringVisible(false);
    if (showWiring_ || wiringCheck_->isChecked()) {
        return QStringLiteral("Bühnenansicht: die Verkabelung lässt sich nicht ausblenden");
    }
    setWiringVisible(true);
    setPreset(preview::ViewPreset::Top);
    if (camera_.pitchDegrees != 90.0F || !camera_.showAxes || !renderNow()) {
        return QStringLiteral("Bühnenansicht: die Draufsicht lässt sich nicht einstellen");
    }
    return {};
}

}  // namespace whitehole::studio
