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
    result.masterDmx = s.value(QStringLiteral("dmx/masterEnabled"), result.masterDmx).toBool();
    result.masterDmxProtocol = std::clamp(s.value(QStringLiteral("dmx/masterProtocol"), 0).toInt(), 0, 1);
    result.masterDmxUniverse = std::clamp(s.value(QStringLiteral("dmx/masterUniverse"), 0).toInt(), 0, 32767);
    result.masterDmxChannel = std::clamp(s.value(QStringLiteral("dmx/masterChannel"), 1).toInt(), 1, 512);
    result.liveDmx = s.value(QStringLiteral("dmx/liveEnabled"), result.liveDmx).toBool();
    result.liveDmxProtocol = std::clamp(s.value(QStringLiteral("dmx/liveProtocol"), 0).toInt(), 0, 1);
    result.liveDmxUniverse = std::clamp(s.value(QStringLiteral("dmx/liveUniverse"), 0).toInt(), 0, 32767);
    result.liveDmxSelectChannel = std::clamp(s.value(QStringLiteral("dmx/liveSelectChannel"), 1).toInt(), 1, 512);
    result.liveDmxFadeChannel = std::clamp(s.value(QStringLiteral("dmx/liveFadeChannel"), 0).toInt(), 0, 512);
    result.outputInterface = s.value(QStringLiteral("network/outInterface")).toString();
    result.inputInterface = s.value(QStringLiteral("network/inInterface")).toString();
    result.midiInput = s.value(QStringLiteral("midi/input")).toString();
    result.midiOutput = s.value(QStringLiteral("midi/output")).toString();
    result.liveColumns = std::clamp(s.value(QStringLiteral("live/columns"), 4).toInt(), 1, 16);
    result.liveRows = std::clamp(s.value(QStringLiteral("live/rows"), 4).toInt(), 1, 16);
    result.liveSplitPreview = s.value(QStringLiteral("live/splitPreview"), true).toBool();
    result.midiProfile = s.value(QStringLiteral("midi/profile")).toString();
    result.midiProfiles = s.value(QStringLiteral("midi/profiles")).toStringList();
    result.liveBindings = s.value(QStringLiteral("midi/liveBindings")).toStringList();
    result.tempoSource = std::clamp(s.value(QStringLiteral("tempo/source"), 0).toInt(), 0, 1);
    result.tempoFixedBpm = std::clamp(s.value(QStringLiteral("tempo/fixedBpm"), 120.0).toDouble(), 30.0, 300.0);
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
    s.setValue(QStringLiteral("dmx/masterEnabled"), settings.masterDmx);
    s.setValue(QStringLiteral("dmx/masterProtocol"), settings.masterDmxProtocol);
    s.setValue(QStringLiteral("dmx/masterUniverse"), settings.masterDmxUniverse);
    s.setValue(QStringLiteral("dmx/masterChannel"), settings.masterDmxChannel);
    s.setValue(QStringLiteral("dmx/liveEnabled"), settings.liveDmx);
    s.setValue(QStringLiteral("dmx/liveProtocol"), settings.liveDmxProtocol);
    s.setValue(QStringLiteral("dmx/liveUniverse"), settings.liveDmxUniverse);
    s.setValue(QStringLiteral("dmx/liveSelectChannel"), settings.liveDmxSelectChannel);
    s.setValue(QStringLiteral("dmx/liveFadeChannel"), settings.liveDmxFadeChannel);
    s.setValue(QStringLiteral("network/outInterface"), settings.outputInterface);
    s.setValue(QStringLiteral("network/inInterface"), settings.inputInterface);
    s.setValue(QStringLiteral("midi/input"), settings.midiInput);
    s.setValue(QStringLiteral("midi/output"), settings.midiOutput);
    s.setValue(QStringLiteral("live/columns"), settings.liveColumns);
    s.setValue(QStringLiteral("live/rows"), settings.liveRows);
    s.setValue(QStringLiteral("live/splitPreview"), settings.liveSplitPreview);
    s.setValue(QStringLiteral("midi/profile"), settings.midiProfile);
    s.setValue(QStringLiteral("midi/profiles"), settings.midiProfiles);
    s.setValue(QStringLiteral("midi/liveBindings"), settings.liveBindings);
    s.setValue(QStringLiteral("tempo/source"), settings.tempoSource);
    s.setValue(QStringLiteral("tempo/fixedBpm"), settings.tempoFixedBpm);
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
