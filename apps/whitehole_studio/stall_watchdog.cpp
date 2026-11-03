// stall_watchdog.cpp: siehe stall_watchdog.hpp. Nur unter Linux mit glibc wirksam (execinfo.h);
// auf allen anderen Systemen sind die Funktionen leer.
#include "stall_watchdog.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <thread>

#if defined(__linux__) && defined(__GLIBC__)
#include <execinfo.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <unistd.h>

#include <cstdio>
#include <cstring>
#define WH_STALL_WATCHDOG 1
#endif

namespace whitehole::studio {

#ifdef WH_STALL_WATCHDOG

namespace {

using Clock = std::chrono::steady_clock;

constexpr long long kStallMs = 2000;   // so lange darf der Hauptthread schweigen
constexpr long long kPollMs = 250;     // so oft schaut der Wächter nach
constexpr int kMaxFrames = 48;

// Ereigniszähler: offene Adressierung, Schlüssel = Zeiger auf den Klassennamen | Typ << 48 (0 = frei).
constexpr std::size_t kSlots = 1024;
std::array<std::atomic<std::uint64_t>, kSlots> g_keys{};
std::array<std::atomic<std::uint64_t>, kSlots> g_counts{};

const char* eventTypeName(int type) {
    switch (type) {
        case 1: return "Timer";
        case 12: return "Paint";
        case 13: return "Move";
        case 14: return "Resize";
        case 17: return "Show";
        case 18: return "Hide";
        case 26: return "ShowToParent";
        case 27: return "HideToParent";
        case 43: return "MetaCall(Queued-Slot)";
        case 52: return "DeferredDelete";
        case 68: return "ChildAdded";
        case 71: return "ChildRemoved";
        case 74: return "PolishRequest";
        case 75: return "Polish";
        case 76: return "LayoutRequest";
        case 77: return "UpdateRequest";
        case 78: return "UpdateLater";
        default: return "";
    }
}

std::atomic<long long> g_beatMs{0};
std::atomic<bool> g_started{false};
pthread_t g_mainThread{};
int g_logFd = -1;
char g_title[160];

long long nowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now().time_since_epoch()).count();
}

void writeAll(int fd, const char* text) {
    if (fd < 0) {
        return;
    }
    std::size_t left = std::strlen(text);
    while (left > 0) {
        const ssize_t n = ::write(fd, text, left);
        if (n <= 0) {
            return;
        }
        text += n;
        left -= static_cast<std::size_t>(n);
    }
}

// Nur Aufrufe, die in einem Signalhandler erlaubt sind (write, backtrace_symbols_fd nach Vorwärmen).
void dumpStack(const char* title) {
    void* frames[kMaxFrames];
    const int count = backtrace(frames, kMaxFrames);
    for (const int fd : {2, g_logFd}) {
        writeAll(fd, title);
        if (fd >= 0) {
            backtrace_symbols_fd(frames, count, fd);
        }
    }
}

extern "C" void onStallSignal(int) { dumpStack("\n=== Studio: Hauptthread reagiert nicht, so sieht sein Stapel aus ===\n"); }

extern "C" void onFatalSignal(int signalNumber) {
    dumpStack("\n=== Studio: Absturz (Signal), Aufrufstapel ===\n");
    ::signal(signalNumber, SIG_DFL);
    ::raise(signalNumber);
}

std::array<std::uint64_t, kSlots> snapshotCounts() {
    std::array<std::uint64_t, kSlots> copy{};
    for (std::size_t i = 0; i < kSlots; ++i) {
        copy[i] = g_counts[i].load(std::memory_order_relaxed);
    }
    return copy;
}

void say(const char* text) {
    writeAll(2, text);
    writeAll(g_logFd, text);
}

// Die häufigsten Ereignisse seit `before`: Läuft der Hauptthread in einer Ereignisschleife, steht sie hier oben.
void reportEventDeltas(const std::array<std::uint64_t, kSlots>& before, long long milliseconds) {
    const auto after = snapshotCounts();
    std::array<std::pair<std::uint64_t, std::size_t>, kSlots> ranked{};
    std::size_t used = 0;
    for (std::size_t i = 0; i < kSlots; ++i) {
        if (after[i] > before[i]) {
            ranked[used++] = {after[i] - before[i], i};
        }
    }
    std::sort(ranked.begin(), ranked.begin() + static_cast<std::ptrdiff_t>(used), std::greater<>{});
    std::snprintf(g_title, sizeof g_title, "Studio: häufigste Ereignisse in den letzten %lld ms (Klasse, Typ, Anzahl):\n", milliseconds);
    say(g_title);
    if (used == 0) {
        say("  keine - der Hauptthread verarbeitet keine Qt-Ereignisse (er steckt in einem einzelnen langen Aufruf)\n");
    }
    for (std::size_t r = 0; r < used && r < 8; ++r) {
        const std::uint64_t key = g_keys[ranked[r].second].load();
        const char* name = reinterpret_cast<const char*>(key & 0xFFFFFFFFFFFFULL);
        const int type = static_cast<int>(key >> 48);
        std::snprintf(g_title, sizeof g_title, "  %-28s %3d %-22s %llu\n", name, type, eventTypeName(type),
                      static_cast<unsigned long long>(ranked[r].first));
        say(g_title);
    }
}

