// app_language.cpp: PoTranslator (QTranslator über einen PoCatalog) und die Wahl der Sprache.
#include "app_language.hpp"

#include <QByteArray>
#include <QCoreApplication>
#include <QFile>
#include <QLocale>
#include <QSettings>
#include <QTranslator>

#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "whitehole/i18n/language.hpp"
#include "whitehole/i18n/po_catalog.hpp"

namespace whitehole::studio {

namespace {

constexpr const char* kLanguageKey = "ui/language";

/// QTranslator, der die Texte aus einem Katalog holt. Fehlt ein Text, bleibt es beim deutschen Quelltext.
class PoTranslator final : public QTranslator {
public:
    explicit PoTranslator(i18n::PoCatalog catalog) : catalog_{std::move(catalog)} {}

    [[nodiscard]] QString translate(const char* context, const char* sourceText, const char* disambiguation = nullptr,
                                    int n = -1) const override {
        Q_UNUSED(context)
        Q_UNUSED(disambiguation)
        Q_UNUSED(n)
        if (sourceText == nullptr) {
            return {};
        }
        const std::string* found = catalog_.find(sourceText);
        return found == nullptr ? QString{} : QString::fromUtf8(found->data(), static_cast<qsizetype>(found->size()));
    }
    [[nodiscard]] bool isEmpty() const override { return false; }

private:
    i18n::PoCatalog catalog_;
};

PoTranslator* g_translator = nullptr;  // gehört der Anwendung (Eltern-Objekt)
QString g_language = QStringLiteral("de");

QString resourceText(const QString& path) {
    QFile file{path};
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return QString::fromUtf8(file.readAll());
}

}  // namespace

QString storedLanguage() {
    QSettings settings{QStringLiteral("WhiteHoleFX"), QStringLiteral("WhiteHoleFX")};
    const QString code = settings.value(QString::fromLatin1(kLanguageKey)).toString();
    return i18n::isKnownLanguage(code.toStdString()) ? code : QString{};
}

void storeLanguage(const QString& code) {
    QSettings settings{QStringLiteral("WhiteHoleFX"), QStringLiteral("WhiteHoleFX")};
    settings.setValue(QString::fromLatin1(kLanguageKey), code);
}

QString initialLanguage(const QString& override) {
    if (i18n::isKnownLanguage(override.toStdString())) {
        return override;
    }
    const QString stored = storedLanguage();
    if (!stored.isEmpty()) {
        return stored;
    }
    return QString::fromStdString(i18n::languageFromLocale(QLocale::system().name().toStdString()));
}

bool installLanguage(const QString& code) {
    if (g_translator != nullptr) {
        QCoreApplication::removeTranslator(g_translator);
        delete g_translator;
        g_translator = nullptr;
    }
    g_language = QStringLiteral("de");
    if (code == QLatin1String("de") || !i18n::isKnownLanguage(code.toStdString())) {
        return code == QLatin1String("de");
    }
    const QByteArray text = resourceText(QStringLiteral(":/i18n/%1.po").arg(code)).toUtf8();
    auto catalog = i18n::PoCatalog::parse(std::string_view{text.constData(), static_cast<std::size_t>(text.size())});
    if (!catalog.ok()) {
        return false;
    }
    g_translator = new PoTranslator{std::move(catalog).value()};
    QCoreApplication::installTranslator(g_translator);
    g_language = code;
    return true;
}

QString currentLanguage() { return g_language; }

QList<std::pair<QString, QString>> availableLanguages() {
    QList<std::pair<QString, QString>> result;
    for (const i18n::Language& language : i18n::kLanguages) {
        result.append({QString::fromUtf8(language.code.data(), static_cast<qsizetype>(language.code.size())),
                       QString::fromUtf8(language.nativeName.data(), static_cast<qsizetype>(language.nativeName.size()))});
    }
    return result;
}

QString tutorialText(const QString& code) {
    for (const QString& candidate : {code, QStringLiteral("en"), QStringLiteral("de")}) {
        const QString text = resourceText(QStringLiteral(":/tutorial/%1.md").arg(candidate));
        if (!text.isEmpty()) {
            return text;
        }
    }
    return {};
}

}  // namespace whitehole::studio
