// app_settings.cpp: Umsetzung zu app_settings.hpp. Lesen und Schreiben der Programmeinstellungen über QSettings.
#include "app_settings.hpp"

#include <QSettings>

#include <algorithm>

namespace whitehole::studio {

namespace {

QSettings store() { return QSettings{QStringLiteral("WhiteHoleFX"), QStringLiteral("WhiteHoleFX")}; }

int validPort(int value, int fallback) { return value >= 0 && value <= 65535 ? value : fallback; }

}  // namespace

AppSettings loadAppSettings() {
    const QSettings s = store();
    AppSettings result;
    result.artnetIn = s.value(QStringLiteral("dmx/artnet"), result.artnetIn).toBool();
    result.sacnIn = s.value(QStringLiteral("dmx/sacn"), result.sacnIn).toBool();
    result.artnetPort = validPort(s.value(QStringLiteral("dmx/artnetPort"), result.artnetPort).toInt(), result.artnetPort);
    result.sacnPort = validPort(s.value(QStringLiteral("dmx/sacnPort"), result.sacnPort).toInt(), result.sacnPort);
    result.outputInterface = s.value(QStringLiteral("network/outInterface")).toString();
    result.inputInterface = s.value(QStringLiteral("network/inInterface")).toString();
    result.audioSource = s.value(QStringLiteral("audio/source"), result.audioSource).toString();
    // Eigene Standardwerte: eine Gruppe je Node-Typ. Unlesbare Einträge werden übersprungen.
    QSettings defaults = store();
    defaults.beginGroup(QStringLiteral("nodeDefaults"));
    for (const QString& type : defaults.childGroups()) {
        defaults.beginGroup(type);
        for (const QString& key : defaults.childKeys()) {
            const auto value = runtime::decodeParameterValue(defaults.value(key).toString().toStdString());
            if (value) {
                result.nodeDefaults.set(type.toStdString(), key.toStdString(), *value);
            }
        }
        defaults.endGroup();
    }
    return result;
}

void saveAppSettings(const AppSettings& settings) {
    QSettings s = store();
    s.setValue(QStringLiteral("dmx/artnet"), settings.artnetIn);
    s.setValue(QStringLiteral("dmx/sacn"), settings.sacnIn);
    s.setValue(QStringLiteral("dmx/artnetPort"), settings.artnetPort);
    s.setValue(QStringLiteral("dmx/sacnPort"), settings.sacnPort);
    s.setValue(QStringLiteral("network/outInterface"), settings.outputInterface);
    s.setValue(QStringLiteral("network/inInterface"), settings.inputInterface);
    s.setValue(QStringLiteral("audio/source"), settings.audioSource);
    s.remove(QStringLiteral("nodeDefaults"));
    for (const auto& [type, values] : settings.nodeDefaults.all()) {
        for (const auto& [key, value] : values) {
            s.setValue(QStringLiteral("nodeDefaults/") + QString::fromStdString(type) + QLatin1Char('/') +
                           QString::fromStdString(key),
                       QString::fromStdString(runtime::encodeParameterValue(value)));
        }
    }
}

}  // namespace whitehole::studio
