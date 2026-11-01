// main_window_help.cpp: das Menü "Hilfe" (Dokumentation, Fehler melden, Tastenkürzel, Über) und die Kürzel in den Tooltips.
//
// Warum eine eigene Datei: Das Menü gehört zu keinem Arbeitsbereich, und die Kürzelliste soll nicht von Hand gepflegt
// werden: Sie wird aus den Aktionen des Fensters gelesen und kann deshalb nie vom Menü abweichen.
#include <QAction>
#include <QDesktopServices>
#include <QKeySequence>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QStringList>
#include <QToolButton>
#include <QUrl>

#include <algorithm>
#include <utility>

#include "main_window.hpp"

namespace whitehole::studio {

namespace {

const char* const kDocsUrl = "https://github.com/bsteinmann/WhiteHoleFX/tree/main/docs";
const char* const kIssuesUrl = "https://github.com/bsteinmann/WhiteHoleFX/issues/new";
const char* const kRepoUrl = "https://github.com/bsteinmann/WhiteHoleFX";

}  // namespace

void MainWindow::buildHelp(QMenu* help) {
    auto* docs = new QAction{tr("&Dokumentation"), this};
    docs->setShortcut(QKeySequence::HelpContents);
    docs->setToolTip(tr("Die Anleitung im Browser öffnen"));
    connect(docs, &QAction::triggered, this, []() { QDesktopServices::openUrl(QUrl{QString::fromLatin1(kDocsUrl)}); });

    auto* issue = new QAction{tr("&Fehler melden oder Idee vorschlagen …"), this};
    issue->setToolTip(tr("Öffnet GitHub: ein neues Issue anlegen"));
    connect(issue, &QAction::triggered, this, []() { QDesktopServices::openUrl(QUrl{QString::fromLatin1(kIssuesUrl)}); });

    auto* keys = new QAction{tr("&Tastenkürzel …"), this};
    keys->setToolTip(tr("Alle Tastenkürzel in einer Liste"));
    connect(keys, &QAction::triggered, this, [this]() { showShortcuts(); });

    auto* about = new QAction{tr("&Über WhiteHoleFX"), this};
    about->setMenuRole(QAction::NoRole);
    connect(about, &QAction::triggered, this, [this]() { showAbout(); });

    help->addAction(docs);
    help->addAction(issue);
    help->addSeparator();
    help->addAction(keys);
    help->addAction(about);
}

std::vector<std::pair<QString, QString>> MainWindow::shortcutList() const {
    std::vector<std::pair<QString, QString>> rows;
    for (const QAction* action : findChildren<QAction*>()) {
        if (action->shortcut().isEmpty() || action->isSeparator()) {
            continue;
        }
        QString label = action->text();
        label.remove(QLatin1Char('&'));
        if (label.isEmpty()) {
            continue;
        }
        std::pair<QString, QString> row{action->shortcut().toString(QKeySequence::NativeText), label};
        if (std::find(rows.begin(), rows.end(), row) == rows.end()) {
            rows.push_back(std::move(row));
        }
    }
    std::sort(rows.begin(), rows.end(), [](const auto& a, const auto& b) { return a.second.localeAwareCompare(b.second) < 0; });
    return rows;
}

void MainWindow::showShortcuts() {
    QString html = QStringLiteral("<table cellspacing='6'>");
    for (const auto& [key, label] : shortcutList()) {
        html += QStringLiteral("<tr><td><b>%1</b></td><td>%2</td></tr>").arg(key.toHtmlEscaped(), label.toHtmlEscaped());
    }
    html += QStringLiteral("</table>");
    QMessageBox box{QMessageBox::NoIcon, tr("Tastenkürzel"), html, QMessageBox::Close, this};
    box.setTextFormat(Qt::RichText);
    box.exec();
}

void MainWindow::showAbout() {
    const QString text =
        tr("<b>WhiteHoleFX Studio</b> %1<br><br>Steuerung für LED-Matrizen, Cubes und Pixel mit einem Node-Editor.<br>"
           "Lizenz: PolyForm Shield 1.0.0<br><br><a href=\"%2\">%2</a>")
            .arg(QStringLiteral(WH_VERSION), QString::fromLatin1(kRepoUrl));
    QMessageBox::about(this, tr("Über WhiteHoleFX"), text);
}

void MainWindow::addShortcutHints() {
    // Eine Leistenschaltfläche zeigt ihr Kürzel nicht von selbst; Tooltips mit eigener Klammer (Strg+N …) bleiben.
    for (QAction* action : findChildren<QAction*>()) {
        if (action->shortcut().isEmpty() || action->toolTip().contains(QLatin1Char('('))) {
            continue;
        }
        action->setToolTip(tr("%1 (%2)").arg(action->toolTip(), action->shortcut().toString(QKeySequence::NativeText)));
    }
}

QString MainWindow::exerciseMenus() {
    QStringList titles;
    for (const QAction* entry : menuBar()->actions()) {
        titles << entry->text().remove(QLatin1Char('&'));
    }
    const QStringList expected{QStringLiteral("Datei"),      QStringLiteral("Bearbeiten"), QStringLiteral("Ansicht"),
                               QStringLiteral("Wiedergabe"), QStringLiteral("Live"),       QStringLiteral("Hilfe")};
    if (titles != expected) {
        return QStringLiteral("Menüleiste: falsche Reihenfolge (") + titles.join(QLatin1Char(',')) + QLatin1Char(')');
    }
    QMenu* help = menuBar()->actions().last()->menu();
    int helpEntries = 0;
    for (const QAction* entry : help->actions()) {
        helpEntries += entry->isSeparator() ? 0 : 1;
    }
    if (helpEntries != 4) {
        return QStringLiteral("Menüleiste: das Hilfe-Menü hat nicht vier Einträge");
    }
    // Die Kürzelliste kennt die wichtigsten Aktionen, und die Leistenschaltflächen nennen ihr Kürzel.
    const auto rows = shortcutList();
    for (const QAction* action : {newAction_, openAction_, saveAction_, runAction_, settingsAction_}) {
        QString label = action->text();
        label.remove(QLatin1Char('&'));
        const bool listed = std::any_of(rows.begin(), rows.end(), [&](const auto& row) { return row.second == label; });
        if (!listed && !action->shortcut().isEmpty()) {
            return QStringLiteral("Menüleiste: in der Kürzelliste fehlt ") + label;
        }
    }
    if (!saveAction_->toolTip().contains(QLatin1Char('('))) {
        return QStringLiteral("Menüleiste: der Tooltip von Speichern nennt das Kürzel nicht");
    }
    if (sendButton_ == nullptr || !sendButton_->styleSheet().contains(QStringLiteral("checked"))) {
        return QStringLiteral("Menüleiste: der Senden-Knopf ist nicht rot gestaltet");
    }
    return {};
}

}  // namespace whitehole::studio
