// stage_view.cpp: Umsetzung zu stage_view.hpp. Die Bühnenansicht im Stage Config.
#include "stage_view.hpp"

#include <QHBoxLayout>
#include <QPushButton>
#include <QShowEvent>
#include <QVBoxLayout>

#include <algorithm>
#include <cstdint>

#include "form_helpers.hpp"
#include "preview_widget.hpp"

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
    if (compact) {
        setMinimumHeight(170);
        setMaximumHeight(220);
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
    const auto image = player_.stageImage3d(camera_, options, selection_);
    if (!image.ok()) {
        view_->setMessage(fromStd(image.error().message));
        return false;
    }
    const preview::Image& img = image.value();
    const QImage wrapped{img.rgba.data(), static_cast<int>(img.width), static_cast<int>(img.height),
                         static_cast<qsizetype>(img.width) * 4, QImage::Format_RGBA8888};
    last_ = wrapped.copy();  // eigener Speicher: das Bild überlebt den Rückgabewert
    view_->setMessage(QString{});
    view_->setImage(last_);
    return true;
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
    setPreset(preview::ViewPreset::Top);
    if (camera_.pitchDegrees != 90.0F || !camera_.showAxes || !renderNow()) {
        return QStringLiteral("Bühnenansicht: die Draufsicht lässt sich nicht einstellen");
    }
    return {};
}

}  // namespace whitehole::studio
