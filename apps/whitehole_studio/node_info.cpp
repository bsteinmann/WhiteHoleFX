// node_info.cpp: Umsetzung zu node_info.hpp (Nachschlagen, Übersetzen, Umbrechen, Vollständigkeitsprüfung).
#include "node_info.hpp"

#include <QCoreApplication>
#include <QStringList>

#include <set>

namespace whitehole::studio {

QString wrapWords(const QString& text, int columns) {
    QStringList lines;
    for (const QString& paragraph : text.split(QLatin1Char('\n'))) {
        QString line;
        for (const QString& word : paragraph.split(QLatin1Char(' '), Qt::SkipEmptyParts)) {
            if (!line.isEmpty() && line.size() + 1 + word.size() > columns) {
                lines << line;
                line.clear();
            }
            line += (line.isEmpty() ? QString{} : QStringLiteral(" ")) + word;
        }
        lines << line;
    }
    return lines.join(QLatin1Char('\n'));
}

QString nodeInfoText(const std::string& typeId, int columns) {
    for (const NodeInfoEntry& entry : nodeInfoEntries()) {
        if (typeId == entry.typeId) {
            // Die Kataloge sind nach Quelltext geordnet; der Kontext ist ihnen gleich (siehe PoTranslator).
            return wrapWords(QCoreApplication::translate("NodeInfo", entry.text), columns);
        }
    }
    return {};
}

std::vector<std::string> nodeInfoProblems(const graph::INodeRegistry& registry) {
    std::vector<std::string> problems;
    std::set<std::string> known;
    for (const graph::NodeDescriptor* type : registry.all()) {
        known.insert(type->typeId);
        bool found = false;
        for (const NodeInfoEntry& entry : nodeInfoEntries()) {
            found = found || type->typeId == entry.typeId;
        }
        if (!found) {
            problems.push_back("keine Beschreibung: " + type->typeId);
        }
    }
    for (const NodeInfoEntry& entry : nodeInfoEntries()) {
        if (known.count(entry.typeId) == 0) {
            problems.push_back("Beschreibung ohne Node-Typ: " + std::string{entry.typeId});
        }
    }
    return problems;
}

}  // namespace whitehole::studio
