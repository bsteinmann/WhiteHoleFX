// main_window_language.cpp: Sprachwahl (Hilfe > Sprache / Language) und das eingebaute Tutorial.
//
// Warum eine eigene Datei: Beides gehört zu keinem Arbeitsbereich und bringt Dialoge mit, die main_window_help.cpp
// aufblähen würden. Die Umstellung der Sprache braucht einen Neustart, weil Menüs und Fenster ihre Texte beim
// Aufbau übersetzen; das Fenster bietet ihn an und öffnet danach dasselbe Projekt.
#include <QAction>
#include <QActionGroup>
#include <QCoreApplication>
#include <QDialog>
#include <QMenu>
#include <QMessageBox>
#include <QProcess>
#include <QStatusBar>
#include <QTextBrowser>
#include <QVBoxLayout>

#include <string>

#include "app_language.hpp"
#include "main_window.hpp"

namespace whitehole::studio {

void MainWindow::buildLanguageMenu(QMenu* help) {
    // Der Titel steht bewusst zweisprachig und ohne Übersetzung: Wer die Oberfläche nicht lesen kann, findet ihn trotzdem.
    QMenu* menu = help->addMenu(QStringLiteral("&Sprache / Language"));
    auto* group = new QActionGroup{this};
    group->setExclusive(true);
    const QString current = currentLanguage();
    for (const auto& language : availableLanguages()) {
        const QString code = language.first;  // Kopie: Lambdas fangen keine strukturierten Bindungen
        QAction* action = menu->addAction(language.second);
        action->setCheckable(true);
        action->setChecked(code == current);
        group->addAction(action);
        connect(action, &QAction::triggered, this, [this, code]() { chooseLanguage(code); });
    }
}

void MainWindow::chooseLanguage(const QString& code) {
    if (code == currentLanguage()) {
        return;
    }
    storeLanguage(code);
    const auto answer = QMessageBox::question(
        this, tr("Sprache"),
        tr("Die neue Sprache gilt nach einem Neustart von WhiteHoleFX. Jetzt neu starten?\n(Nicht gespeicherte Änderungen werden vorher abgefragt.)"),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
    if (answer != QMessageBox::Yes) {
        statusBar()->showMessage(tr("Die Sprache wechselt beim nächsten Start."), 8000);
        return;
    }
    QStringList arguments;
    const std::string path = player_->status().path;
    if (!path.empty()) {
        arguments << QString::fromStdString(path);
    }
    if (!close()) {
        return;  // beim Speichern-Dialog abgebrochen: kein Neustart
    }
    (void)QProcess::startDetached(QCoreApplication::applicationFilePath(), arguments);
}

void MainWindow::showTutorial() {
    QDialog dialog{this};
    dialog.setWindowTitle(tr("Tutorial"));
    auto* column = new QVBoxLayout{&dialog};
    auto* browser = new QTextBrowser{&dialog};
    browser->setOpenExternalLinks(true);
    browser->setMarkdown(tutorialText(currentLanguage()));
    column->addWidget(browser);
    dialog.resize(820, 680);
    dialog.exec();
}

QString MainWindow::exerciseLanguage() {
    const QString before = currentLanguage();
    const QString german = tr("&Datei");
    QString problem;
    for (const QString& code : {QStringLiteral("en"), QStringLiteral("fr"), QStringLiteral("es")}) {
        if (!installLanguage(code) || currentLanguage() != code) {
            problem = QStringLiteral("Sprache %1: Katalog lässt sich nicht laden").arg(code);
            break;
        }
        const QString translated = tr("&Datei");
        const QString withPlaceholder = tr("WhiteHoleFX – %1%2").arg(QStringLiteral("A"), QStringLiteral("B"));
        if (translated == german || translated.isEmpty() || !withPlaceholder.contains(QLatin1Char('A')) ||
            !withPlaceholder.contains(QLatin1Char('B'))) {
            problem = QStringLiteral("Sprache %1: Menütext nicht übersetzt (%2)").arg(code, translated);
            break;
        }
        const QString text = tutorialText(code);
        if (text.size() < 2000 || !text.contains(QStringLiteral("WhiteHoleFX"))) {
            problem = QStringLiteral("Sprache %1: Tutorial fehlt oder ist zu kurz").arg(code);
            break;
        }
    }
    if (problem.isEmpty() && tutorialText(QStringLiteral("de")).size() < 2000) {
        problem = QStringLiteral("Tutorial (de) fehlt");
    }
    installLanguage(before);
    return problem;
}

}  // namespace whitehole::studio