void watchLoop() {
    bool reported = false;
    bool secondDone = false;
    long long stalledSince = 0;
    long long sampleAt = 0;
    std::array<std::uint64_t, kSlots> counts{};
    for (;;) {
        std::this_thread::sleep_for(std::chrono::milliseconds(kPollMs));
        const long long idle = nowMs() - g_beatMs.load();
        if (idle > kStallMs && !reported) {
            reported = true;
            secondDone = false;
            stalledSince = nowMs() - idle;
            sampleAt = nowMs();
            counts = snapshotCounts();
            std::snprintf(g_title, sizeof g_title, "Studio: Hauptthread reagiert seit über %lld ms nicht mehr\n", idle);
            writeAll(2, g_title);
            writeAll(g_logFd, g_title);
            pthread_kill(g_mainThread, SIGUSR1);
        } else if (idle > kStallMs && reported && !secondDone && nowMs() - sampleAt >= 1000) {
            // Zweite Probe nach einer Sekunde: Steht er noch an derselben Stelle? Was lief in der Zwischenzeit?
            secondDone = true;
            reportEventDeltas(counts, nowMs() - sampleAt);
            pthread_kill(g_mainThread, SIGUSR1);
        } else if (idle <= kStallMs && reported) {
            reported = false;
            std::snprintf(g_title, sizeof g_title, "Studio: Hauptthread läuft wieder (stand %lld ms)\n", nowMs() - stalledSince);
            writeAll(2, g_title);
            writeAll(g_logFd, g_title);
        }
    }
}

}  // namespace

bool startStallWatchdog(const char* logPath) {
    const char* off = std::getenv("WHITEHOLEFX_NO_WATCHDOG");
    if ((off != nullptr && off[0] != '\0' && off[0] != '0') || g_started.exchange(true)) {
        return false;
    }
    if (logPath != nullptr && logPath[0] != '\0') {
        g_logFd = ::open(logPath, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
    }
    void* warm[4];
    (void)backtrace(warm, 4);  // lädt libgcc jetzt, nicht erst im Signalhandler

    // Eigener Stapel für die Handler: Auch ein Stapelüberlauf soll noch etwas ausgeben.
    static char altStack[64 * 1024];
    stack_t alt{};
    alt.ss_sp = altStack;
    alt.ss_size = sizeof altStack;
    sigaltstack(&alt, nullptr);

    struct sigaction action{};
    action.sa_handler = onFatalSignal;
    sigemptyset(&action.sa_mask);
    action.sa_flags = SA_ONSTACK;
    for (const int s : {SIGSEGV, SIGABRT, SIGBUS, SIGFPE, SIGILL}) {
        sigaction(s, &action, nullptr);
    }
    action.sa_handler = onStallSignal;
    action.sa_flags = SA_RESTART;
    sigaction(SIGUSR1, &action, nullptr);

    g_mainThread = pthread_self();
    g_beatMs.store(nowMs());
    std::thread(watchLoop).detach();
    return true;
}

void stallHeartbeat() { g_beatMs.store(nowMs()); }

void stallCountEvent(const char* name, int type) {
    if (name == nullptr || type < 0 || type > 0xFFFF) {
        return;
    }
    const std::uint64_t key = (reinterpret_cast<std::uintptr_t>(name) & 0xFFFFFFFFFFFFULL) | (static_cast<std::uint64_t>(type) << 48);
    std::size_t slot = static_cast<std::size_t>((key >> 3) * 0x9E3779B97F4A7C15ULL >> 40) % kSlots;
    for (std::size_t probe = 0; probe < kSlots; ++probe, slot = (slot + 1) % kSlots) {
        std::uint64_t seen = g_keys[slot].load(std::memory_order_relaxed);
        if (seen == 0 && g_keys[slot].compare_exchange_strong(seen, key)) {
            seen = key;
        }
        if (seen == key) {
            g_counts[slot].fetch_add(1, std::memory_order_relaxed);
            return;
        }
    }
}

void stallForTest(int milliseconds) { std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds)); }

#else  // kein Linux/glibc: nichts zu tun

bool startStallWatchdog(const char*) { return false; }
void stallHeartbeat() {}
void stallCountEvent(const char*, int) {}
void stallForTest(int milliseconds) { std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds)); }

#endif

}  // namespace whitehole::studio
