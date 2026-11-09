// shape_preview.cpp: Umsetzung zu shape_preview.hpp.
#include "shape_preview.hpp"

#include <QComboBox>
#include <QHBoxLayout>
#include <QPushButton>
#include <QShowEvent>
#include <QVBoxLayout>

#include <algorithm>
#include <cstdint>

#include "form_helpers.hpp"
#include "preview_widget.hpp"
#include "whitehole/mapping/layout.hpp"

namespace whitehole::studio {

namespace {

preview::Camera3d shapeCamera() {
    preview::Camera3d camera = preview::cameraFor(preview::ViewPreset::Perspective);
    camera.offLeds = preview::OffLeds::Gray;
    camera.glow = true;
    camera.showBox = true;
    camera.showAxes = true;
    camera.depthCue = 0.2F;
    return camera;
}

}  // namespace

ShapePreview::ShapePreview(QWidget* parent)
    : QWidget{parent}, view_{new PreviewWidget{this}}, pattern_{new QComboBox{this}},
      testButton_{new QPushButton{tr("Test"), this}}, camera_{shapeCamera()} {
    auto* column = new QVBoxLayout{this};
    column->setContentsMargins(0, 0, 0, 0);
    view_->setView3d(true);
    view_->setCamera(camera_);
    view_->setToolTip(tr("Ziehen dreht, Rad zoomt. Die erste LED am Kabel ist weiß; x rot, y grün, z blau."));
    column->addWidget(view_, 1);
    auto* row = new QHBoxLayout;
    pattern_->addItems({tr("Lauflicht (Kabelreihenfolge)"), tr("Balken entlang x"), tr("Balken entlang y"),
                        tr("Balken entlang z"), tr("Farbfolge R G B W")});
    testButton_->setCheckable(true);
    testButton_->setToolTip(tr("Lässt ein Testmuster über diese Form laufen (nur hier, nichts wird gesendet)"));
    row->addWidget(testButton_);
    row->addWidget(pattern_, 1);
    column->addLayout(row);
    timer_.setInterval(33);
    connect(&timer_, &QTimer::timeout, this, [this]() { tick(); });
    connect(testButton_, &QPushButton::toggled, this, [this](bool on) { setTesting(on); });
    connect(pattern_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) { renderNow(); });
    connect(view_, &PreviewWidget::cameraChanged, this, [this](const preview::Camera3d& changed) {
        camera_ = changed;
        view_->setCamera(camera_);
        renderNow();
    });
    connect(view_, &PreviewWidget::resized, this, [this](const QSize&) { renderNow(); });
}

void ShapePreview::setShape(const mapping::FixtureShape& shape) {
    map_.reset();
    mapping::Layout layout;
    const auto id = layout.add("Form", Vec3{}, shape);
    if (id.ok()) {
        auto baked = layout.bake();
        if (baked.ok()) {
            map_ = std::move(baked.value());
        }
    }
    if (!map_) {
        view_->setMessage(tr("Diese Form ist ungültig."));
        last_ = QImage{};
        return;
    }
    renderNow();
}

void ShapePreview::setTesting(bool on) {
    testing_ = on;
    if (testButton_->isChecked() != on) {
        testButton_->setChecked(on);
        return;
    }
    if (on) {
        clock_.start();
        if (isVisible()) {
            timer_.start();
        }
    } else {
        timer_.stop();
    }
    renderNow();
}

void ShapePreview::setPatternIndex(int index) { pattern_->setCurrentIndex(std::clamp(index, 0, pattern_->count() - 1)); }
int ShapePreview::patternIndex() const { return pattern_->currentIndex(); }

void ShapePreview::tick() { renderNow(); }

