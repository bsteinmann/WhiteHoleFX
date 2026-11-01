// app_language.hpp: die Sprache der Oberfläche (Deutsch, Englisch, Französisch, Spanisch).
//
// Warum es das gibt: Die Texte stehen deutsch im Quelltext (tr("…")). Für die anderen Sprachen hängt hier ein
// QTranslator, der die eingebetteten .po-Kataloge (apps/whitehole_studio/i18n/) liest, ohne lupdate/lrelease. Welche
// Sprache gilt, entscheidet: Kommandozeile (--lang) vor gespeicherter Wahl vor Sprache des Systems.
// Die Umstellung während des Betriebs gibt es nicht; die Oberfläche baut ihre Texte beim Start. Das Hauptfenster
// bietet darum "Neu starten" an.
#pragma once

#include <QString>
#include <QStringList>

namespace whitehole::studio {

/// Gespeicherte Wahl ("de", "en", "fr", "es") oder leer.
[[nodiscard]] QString storedLanguage();
void storeLanguage(const QString& code);

/// Welche Sprache beim Start gilt: `override` (leer = keine), sonst die gespeicherte, sonst die des Systems.
[[nodiscard]] QString initialLanguage(const QString& override);

/// Installiert die Übersetzung (Deutsch entfernt sie). Gibt false zurück, wenn der Katalog fehlt oder unlesbar ist.
bool installLanguage(const QString& code);

/// Die gerade installierte Sprache.
[[nodiscard]] QString currentLanguage();

/// Sprachen zum Auswählen: Code und Name in der eigenen Sprache.
[[nodiscard]] QList<std::pair<QString, QString>> availableLanguages();

/// Das Tutorial als Markdown in der Sprache (Ausweichen auf Englisch, dann Deutsch).
[[nodiscard]] QString tutorialText(const QString& code);

}  // namespace whitehole::studio
