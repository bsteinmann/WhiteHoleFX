// update_checker.hpp: fragt bei GitHub, ob es eine neuere Version von WhiteHoleFX gibt.
//
// Warum es das gibt: Wer das Studio nutzt, soll von neuen Versionen erfahren, ohne die Projektseite zu
// beobachten. Datenschutz geht dabei vor: Das Programm fragt nie von allein nach, solange der Nutzer nicht
// zugestimmt hat (Einwilligung "update/autoCheck" in QSettings, beim ersten Start per Frage). Die Anfrage
// ist ein einziger GET auf die öffentliche API von GitHub; sie enthält nur Programmname und Version im
// User-Agent (und, wie jede Anfrage, die IP-Adresse). Es wird nichts installiert und nichts heruntergeladen.
//
// Die Bewertung der Antwort (`evaluate`) hängt nicht am Netzwerk und ist in der Oberfläche und in den Tests
// ohne Verbindung prüfbar; die Logik dahinter steht in core/update_check.hpp.
#pragma once

#include <QByteArray>
#include <QNetworkAccessManager>
#include <QObject>
#include <QString>

#include "whitehole/core/update_check.hpp"

class QNetworkReply;

namespace whitehole::studio {

/// Einwilligung zur automatischen Prüfung beim Start.
enum class UpdateConsent { Unasked, Yes, No };

[[nodiscard]] UpdateConsent updateConsent();
void setUpdateConsent(UpdateConsent consent);

/// Die Umgebungsvariable WHITEHOLEFX_NO_UPDATE_CHECK=1 schaltet jede Anfrage beim Start ab (CI, Vorführrechner).
[[nodiscard]] bool updateCheckDisabledByEnvironment();

class UpdateChecker : public QObject {
    Q_OBJECT

public:
    enum class Outcome { UpToDate, Available, Failed };

    struct Result {
        Outcome outcome{Outcome::Failed};
        QString version;  // bei Available: die neue Version, z. B. "0.65.0"
        QString url;      // bei Available: Seite des Releases
        QString error;    // bei Failed: Grund, in Worten für den Nutzer
    };

    explicit UpdateChecker(QObject* parent = nullptr);

    /// Startet die Anfrage; das Ergebnis kommt asynchron über `finished`. Läuft schon eine, passiert nichts.
    void start();
    [[nodiscard]] bool running() const noexcept { return reply_ != nullptr; }

    /// Wertet die Antwort von GitHub aus (ohne Netzwerk). Eine unlesbare Antwort ist Failed.
    [[nodiscard]] static Result evaluate(const QByteArray& json, const core::Version& current);
    [[nodiscard]] static core::Version currentVersion();

Q_SIGNALS:
    void finished(const UpdateChecker::Result& result);

private:
    void onReply();

    QNetworkAccessManager network_;
    QNetworkReply* reply_{nullptr};
};

}  // namespace whitehole::studio

Q_DECLARE_METATYPE(whitehole::studio::UpdateChecker::Result)
