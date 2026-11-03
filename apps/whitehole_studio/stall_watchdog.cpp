// stall_watchdog.cpp: siehe stall_watchdog.hpp. Nur unter Linux mit glibc wirksam (execinfo.h);
// auf allen anderen Systemen sind die Funktionen leer.
#include "stall_watchdog.hpp"

#include <atomic>
#include <chrono>
#include <cstdlib>
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

void watchLoop() {
    bool reported = false;
    long long stalledSince = 0;
    for (;;) {
        std::this_thread::sleep_for(std::chrono::milliseconds(kPollMs));
        const long long idle = nowMs() - g_beatMs.load();
        if (idle > kStallMs && !reported) {
            reported = true;
            stalledSince = nowMs() - idle;
            std::snprintf(g_title, sizeof g_title, "Studio: Hauptthread reagiert seit über %lld ms nicht mehr\n", idle);
            writeAll(2, g_title);
            writeAll(g_logFd, g_title);
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

void stallForTest(int milliseconds) { std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds)); }

#else  // kein Linux/glibc: nichts zu tun

bool startStallWatchdog(const char*) { return false; }
void stallHeartbeat() {}
void stallForTest(int milliseconds) { std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds)); }

#endif

}  // namespace whitehole::studio