runtime::TestPattern ShapePreview::currentPattern() const {
    runtime::TestPattern pattern;
    switch (pattern_->currentIndex()) {
        case 1: pattern.kind = runtime::TestPatternKind::Sweep; pattern.axis = runtime::SweepAxis::X; break;
        case 2: pattern.kind = runtime::TestPatternKind::Sweep; pattern.axis = runtime::SweepAxis::Y; break;
        case 3: pattern.kind = runtime::TestPatternKind::Sweep; pattern.axis = runtime::SweepAxis::Z; break;
        case 4: pattern.kind = runtime::TestPatternKind::ColorOrder; break;
        default: pattern.kind = runtime::TestPatternKind::Chase; break;
    }
    return pattern;
}

std::vector<ColorRgba> ShapePreview::colors() const {
    if (testing_) {
        return runtime::renderTestPattern(currentPattern(), *map_, static_cast<double>(clock_.elapsed()) / 1000.0);
    }
    // Ruhebild: entlang des Kabels immer heller, die erste LED weiß.
    std::vector<ColorRgba> out(map_->size());
    const float n = static_cast<float>(std::max<std::size_t>(out.size(), 2U) - 1U);
    for (std::size_t i = 0; i < out.size(); ++i) {
        const float t = static_cast<float>(i) / n;
        out[i] = i == 0 ? ColorRgba{1.0F, 1.0F, 1.0F, 1.0F} : ColorRgba{0.05F + 0.15F * t, 0.25F + 0.45F * t, 0.45F + 0.55F * t, 1.0F};
    }
    return out;
}

bool ShapePreview::renderNow() {
    if (!map_) {
        return false;
    }
    preview::PreviewOptions options;
    options.width = static_cast<std::uint32_t>(std::clamp(view_->width(), 64, 4096));
    options.height = static_cast<std::uint32_t>(std::clamp(view_->height(), 64, 4096));
    const auto image = preview::renderPreview3d(*map_, colors(), camera_, options);
    if (!image.ok()) {
        view_->setMessage(form::fromStd(image.error().message));
        return false;
    }
    const preview::Image& img = image.value();
    const QImage wrapped{img.rgba.data(), static_cast<int>(img.width), static_cast<int>(img.height),
                         static_cast<qsizetype>(img.width) * 4, QImage::Format_RGBA8888};
    last_ = wrapped.copy();
    view_->setMessage(QString{});
    view_->setImage(last_);
    return true;
}

void ShapePreview::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    if (testing_) {
        timer_.start();
    }
    renderNow();
}

void ShapePreview::hideEvent(QHideEvent* event) {
    QWidget::hideEvent(event);
    timer_.stop();  // unsichtbar wird nichts gezeichnet
}

QString ShapePreview::exercise() {
    setShape(mapping::RingShape{});
    if (ledCount() != mapping::RingShape{}.count || !renderNow() || last_.isNull()) {
        return QStringLiteral("Formvorschau: Ring wird nicht gezeichnet");
    }
    setShape(mapping::HelixShape{});
    if (ledCount() != mapping::HelixShape{}.count || !renderNow()) {
        return QStringLiteral("Formvorschau: Spirale wird nicht gezeichnet");
    }
    setShape(mapping::CylinderShape{});
    if (ledCount() != mapping::ledCount(mapping::CylinderShape{}) || !renderNow()) {
        return QStringLiteral("Formvorschau: Säule wird nicht gezeichnet");
    }
    setShape(mapping::MatrixShape{});
    setTesting(true);
    for (int index = 0; index < pattern_->count(); ++index) {
        setPatternIndex(index);
        if (!renderNow()) {
            return QStringLiteral("Formvorschau: Testmuster %1 wird nicht gezeichnet").arg(index);
        }
        tick();
    }
    setTesting(false);
    if (testing_ || timer_.isActive()) {
        return QStringLiteral("Formvorschau: Test lässt sich nicht beenden");
    }
    mapping::MatrixShape bad;
    bad.columns = 0;
    setShape(bad);
    if (ledCount() != 0) {
        return QStringLiteral("Formvorschau: ungültige Form wird gezeichnet");
    }
    return {};
}

}  // namespace whitehole::studio
