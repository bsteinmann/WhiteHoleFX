// main_window_view3d.cpp: Umschalter zwischen der bearbeitbaren 2D-Vorschau und der drehbaren 3D-Ansicht.
//
// Warum eine eigene Datei: main_window.cpp ist an der 400-Zeilen-Grenze. Hier steht, was Menü, Leiste
// "Ansicht" und Tastenkürzel mit PreviewWidget und Player::image3d verbindet, und der Rauchtest dafür.
//
// Zwei Betriebsarten der Vorschau: "2D" ist die Ansicht von vorn, in der man Fixtures verschiebt und den
// Bereich aufzieht. Die 3D-Ansichten (frei drehbar, Front, Seite, Oben) sind nur zum Ansehen: die Maus
// gehört dort der Kamera. Ein Wechsel ändert nichts am Projekt.
#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QMenu>
#include <QMouseEvent>
#include <QSettings>
#include <QToolBar>

#include <algorithm>
#include <cmath>

#include "main_window.hpp"
#include "preview_widget.hpp"

namespace whitehole::studio {

namespace {

constexpr std::size_t slotOf(int mode) { return static_cast<std::size_t>(mode); }

}  // namespace

void MainWindow::buildView3d(QMenu* menu) {
    struct Entry {
        ViewMode mode;
        const char* text;
        const char* tip;
        Qt::Key key;
    };
    static const Entry entries[] = {
        {ViewMode::Edit2d, QT_TR_NOOP("&2D (bearbeiten)"), QT_TR_NOOP("Ansicht von vorn: Fixtures verschieben, Bereich aufziehen"), Qt::Key_1},
        {ViewMode::Free3d, QT_TR_NOOP("&3D"), QT_TR_NOOP("Frei drehbare Ansicht: ziehen = drehen, Rechtsklick = verschieben, Rad = Zoom"), Qt::Key_2},
        {ViewMode::Front, QT_TR_NOOP("3D &Front"), QT_TR_NOOP("3D von vorn (maßhaltig, zum Drehen einfach ziehen)"), Qt::Key_3},
        {ViewMode::Side, QT_TR_NOOP("3D &Seite"), QT_TR_NOOP("3D von rechts (maßhaltig)"), Qt::Key_4},
        {ViewMode::Top, QT_TR_NOOP("3D &Oben"), QT_TR_NOOP("3D von oben (maßhaltig)"), Qt::Key_5},
    };
    viewGroup_ = new QActionGroup{this};
    viewGroup_->setExclusive(true);
    QToolBar* bar = addToolBar(tr("Ansicht"));
    bar->setObjectName(QStringLiteral("viewBar"));
    bar->setMovable(false);
    menu->addSeparator();
    for (const Entry& entry : entries) {
        auto* action = new QAction{tr(entry.text), this};
        action->setCheckable(true);
        action->setToolTip(tr(entry.tip));
        action->setShortcut(QKeySequence{Qt::CTRL | entry.key});
        viewGroup_->addAction(action);
        menu->addAction(action);
        bar->addAction(action);
        viewActions_[slotOf(static_cast<int>(entry.mode))] = action;
        const ViewMode mode = entry.mode;
        connect(action, &QAction::triggered, this, [this, mode]() { setViewMode(mode); });
    }
    viewActions_[slotOf(static_cast<int>(ViewMode::Edit2d))]->setChecked(true);
    buildView3dStyle(menu);
    freeCamera_ = styled(preview::cameraFor(preview::ViewPreset::Perspective));
    connect(preview_, &PreviewWidget::cameraChanged, this, &MainWindow::onCameraChanged);
}

preview::Camera3d MainWindow::styled(preview::Camera3d camera) const {
    camera.offLeds = offLeds3d_;
    camera.glow = glow3d_;
    return camera;
}

void MainWindow::applyView3dStyle() {
    freeCamera_ = styled(freeCamera_);
    preview_->setCamera(styled(preview_->camera()));
    refreshImage();
}

void MainWindow::buildView3dStyle(QMenu* menu) {
    // Aus-LEDs: In 2D sieht man das Layout gern an grauen Punkten, in einem Cube verdecken sie aber die
    // leuchtenden LEDs dahinter. Darum ist die Vorgabe in 3D "unsichtbar"; die Wahl bleibt gespeichert.
    QSettings stored{QStringLiteral("WhiteHoleFX"), QStringLiteral("WhiteHoleFX")};
    offLeds3d_ = static_cast<preview::OffLeds>(std::clamp(stored.value(QStringLiteral("view3d/offLeds"), 0).toInt(), 0, 2));
    glow3d_ = stored.value(QStringLiteral("view3d/glow"), true).toBool();

    QMenu* style = menu->addMenu(tr("3D-&Darstellung"));
    struct Entry {
        preview::OffLeds mode;
        const char* text;
        const char* tip;
    };
    static const Entry entries[] = {
        {preview::OffLeds::Hidden, QT_TR_NOOP("Aus-LEDs &unsichtbar"), QT_TR_NOOP("Nur leuchtende LEDs zeichnen: man sieht durch den Cube hindurch")},
        {preview::OffLeds::Dim, QT_TR_NOOP("Aus-LEDs &schwach"), QT_TR_NOOP("Ausgeschaltete LEDs als sehr dunkle Punkte, nur als Orientierung")},
        {preview::OffLeds::Gray, QT_TR_NOOP("Aus-LEDs &grau"), QT_TR_NOOP("Wie in der 2D-Ansicht; graue Punkte der vorderen Schicht verdecken LEDs dahinter")},
    };
    auto* group = new QActionGroup{this};
    group->setExclusive(true);
    for (const Entry& entry : entries) {
        auto* action = new QAction{tr(entry.text), this};
        action->setCheckable(true);
        action->setToolTip(tr(entry.tip));
        action->setChecked(entry.mode == offLeds3d_);
        group->addAction(action);
        style->addAction(action);
        const preview::OffLeds mode = entry.mode;
        connect(action, &QAction::triggered, this, [this, mode]() {
            offLeds3d_ = mode;
            QSettings{QStringLiteral("WhiteHoleFX"), QStringLiteral("WhiteHoleFX")}.setValue(QStringLiteral("view3d/offLeds"), static_cast<int>(mode));
            applyView3dStyle();
        });
    }
    style->addSeparator();
    auto* glow = new QAction{tr("&Leuchten (additiv)"), this};
    glow->setCheckable(true);
    glow->setChecked(glow3d_);
    glow->setToolTip(tr("Leuchtende LEDs mit weichem Schein, Farben addieren sich; nichts verdeckt etwas"));
    style->addAction(glow);
    connect(glow, &QAction::toggled, this, [this](bool on) {
        glow3d_ = on;
        QSettings{QStringLiteral("WhiteHoleFX"), QStringLiteral("WhiteHoleFX")}.setValue(QStringLiteral("view3d/glow"), on);
        applyView3dStyle();
    });
}

void MainWindow::setViewMode(ViewMode mode) {
    viewMode_ = mode;
    switch (mode) {
        case ViewMode::Edit2d: break;
        case ViewMode::Free3d: preview_->setCamera(freeCamera_); break;
        case ViewMode::Front: preview_->setCamera(styled(preview::cameraFor(preview::ViewPreset::Front))); break;
        case ViewMode::Side: preview_->setCamera(styled(preview::cameraFor(preview::ViewPreset::Side))); break;
        case ViewMode::Top: preview_->setCamera(styled(preview::cameraFor(preview::ViewPreset::Top))); break;
    }
    preview_->setView3d(mode != ViewMode::Edit2d);
    viewActions_[slotOf(static_cast<int>(mode))]->setChecked(true);  // setChecked löst kein triggered aus
    refreshImage();
}

void MainWindow::onCameraChanged(const preview::Camera3d& camera) {
    // Frei gedreht (Maus oder Pfeiltasten) heißt: keine feste Ansicht mehr, der Haken wandert auf "3D".
    if (camera.perspective) {
        freeCamera_ = camera;
        if (viewMode_ != ViewMode::Free3d) {
            viewMode_ = ViewMode::Free3d;
            viewActions_[slotOf(static_cast<int>(ViewMode::Free3d))]->setChecked(true);
        }
    }
    refreshImage();
}

QString MainWindow::exerciseView3d() {
    if (player_->project() == nullptr) {
        return QStringLiteral("kein Projekt geladen");
    }
    const auto action = [this](ViewMode mode) { return viewActions_[slotOf(static_cast<int>(mode))]; };
    if (viewMode_ != ViewMode::Edit2d || preview_->view3d() || !action(ViewMode::Edit2d)->isChecked()) {
        return QStringLiteral("die Vorschau startet nicht in 2D");
    }

    action(ViewMode::Free3d)->trigger();
    if (!preview_->view3d() || viewMode_ != ViewMode::Free3d || !action(ViewMode::Free3d)->isChecked()) {
        return QStringLiteral("\"3D\" schaltet die Vorschau nicht um");
    }
    preview::PreviewOptions options;
    options.width = 320;
    options.height = 200;
    const auto before = player_->image3d(preview_->camera(), options);
    if (!before.ok()) {
        return QStringLiteral("3D-Bild: ") + QString::fromUtf8(before.error().message.c_str());
    }

    // Ziehen mit der linken Taste dreht die Kamera (echte Mausereignisse an das Widget).
    const float yawBefore = preview_->camera().yawDegrees;
    const QPointF start{100.0, 100.0};
    QMouseEvent press{QEvent::MouseButtonPress, start, preview_->mapToGlobal(start), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier};
    QApplication::sendEvent(preview_, &press);
    const QPointF moved{160.0, 100.0};
    QMouseEvent move{QEvent::MouseMove, moved, preview_->mapToGlobal(moved), Qt::NoButton, Qt::LeftButton, Qt::NoModifier};
    QApplication::sendEvent(preview_, &move);
    QMouseEvent release{QEvent::MouseButtonRelease, moved, preview_->mapToGlobal(moved), Qt::LeftButton, Qt::NoButton, Qt::NoModifier};
    QApplication::sendEvent(preview_, &release);
    if (std::abs(preview_->camera().yawDegrees - yawBefore) < 1.0F) {
        return QStringLiteral("Ziehen dreht die Kamera nicht");
    }
    const auto after = player_->image3d(preview_->camera(), options);
    if (!after.ok() || after.value().rgba == before.value().rgba) {
        return QStringLiteral("nach dem Drehen sieht das Bild gleich aus");
    }

    // Zoom und Verschieben, dann zurücksetzen.
    preview_->zoomBy(2.0);
    preview_->panBy(20.0, 10.0);
    if (preview_->camera().zoom < 1.9F || preview_->camera().panX <= 0.0F) {
        return QStringLiteral("Zoom oder Verschieben kommen nicht an");
    }
    preview_->resetZoomAndPan();
    if (preview_->camera().zoom != 1.0F || preview_->camera().panX != 0.0F || preview_->camera().panY != 0.0F) {
        return QStringLiteral("Zurücksetzen von Zoom und Verschiebung scheitert");
    }

    // Feste Ansichten.
    action(ViewMode::Top)->trigger();
    if (preview_->camera().pitchDegrees != 90.0F || preview_->camera().perspective) {
        return QStringLiteral("\"3D Oben\" stellt nicht die Draufsicht ein");
    }
    action(ViewMode::Side)->trigger();
    if (preview_->camera().yawDegrees != 90.0F) {
        return QStringLiteral("\"3D Seite\" stellt nicht die Seitenansicht ein");
    }
    action(ViewMode::Front)->trigger();
    if (preview_->camera().yawDegrees != 0.0F || preview_->camera().pitchDegrees != 0.0F) {
        return QStringLiteral("\"3D Front\" stellt nicht die Frontansicht ein");
    }
    // Aus einer festen Ansicht heraus ziehen: wird zur freien 3D-Ansicht.
    preview_->orbitBy(30.0, 0.0);
    if (viewMode_ != ViewMode::Free3d || !action(ViewMode::Free3d)->isChecked()) {
        return QStringLiteral("Drehen aus einer festen Ansicht schaltet nicht auf \"3D\"");
    }

    // Darstellung: Aus-LEDs grau und ohne Schein ändert das Bild und bleibt bei festen Ansichten erhalten.
    const auto defaultStyle = player_->image3d(preview_->camera(), options);
    offLeds3d_ = preview::OffLeds::Gray;
    glow3d_ = false;
    applyView3dStyle();
    if (preview_->camera().offLeds != preview::OffLeds::Gray || preview_->camera().glow) {
        return QStringLiteral("die 3D-Darstellung kommt nicht in der Kamera an");
    }
    const auto grayStyle = player_->image3d(preview_->camera(), options);
    if (!defaultStyle.ok() || !grayStyle.ok() || defaultStyle.value().rgba == grayStyle.value().rgba) {
        return QStringLiteral("graue Aus-LEDs ohne Schein sehen aus wie die Vorgabe");
    }
    action(ViewMode::Top)->trigger();
    if (preview_->camera().offLeds != preview::OffLeds::Gray || preview_->camera().glow) {
        return QStringLiteral("die 3D-Darstellung geht beim Wechsel der Ansicht verloren");
    }
    offLeds3d_ = preview::OffLeds::Hidden;
    glow3d_ = true;
    applyView3dStyle();

    action(ViewMode::Edit2d)->trigger();
    if (preview_->view3d() || viewMode_ != ViewMode::Edit2d) {
        return QStringLiteral("\"2D\" schaltet nicht zurück");
    }
    return {};
}

}  // namespace whitehole::studio
