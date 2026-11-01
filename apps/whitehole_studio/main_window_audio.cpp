// main_window_audio.cpp: Audio-Eingang im Hauptfenster (Quelle starten und stoppen, Statusanzeige).
//
// Warum eine eigene Datei: Die Wahl der Audioquelle ist wie der DMX-Empfang eine Betriebsentscheidung
// und kein Teil des Projekts. Das Rechnen (Pegel, Spektrum, Beat) macht audio::AudioInput (wh_audio);
// hier stehen nur das Starten der gewählten Quelle und die Anzeige in der Statuszeile. Gewählt wird in den
// Einstellungen (Bearbeiten > Einstellungen, Seite Audio).
//
// Sicherheit/Datenschutz: Nach der Installation ist der Eingang AUS; das Mikrofon wird erst auf eine
// ausdrückliche Wahl in den Einstellungen geöffnet (das Betriebssystem fragt dann ggf. nach). Die Wahl wird
// gemerkt, weil sie bewusst getroffen wurde; scheitert das Öffnen beim nächsten Start, bleibt der
// Eingang aus.
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QThread>

#include <cmath>
#include <cstdint>

#include "main_window.hpp"
#include "whitehole/audio/audio_input.hpp"

namespace whitehole::studio {

namespace {

QString toQ(const std::string& text) { return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size())); }

std::string toStd(const QString& text) {
    const QByteArray bytes = text.toUtf8();
    return std::string{bytes.constData(), static_cast<std::size_t>(bytes.size())};
}

}  // namespace

QString MainWindow::applyAudioChoice(const QString& choice) {
    audioInput_->stop();
    Result<void> result;
    if (choice == QStringLiteral("default")) {
        result = audioInput_->startDevice("");
    } else if (choice.startsWith(QStringLiteral("device:"))) {
        result = audioInput_->startDevice(toStd(choice.mid(7)));
    } else if (choice.startsWith(QStringLiteral("file:"))) {
        result = audioInput_->startFile(toStd(choice.mid(5)));
    }
    if (!result.ok()) {
        audioChoice_ = QStringLiteral("off");
        return tr("Audio-Eingang: %1").arg(toQ(result.error().message));
    }
    audioChoice_ = choice;
    return {};
}

QString MainWindow::audioStatusText() const {
    const audio::AudioStatus status = library_->audio->status();
    if (!status.running) {
        return player_->audioUsage().any() ? tr("   |   Audio-Nodes im Projekt, Eingang aus (Einstellungen)") : QString{};
    }
    QString text = tr("   |   Audio: %1").arg(toQ(status.source));
    if (const auto snapshot = library_->audio->read()) {
        text += tr(", Pegel %1 %").arg(std::lround(static_cast<double>(snapshot->frame.rms) * 100.0));
        if (snapshot->frame.beat.bpm > 0.0F) {
            text += tr(", %1 BPM").arg(std::lround(static_cast<double>(snapshot->frame.beat.bpm)));
        }
    }
    return text;
}

QString MainWindow::exerciseAudio() {
    // Ende-zu-Ende ohne Gerät: eine WAV-Datei über das Menü-Verfahren wählen, warten, bis Frames im
    // Speicher ankommen, dann ausschalten.
    QTemporaryDir folder;
    if (!folder.isValid()) {
        return QStringLiteral("Audio-Eingang: kein temporärer Ordner für den Test");
    }
    const QString path = folder.filePath(QStringLiteral("test.wav"));
    {
        constexpr std::uint32_t kRate = 44100;
        constexpr std::uint32_t kSamples = kRate;  // eine Sekunde
        QByteArray wav;
        const auto put32 = [&wav](std::uint32_t v) {
            for (int i = 0; i < 4; ++i) {
                wav.append(static_cast<char>((v >> (8 * i)) & 0xFFU));
            }
        };
        const auto put16 = [&wav](std::uint16_t v) {
            wav.append(static_cast<char>(v & 0xFFU));
            wav.append(static_cast<char>((v >> 8) & 0xFFU));
        };
        wav.append("RIFF", 4);
        put32(36 + kSamples * 2);
        wav.append("WAVEfmt ", 8);
        put32(16);
        put16(1);
        put16(1);
        put32(kRate);
        put32(kRate * 2);
        put16(2);
        put16(16);
        wav.append("data", 4);
        put32(kSamples * 2);
        for (std::uint32_t i = 0; i < kSamples; ++i) {
            const double s = 0.5 * std::sin(2.0 * 3.14159265358979 * 440.0 * static_cast<double>(i) / kRate);
            put16(static_cast<std::uint16_t>(static_cast<std::int16_t>(std::lround(s * 32767.0))));
        }
        QFile file{path};
        if (!file.open(QIODevice::WriteOnly) || file.write(wav) != wav.size()) {
            return QStringLiteral("Audio-Eingang: Testdatei nicht geschrieben");
        }
    }
    const AppSettings before = appSettings_;
    AppSettings withFile = before;
    withFile.audioSource = QStringLiteral("file:") + path;
    if (const QString error = applySettings(withFile, false); !error.isEmpty() || !audioInput_->running()) {
        return QStringLiteral("Audio-Eingang: die WAV-Datei startet nicht ") + error;
    }
    bool arrived = false;
    for (int i = 0; i < 300 && !arrived; ++i) {
        const auto snapshot = library_->audio->read();
        arrived = snapshot && snapshot->frame.frameCount > 3 && snapshot->frame.rms > 0.1F;
        if (!arrived) {
            QThread::msleep(10);
            QCoreApplication::processEvents();
        }
    }
    const QString status = audioStatusText();
    applySettings(before, false);
    if (audioInput_->running() != (before.audioSource != QStringLiteral("off"))) {
        return QStringLiteral("Audio-Eingang: Ausschalten gescheitert");
    }
    if (!arrived) {
        return QStringLiteral("Audio-Eingang: es kamen keine Frames an");
    }
    return status.contains(QStringLiteral("Audio:")) ? QString{} : QStringLiteral("Audio-Eingang: keine Statusanzeige");
}

}  // namespace whitehole::studio
