// scroll_behavior.cpp: Umsetzung zu scroll_behavior.hpp.
#include "scroll_behavior.hpp"

#include <QAbstractSlider>
#include <QAbstractSpinBox>
#include <QApplication>
#include <QComboBox>
#include <QEvent>
#include <QPointF>
#include <QScrollBar>
#include <QSlider>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <QWidget>

namespace whitehole::studio {

namespace {

/// Widgets, deren Wert das Mausrad nicht ändern darf. Scrollbalken selbst sind ausgenommen: Sie sollen scrollen.
bool isValueWidget(const QObject* object) {
    if (object->isWidgetType() && qobject_cast<const QScrollBar*>(object) == nullptr) {
        return qobject_cast<const QAbstractSpinBox*>(object) != nullptr || qobject_cast<const QComboBox*>(object) != nullptr ||
               qobject_cast<const QAbstractSlider*>(object) != nullptr;
    }
    return false;
}

class WheelGuard final : public QObject {
public:
    using QObject::QObject;

    bool eventFilter(QObject* watched, QEvent* event) override {
        if (event->type() == QEvent::Wheel && isValueWidget(watched)) {
            // Als "nicht behandelt" zurückgeben und trotzdem den Wert verschonen: Qt reicht ein abgelehntes
            // Mausrad-Ereignis an das Elternwidget weiter, dort scrollt die Fläche.
            event->ignore();
            return true;
        }
        return QObject::eventFilter(watched, event);
    }
};

const char* const kScrollBarStyle =
    "QScrollBar:vertical { width: 16px; margin: 0; background: palette(alternate-base); }"
    "QScrollBar:horizontal { height: 16px; margin: 0; background: palette(alternate-base); }"
    "QScrollBar::handle:vertical { min-height: 36px; margin: 2px; border-radius: 6px; background: palette(mid); border: 1px solid palette(dark); }"
    "QScrollBar::handle:horizontal { min-width: 36px; margin: 2px; border-radius: 6px; background: palette(mid); border: 1px solid palette(dark); }"
    "QScrollBar::handle:hover { background: palette(highlight); }"
    "QScrollBar::handle:pressed { background: palette(highlight); }"
    "QScrollBar::add-line, QScrollBar::sub-line { width: 0; height: 0; }"
    "QScrollBar::add-page, QScrollBar::sub-page { background: none; }";

}  // namespace

void installScrollBehavior(QApplication& app) {
    app.installEventFilter(new WheelGuard{&app});
    app.setStyleSheet(app.styleSheet() + QString::fromUtf8(kScrollBarStyle));
}

QString scrollBehaviorProblem() {
    // Elternwidget, das Mausrad-Ereignisse zählt, mit einem Zahlenfeld, einer Auswahl und einem Schieber darin.
    struct Parent : QWidget {
        int wheels = 0;
        void wheelEvent(QWheelEvent* event) override {
            ++wheels;
            event->accept();
        }
    } parent;
    auto* layout = new QVBoxLayout{&parent};
    auto* spin = new QSpinBox{&parent};
    spin->setRange(0, 100);
    spin->setValue(50);
    auto* combo = new QComboBox{&parent};
    combo->addItems({QStringLiteral("a"), QStringLiteral("b"), QStringLiteral("c")});
    combo->setCurrentIndex(1);
    auto* slider = new QSlider{Qt::Horizontal, &parent};
    slider->setRange(0, 100);
    slider->setValue(50);
    layout->addWidget(spin);
    layout->addWidget(combo);
    layout->addWidget(slider);
    parent.show();
    for (QWidget* widget : {static_cast<QWidget*>(spin), static_cast<QWidget*>(combo), static_cast<QWidget*>(slider)}) {
        QWheelEvent wheel{QPointF{4, 4}, widget->mapToGlobal(QPointF{4, 4}), QPoint{}, QPoint{0, 120}, Qt::NoButton,
                          Qt::NoModifier, Qt::NoScrollPhase, false};
        const int before = parent.wheels;
        QApplication::sendEvent(widget, &wheel);
        if (parent.wheels != before + 1) {
            return QStringLiteral("Mausrad: %1 hat das Ereignis nicht an das Elternwidget weitergereicht")
                .arg(QString::fromLatin1(widget->metaObject()->className()));
        }
    }
    if (spin->value() != 50 || combo->currentIndex() != 1 || slider->value() != 50) {
        return QStringLiteral("Mausrad: ein Zahlenfeld, eine Auswahl oder ein Schieber hat seinen Wert geändert");
    }
    return {};
}

}  // namespace whitehole::studio
