// session_marker.cpp: Umsetzung zu session_marker.hpp.
#include "session_marker.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <cstdio>

#ifdef Q_OS_WIN
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace whitehole::studio {

namespace {

QString markerPath(const QString& cacheDir) { return QDir{cacheDir}.filePath(QStringLiteral("crash/session.txt")); }

qint64 readPid(const QString& path) {
    QFile file{path};
    if (!file.open(QIODevice::ReadOnly)) {
        return 0;
    }
    return QString::fromUtf8(file.readAll()).trimmed().toLongLong();
}

bool processAlive(qint64 pid) {
    if (pid <= 0) {
        return false;
    }
#ifdef Q_OS_WIN
    HANDLE handle = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, static_cast<DWORD>(pid));
    if (handle == nullptr) {
        return false;
    }
    DWORD code = 0;
    const bool alive = GetExitCodeProcess(handle, &code) != FALSE && code == STILL_ACTIVE;
    CloseHandle(handle);
    return alive;
#else
    return QFile::exists(QStringLiteral("/proc/%1").arg(pid));
#endif
}

}  // namespace

bool beginSession(const QString& cacheDir) {
    const QString path = markerPath(cacheDir);
    QDir{}.mkpath(QFileInfo{path}.absolutePath());
    const qint64 own = QCoreApplication::applicationPid();
    const qint64 old = readPid(path);
    // Dieselbe Nummer wie unsere: Windows und Linux vergeben Nummern neu, die alte Sitzung ist also vorbei.
    const bool unclean = QFile::exists(path) && (old == own || !processAlive(old));
    QFile file{path};
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        file.write(QByteArray::number(own));
    }
    return unclean;
}

void endSession(const QString& cacheDir) {
    const QString path = markerPath(cacheDir);
    if (readPid(path) == QCoreApplication::applicationPid()) {
        QFile::remove(path);
    }
}

bool sessionSelfTest() {
    QTemporaryDir dir;
    if (!dir.isValid()) {
        std::fprintf(stderr, "Sitzungsmarke: kein Wegwerf-Ordner\n");
        return false;
    }
    const QString cache = dir.path();
    const QString path = markerPath(cache);
    bool ok = true;
    const auto check = [&ok](bool condition, const char* what) {
        if (!condition) {
            std::fprintf(stderr, "Sitzungsmarke: %s\n", what);
            ok = false;
        }
    };
    check(!beginSession(cache), "erster Start meldet fälschlich ein unsauberes Ende");
    check(QFile::exists(path), "Marke wird nicht angelegt");
    endSession(cache);
    check(!QFile::exists(path), "Marke bleibt nach sauberem Ende liegen");
    check(!beginSession(cache), "Start nach sauberem Ende meldet fälschlich ein unsauberes Ende");
    // Absturz nachstellen: Die Marke bleibt mit der Nummer eines nicht mehr laufenden Prozesses liegen.
    {
        QFile file{path};
        check(file.open(QIODevice::WriteOnly | QIODevice::Truncate), "Marke nicht schreibbar");
        file.write("2147480000");
    }
    check(beginSession(cache), "unsauberes Ende wird nicht erkannt");
    check(readPid(path) == QCoreApplication::applicationPid(), "neue Marke trägt nicht die eigene Nummer");
    // Eine fremde Marke darf das Beenden nicht löschen.
    {
        QFile file{path};
        check(file.open(QIODevice::WriteOnly | QIODevice::Truncate), "Marke nicht schreibbar (2)");
        file.write("2147480001");
    }
    endSession(cache);
    check(QFile::exists(path), "endSession löscht eine fremde Marke");
    return ok;
}

}  // namespace whitehole::studio
