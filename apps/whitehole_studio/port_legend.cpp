// port_legend.cpp: Umsetzung zu port_legend.hpp.
#include "port_legend.hpp"

#include <QGridLayout>
#include <QLabel>
#include <QPainter>

#include <array>

#include "node_items.hpp"

namespace whitehole::studio {

namespace {

/// Ein kleines Widget, das nur den Port-Marker eines Typs zeichnet (Ausgang gefüllt, Eingang als Ring).
class MarkerWidget : public QWidget {
public:
    MarkerWidget(graph::PortType type, bool output, QWidget* parent) : QWidget{parent}, type_{type}, output_{output} {
        setFixedSize(22, 22);
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter painter{this};
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.fillRect(rect(), QColor{30, 31, 36});
        drawPortMarker(&painter, QPointF{width() / 2.0, height() / 2.0}, type_, output_, 6.0);
    }

private:
    graph::PortType type_;
    bool output_;
};

struct Row {
    graph::PortType type;
    const char* name;
    const char* meaning;
};

}  // namespace

PortLegend::PortLegend(QWidget* parent) : QWidget{parent, Qt::Tool} {
    setWindowTitle(tr("Legende der Ports"));
    auto* grid = new QGridLayout{this};
    grid->setColumnStretch(3, 1);
    int row = 0;
    const auto heading = [&](const QString& text) {
        auto* label = new QLabel{QStringLiteral("<b>%1</b>").arg(text), this};
        grid->addWidget(label, row++, 0, 1, 4);
    };

    heading(tr("Ein- und Ausgang"));
    grid->addWidget(new MarkerWidget{graph::PortType::Field, true, this}, row, 0);
    grid->addWidget(new QLabel{tr("Ausgang (gefüllt, rechts am Node)"), this}, row++, 1, 1, 3);
    grid->addWidget(new MarkerWidget{graph::PortType::Field, false, this}, row, 0);
    grid->addWidget(new QLabel{tr("Eingang (nur Ring, links am Node)"), this}, row++, 1, 1, 3);

    heading(tr("Porttypen: Farbe und Form"));
    // Die Texte sind Übersetzungs-Schlüssel (QT_TR_NOOP), damit sie im Katalog stehen.
    const std::array<Row, 12> rows{{
        {graph::PortType::Field, QT_TR_NOOP("Field"), QT_TR_NOOP("Bild auf der Bühne: eine Farbe für jede Position")},
        {graph::PortType::Image, QT_TR_NOOP("Image"), QT_TR_NOOP("Bild als Textur (z. B. aus einer Datei)")},
        {graph::PortType::Scalar, QT_TR_NOOP("Zahl"), QT_TR_NOOP("ein Zahlenwert, auch zeitlich veränderlich")},
        {graph::PortType::Integer, QT_TR_NOOP("Ganzzahl"), QT_TR_NOOP("Zähler oder Auswahl")},
        {graph::PortType::Boolean, QT_TR_NOOP("Schalter"), QT_TR_NOOP("an oder aus")},
        {graph::PortType::Color, QT_TR_NOOP("Farbe"), QT_TR_NOOP("eine einzelne Farbe")},
        {graph::PortType::Palette, QT_TR_NOOP("Palette"), QT_TR_NOOP("ein Farbverlauf aus mehreren Farben")},
        {graph::PortType::Text, QT_TR_NOOP("Text"), QT_TR_NOOP("Zeichen, z. B. für den Text-Node")},
        {graph::PortType::Trigger, QT_TR_NOOP("Impuls"), QT_TR_NOOP("ein kurzer Auslöser, z. B. ein Beat oder Treffer")},
        {graph::PortType::AudioSpectrum, QT_TR_NOOP("Audio-Spektrum"), QT_TR_NOOP("Lautstärke je Frequenzband")},
        {graph::PortType::PointCloud, QT_TR_NOOP("Punktwolke"), QT_TR_NOOP("Positionen von Punkten im Raum")},
        {graph::PortType::AudioWave, QT_TR_NOOP("Audio-Welle"), QT_TR_NOOP("die Schwingung des Tons")},
    }};
    for (const Row& entry : rows) {
        grid->addWidget(new MarkerWidget{entry.type, true, this}, row, 0);
        grid->addWidget(new QLabel{QStringLiteral("<b>%1</b>").arg(tr(entry.name)), this}, row, 1);
        grid->addWidget(new QLabel{tr(entry.meaning), this}, row, 2, 1, 2);
        ++row;
        ++typeRows_;
    }

    heading(tr("Weiteres"));
    const auto note = [&](const QString& text) {
        auto* label = new QLabel{text, this};
        label->setWordWrap(true);
        grid->addWidget(label, row++, 0, 1, 4);
    };
    note(tr("Ein Kabel lässt sich nur zwischen passenden Typen ziehen."));
    note(tr("Orange Umrandung am Node: sein Ausgang wird auf die LEDs abgetastet."));
    note(tr("Abgeblendeter Node mit gestrichelter Durchleitung: überbrückt (Taste B)."));
    setMinimumWidth(380);
}

}  // namespace whitehole::studio
