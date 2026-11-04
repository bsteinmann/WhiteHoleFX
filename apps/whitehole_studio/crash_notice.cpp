// crash_notice.cpp: Umsetzung zu crash_notice.hpp.
#include "crash_notice.hpp"

#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMessageBox>
#include <QPushButton>
#include <QUrl>
#include <QWidget>

namespace whitehole::studio {

QString takePendingCrashReport(const QString& cacheDir) {
    const QString marker = QDir{cacheDir}.filePath(QStringLiteral("crash/pending.txt"));
    QFile file{marker};
    if (!file.exists() || !file.open(QIODevice::ReadOnly)) {
        return {};
    }
    const QString path = QString::fromUtf8(file.readAll()).trimmed();
    file.close();
    QFile::remove(marker);
    return QFileInfo::exists(path) ? path : QString{};
}

void showCrashNotice(QWidget* parent, const QString& reportPath) {
    if (reportPath.isEmpty()) {
        return;
    }
    const QString folder = QFileInfo{reportPath}.absolutePath();
    auto* box = new QMessageBox{QMessageBox::Warning, QObject::tr("Absturz beim letzten Mal"),
                                QObject::tr("Das Studio ist beim letzten Mal abgestürzt."), QMessageBox::Ok, parent};
    box->setInformativeText(QObject::tr("Ein Bericht (.txt) und ein Speicherabbild (.dmp) liegen im Ordner:\n%1\n\n"
                                        "Bitte schicke beide Dateien an den Entwickler. Das Speicherabbild enthält Teile des "
                                        "Arbeitsspeichers; gib es nur weiter, wenn du damit einverstanden bist.")
                                .arg(QDir::toNativeSeparators(folder)));
    QPushButton* open = box->addButton(QObject::tr("Ordner öffnen"), QMessageBox::ActionRole);
    QObject::connect(open, &QPushButton::clicked, box, [folder]() { QDesktopServices::openUrl(QUrl::fromLocalFile(folder)); });
    box->setAttribute(Qt::WA_DeleteOnClose);
    box->open();
}

}  // namespace whitehole::studio
