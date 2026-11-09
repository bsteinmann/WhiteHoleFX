// main_window_update.cpp: Update-Benachrichtigung im Hauptfenster (Menü Hilfe, Frage beim ersten Start).
//
// Warum eine eigene Datei: Das Netzwerk, die Einwilligung und die Dialoge gehören zusammen und sollen nicht
// in main_window_help.cpp wachsen. Ablauf:
//   - Hilfe > "Auf Updates prüfen …" fragt sofort (der Klick ist die Zustimmung) und zeigt das Ergebnis.
//   - Beim ersten Start fragt das Programm einmal, ob es künftig beim Start nachsehen darf. Ohne ein "Ja"
//     geht nie eine Anfrage hinaus. Die Wahl lässt sich im Hilfe-Menü jederzeit ändern.
//   - Der Start-Check meldet sich nur, wenn es etwas Neues gibt (Statusleiste), nie bei "aktuell" oder Fehlern.
#include <QAction>
#include <QDesktopServices>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QStatusBar>
#include <QTimer>
#include <QUrl>

#include "main_window.hpp"
#include "update_checker.hpp"

namespace whitehole::studio {

void MainWindow::buildUpdateActions(QMenu* help) {
    updateChecker_ = new UpdateChecker{this};
    connect(updateChecker_, &UpdateChecker::finished, this, [this](const UpdateChecker::Result& result) {
        showUpdateResult(result, updateUserInitiated_);
        updateUserInitiated_ = false;
    });

    auto* checkAction = new QAction{tr("Auf &Updates prüfen …"), this};
    checkAction->setToolTip(tr("Fragt bei GitHub nach der neuesten Version (nur die Versionsnummer, nichts wird installiert)"));
    connect(checkAction, &QAction::triggered, this, [this]() { checkForUpdates(true); });

    autoUpdateAction_ = new QAction{tr("Beim Start nach Updates &suchen"), this};
    autoUpdateAction_->setCheckable(true);
    autoUpdateAction_->setChecked(updateConsent() == UpdateConsent::Yes);
    autoUpdateAction_->setToolTip(tr("Beim Programmstart einmal bei GitHub nachsehen, ob es eine neuere Version gibt"));
    connect(autoUpdateAction_, &QAction::toggled, this,
            [](bool on) { setUpdateConsent(on ? UpdateConsent::Yes : UpdateConsent::No); });

    help->addAction(checkAction);
    help->addAction(autoUpdateAction_);
}

void MainWindow::checkForUpdates(bool userInitiated) {
    if (updateChecker_->running()) {
        return;
    }
    updateUserInitiated_ = userInitiated;
    if (userInitiated) {
        statusBar()->showMessage(tr("Suche nach Updates …"), 8000);
    }
    updateChecker_->start();
}

void MainWindow::showUpdateResult(const UpdateChecker::Result& result, bool userInitiated) {
    using Outcome = UpdateChecker::Outcome;
    if (!userInitiated) {
        if (result.outcome == Outcome::Available) {
            statusBar()->showMessage(tr("Neue Version %1 verfügbar – Hilfe > Auf Updates prüfen …").arg(result.version), 30000);
        }
        return;
    }
    statusBar()->clearMessage();
    if (result.outcome == Outcome::Failed) {
        QMessageBox::warning(this, tr("Update-Prüfung"), tr("Die Prüfung ist fehlgeschlagen:\n%1").arg(result.error));
    } else if (result.outcome == Outcome::UpToDate) {
        QMessageBox::information(this, tr("Update-Prüfung"),
                                 tr("Du hast die neueste Version (%1).").arg(QStringLiteral(WH_VERSION)));
    } else {
        QMessageBox box{QMessageBox::Information, tr("Update-Prüfung"),
                        tr("Version %1 ist verfügbar (installiert: %2).").arg(result.version, QStringLiteral(WH_VERSION)), QMessageBox::Close, this};
        QPushButton* openButton = box.addButton(tr("Seite öffnen"), QMessageBox::AcceptRole);
        box.exec();
        if (box.clickedButton() == openButton) {
            QDesktopServices::openUrl(QUrl{result.url});
        }
    }
}

void MainWindow::startupUpdateCheck() {
    if (updateCheckDisabledByEnvironment() || updateConsent() == UpdateConsent::No) {
        return;
    }
    // Erst nach dem Start, wenn das Fenster steht: weder Frage noch Anfrage bremsen das Öffnen.
    QTimer::singleShot(1500, this, [this]() {
        if (updateConsent() == UpdateConsent::Unasked) {
            QMessageBox box{QMessageBox::Question, tr("Nach Updates suchen?"),
                            tr("Soll WhiteHoleFX beim Start bei GitHub nachsehen, ob es eine neuere Version gibt?"),
                            QMessageBox::NoButton, this};
            box.setInformativeText(tr("Dabei wird nur die öffentliche Seite des neuesten Releases abgefragt. Gesendet werden "
                                      "Programmname und Versionsnummer (und, wie bei jedem Netzwerkzugriff, deine IP-Adresse), "
                                      "nichts über dich oder deine Projekte. Es wird nichts installiert. Du kannst das jederzeit "
                                      "im Menü Hilfe ändern."));
            QPushButton* yesButton = box.addButton(tr("Ja, beim Start suchen"), QMessageBox::AcceptRole);
            box.addButton(tr("Nein"), QMessageBox::RejectRole);
            box.exec();
            const bool agreed = box.clickedButton() == yesButton;
            setUpdateConsent(agreed ? UpdateConsent::Yes : UpdateConsent::No);
            autoUpdateAction_->setChecked(agreed);  // löst setUpdateConsent erneut aus, mit demselben Wert
            if (!agreed) {
                return;
            }
        }
        checkForUpdates(false);
    });
}

QString MainWindow::exerciseUpdate() {
    using Outcome = UpdateChecker::Outcome;
    const core::Version now = UpdateChecker::currentVersion();
    const QByteArray newer = QByteArrayLiteral(R"({"tag_name":"v99.0.0","html_url":"https://github.com/bsteinmann/WhiteHoleFX/releases/tag/v99.0.0"})");
    const auto available = UpdateChecker::evaluate(newer, now);
    if (available.outcome != Outcome::Available || available.version != QStringLiteral("99.0.0") || available.url.isEmpty()) {
        return QStringLiteral("Update-Prüfung: neuere Version wird nicht erkannt");
    }
    const QByteArray same = QByteArrayLiteral(R"({"tag_name":"v)") + WH_VERSION + R"(","html_url":"https://github.com/bsteinmann/WhiteHoleFX"})";
    if (UpdateChecker::evaluate(same, now).outcome != Outcome::UpToDate) {
        return QStringLiteral("Update-Prüfung: gleiche Version gilt nicht als aktuell");
    }
    if (UpdateChecker::evaluate(QByteArrayLiteral("<html>"), now).outcome != Outcome::Failed) {
        return QStringLiteral("Update-Prüfung: unlesbare Antwort gilt nicht als Fehler");
    }
    if (autoUpdateAction_ == nullptr || !autoUpdateAction_->isCheckable() || updateChecker_ == nullptr || updateChecker_->running()) {
        return QStringLiteral("Update-Prüfung: Menüeintrag oder Prüfer fehlt");
    }
    return {};
}

}  // namespace whitehole::studio
