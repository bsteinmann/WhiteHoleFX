// stall_watchdog_win.cpp: Absturz- und Stillstandsbericht für Windows (Gegenstück zu stall_watchdog.cpp unter Linux).
//
// Warum es diese Datei gibt: Unter Windows verschwindet das Fenster bei einem Absturz spurlos, und ein Freeze
// lässt sich ohne Debugger nicht ansehen. Diese Datei setzt dieselbe Schnittstelle (stall_watchdog.hpp) für Windows um:
//   - Absturz (nicht abgefangene Ausnahme, abort(), std::terminate): ein Textbericht (Version, Ausnahmecode, Adresse als
//     Modul+Offset, Aufrufstapel, geladene Module, GPU-Adapter, Windows-Version) und ein Minidump (.dmp) in
//     <Cache>/crash/. Dazu eine Marke "pending.txt", damit das Studio beim nächsten Start auf den Bericht hinweist.
//   - Stillstand (Hauptthread meldet sich 5 s nicht): derselbe Bericht mit dem Stapel des Hauptthreads und ein Dump,
//     das Programm läuft weiter.
// Funktionsnamen stehen nur im Bericht, wenn die .pdb neben der .exe liegt; sonst Modul+Offset, was sich mit der .pdb
// des Pakets (Artefakt "Symbole" der CI) später auflösen lässt. Ausschalten: Umgebungsvariable WHITEHOLEFX_NO_WATCHDOG=1.
// Der Handler läuft in einem schon beschädigten Prozess und benutzt darum nur Windows-Aufrufe und feste Puffer.
#include "stall_watchdog.hpp"

#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <dbghelp.h>
#include <tlhelp32.h>

#include <atomic>
#include <csignal>
#include <cwchar>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <thread>

#pragma comment(lib, "dbghelp.lib")

namespace whitehole::studio {

namespace {

constexpr ULONGLONG kStallMs = 5000;  // so lange darf der Hauptthread schweigen
constexpr ULONGLONG kPollMs = 250;
constexpr int kMaxFrames = 48;

wchar_t g_dir[MAX_PATH + 16];  // Berichtsordner
char g_version[32];
char g_adapter[256];
char g_note[512];  // Hinweis zur Ursache (zum Beispiel was() einer nicht abgefangenen Ausnahme)
std::atomic<ULONGLONG> g_beatMs{0};
std::atomic<bool> g_started{false};
std::atomic<bool> g_beatSeen{false};  // erst nach dem ersten Herzschlag wachen (der Start darf lange dauern)
std::atomic<bool> g_crashing{false};
HANDLE g_mainThread = nullptr;
bool g_symbolsReady = false;

/// Kleiner Schreiber auf eine Datei, ohne Heap (ein fester Puffer je Zeile).
class Out {
public:
    explicit Out(const wchar_t* path) : file_(CreateFileW(path, GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr)) {}
    ~Out() {
        if (file_ != INVALID_HANDLE_VALUE) {
            CloseHandle(file_);
        }
    }
    Out(const Out&) = delete;
    Out& operator=(const Out&) = delete;
    void print(const char* format, ...) {
        char line[1024];
        va_list args;
        va_start(args, format);
        const int n = std::vsnprintf(line, sizeof line, format, args);
        va_end(args);
        if (n > 0 && file_ != INVALID_HANDLE_VALUE) {
            DWORD written = 0;
            WriteFile(file_, line, static_cast<DWORD>(n < static_cast<int>(sizeof line) ? n : static_cast<int>(sizeof line) - 1), &written, nullptr);
        }
    }

private:
    HANDLE file_;
};

void timestamp(wchar_t* out, int size) {
    SYSTEMTIME t;
    GetLocalTime(&t);
    swprintf(out, static_cast<size_t>(size), L"%04d%02d%02d-%02d%02d%02d", t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond);
}

void pathFor(wchar_t* out, int size, const wchar_t* kind, const wchar_t* stamp, const wchar_t* extension) {
    swprintf(out, static_cast<size_t>(size), L"%ls\\%ls-%ls.%ls", g_dir, kind, stamp, extension);
}

void prepareSymbols() {
    if (!g_symbolsReady) {
        SymSetOptions(SYMOPT_DEFERRED_LOADS | SYMOPT_UNDNAME | SYMOPT_LOAD_LINES);
        g_symbolsReady = SymInitialize(GetCurrentProcess(), nullptr, TRUE) != FALSE;
    }
}

/// Sammelt den Aufrufstapel ab diesem Kontext (nur x64; sonst nur die aktuelle Adresse).
int walkStack(CONTEXT context, HANDLE thread, DWORD64* frames, int maxFrames) {
#if defined(_M_X64)
    STACKFRAME64 frame{};
    frame.AddrPC.Offset = context.Rip;
    frame.AddrFrame.Offset = context.Rbp;
    frame.AddrStack.Offset = context.Rsp;
    frame.AddrPC.Mode = frame.AddrFrame.Mode = frame.AddrStack.Mode = AddrModeFlat;
    int count = 0;
    while (count < maxFrames &&
           StackWalk64(IMAGE_FILE_MACHINE_AMD64, GetCurrentProcess(), thread, &frame, &context, nullptr, SymFunctionTableAccess64,
                       SymGetModuleBase64, nullptr) != FALSE &&
           frame.AddrPC.Offset != 0) {
        frames[count++] = frame.AddrPC.Offset;
    }
    return count;
#else
    (void)context;
    (void)thread;
    (void)frames;
    (void)maxFrames;
    return 0;
#endif
}

void describeAddress(Out& out, DWORD64 address) {
    char module[MAX_PATH] = "?";
    DWORD64 offset = 0;
    HMODULE handle = nullptr;
    if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           reinterpret_cast<LPCSTR>(address), &handle) != FALSE) {
        GetModuleFileNameA(handle, module, MAX_PATH);
        offset = address - reinterpret_cast<DWORD64>(handle);
    }
    const char* name = std::strrchr(module, '\\');
    name = name != nullptr ? name + 1 : module;
    alignas(SYMBOL_INFO) char buffer[sizeof(SYMBOL_INFO) + 256];
    auto* symbol = reinterpret_cast<SYMBOL_INFO*>(buffer);
    std::memset(buffer, 0, sizeof buffer);
    symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
    symbol->MaxNameLen = 255;
    DWORD64 displacement = 0;
    if (g_symbolsReady && SymFromAddr(GetCurrentProcess(), address, &displacement, symbol) != FALSE) {
        out.print("  %s!%s+0x%llx  (%s+0x%llx)\n", name, symbol->Name, static_cast<unsigned long long>(displacement), name,
                  static_cast<unsigned long long>(offset));
    } else {
        out.print("  %s+0x%llx\n", name, static_cast<unsigned long long>(offset));
    }
}

const char* exceptionName(DWORD code) {
    switch (code) {
        case EXCEPTION_ACCESS_VIOLATION: return "Zugriffsverletzung";
        case EXCEPTION_STACK_OVERFLOW: return "Stapelüberlauf";
        case EXCEPTION_ILLEGAL_INSTRUCTION: return "ungültiger Befehl";
        case EXCEPTION_INT_DIVIDE_BY_ZERO: return "Ganzzahl durch 0";
        case EXCEPTION_FLT_DIVIDE_BY_ZERO: return "Fließkommazahl durch 0";
        case EXCEPTION_IN_PAGE_ERROR: return "Seitenfehler beim Lesen (Datei/Treiber)";
        case EXCEPTION_ARRAY_BOUNDS_EXCEEDED: return "Feldgrenze überschritten";
        case 0xE06D7363: return "nicht abgefangene C++-Ausnahme";
        case 0xC0000409: return "Sicherheitsprüfung des Laufzeitsystems (Stapelpuffer/abort)";
        default: return "";
    }
}

void writeSystemInfo(Out& out) {
    out.print("WhiteHoleFX %s\n", g_version[0] != '\0' ? g_version : "?");
    SYSTEMTIME t;
    GetLocalTime(&t);
    out.print("Zeit: %04d-%02d-%02d %02d:%02d:%02d\n", t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond);
    using RtlGetVersionFn = LONG(WINAPI*)(OSVERSIONINFOW*);
    OSVERSIONINFOW version{};
    version.dwOSVersionInfoSize = sizeof version;
    if (HMODULE ntdll = GetModuleHandleW(L"ntdll.dll")) {
        if (auto fn = reinterpret_cast<RtlGetVersionFn>(GetProcAddress(ntdll, "RtlGetVersion"))) {
            fn(&version);
            out.print("Windows: %lu.%lu Build %lu\n", version.dwMajorVersion, version.dwMinorVersion, version.dwBuildNumber);
        }
    }
    out.print("GPU-Adapter: %s\n", g_adapter[0] != '\0' ? g_adapter : "(noch nicht ermittelt)");
    if (g_note[0] != '\0') {
        out.print("Hinweis: %s\n", g_note);
    }
}

void writeModules(Out& out) {
    out.print("\nGeladene Module (Name, Basisadresse, Größe):\n");
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, GetCurrentProcessId());
    if (snapshot == INVALID_HANDLE_VALUE) {
        return;
    }
    MODULEENTRY32W entry{};
    entry.dwSize = sizeof entry;
    int shown = 0;
    for (BOOL ok = Module32FirstW(snapshot, &entry); ok != FALSE && shown < 200; ok = Module32NextW(snapshot, &entry), ++shown) {
        out.print("  %-32ls %p %lu\n", entry.szModule, static_cast<void*>(entry.modBaseAddr), entry.modBaseSize);
    }
    CloseHandle(snapshot);
}

void writeDump(const wchar_t* path, EXCEPTION_POINTERS* pointers) {
    HANDLE file = CreateFileW(path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return;
    }
    MINIDUMP_EXCEPTION_INFORMATION info{};
    if (pointers != nullptr) {
        info.ThreadId = GetCurrentThreadId();
        info.ExceptionPointers = pointers;
        info.ClientPointers = FALSE;
    }
    const auto type = static_cast<MINIDUMP_TYPE>(MiniDumpNormal | MiniDumpWithThreadInfo | MiniDumpWithUnloadedModules);
    MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), file, type, pointers != nullptr ? &info : nullptr, nullptr, nullptr);
    CloseHandle(file);
}

/// Marke für den nächsten Start: Pfad des Berichts, damit das Studio darauf hinweisen kann.
void markPending(const wchar_t* reportPath) {
    wchar_t marker[MAX_PATH + 32];
    swprintf(marker, MAX_PATH + 32, L"%ls\\pending.txt", g_dir);
    char utf8[MAX_PATH * 3];
    const int n = WideCharToMultiByte(CP_UTF8, 0, reportPath, -1, utf8, static_cast<int>(sizeof utf8), nullptr, nullptr);
    HANDLE file = CreateFileW(marker, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file != INVALID_HANDLE_VALUE) {
        DWORD written = 0;
        WriteFile(file, utf8, static_cast<DWORD>(n > 1 ? n - 1 : 0), &written, nullptr);
        CloseHandle(file);
    }
}

/// Schreibt Bericht und Dump. `pointers` ist null bei abort() und Stillstand; `frames` sind schon gesammelt (Stillstand).
void writeReport(const char* title, bool crash, EXCEPTION_POINTERS* pointers, const DWORD64* frames, int frameCount) {
    wchar_t stamp[32];
    timestamp(stamp, 32);
    wchar_t textPath[MAX_PATH + 64];
    wchar_t dumpPath[MAX_PATH + 64];
    const wchar_t* kind = crash ? L"crash" : L"stall";
    pathFor(textPath, MAX_PATH + 64, kind, stamp, L"txt");
    pathFor(dumpPath, MAX_PATH + 64, kind, stamp, L"dmp");

    prepareSymbols();
    DWORD64 own[kMaxFrames];
    if (frames == nullptr) {
        CONTEXT context{};
        if (pointers != nullptr && pointers->ContextRecord != nullptr) {
            context = *pointers->ContextRecord;
        } else {
            RtlCaptureContext(&context);
        }
        frameCount = walkStack(context, GetCurrentThread(), own, kMaxFrames);
        frames = own;
    }
    {
        Out out(textPath);
        out.print("=== %s ===\n", title);
        writeSystemInfo(out);
        if (pointers != nullptr && pointers->ExceptionRecord != nullptr) {
            const EXCEPTION_RECORD* record = pointers->ExceptionRecord;
            out.print("Ausnahme: 0x%08lx %s\n", record->ExceptionCode, exceptionName(record->ExceptionCode));
            out.print("Adresse:\n");
            describeAddress(out, reinterpret_cast<DWORD64>(record->ExceptionAddress));
            if (record->ExceptionCode == EXCEPTION_ACCESS_VIOLATION && record->NumberParameters >= 2) {
                out.print("Zugriff: %s an Adresse 0x%llx\n", record->ExceptionInformation[0] == 0 ? "Lesen" : (record->ExceptionInformation[0] == 1 ? "Schreiben" : "Ausführen"),
                          static_cast<unsigned long long>(record->ExceptionInformation[1]));
            }
        }
        out.print("\nAufrufstapel (%d Einträge):\n", frameCount);
        for (int i = 0; i < frameCount; ++i) {
            describeAddress(out, frames[i]);
        }
        writeModules(out);
    }
    writeDump(dumpPath, pointers);
    if (crash) {
        markPending(textPath);
    }
}

LONG WINAPI onException(EXCEPTION_POINTERS* pointers) {
    if (!g_crashing.exchange(true)) {
        writeReport("Absturz (nicht abgefangene Ausnahme)", true, pointers, nullptr, 0);
    }
    return EXCEPTION_EXECUTE_HANDLER;  // beendet den Prozess still, ohne Windows-Fehlerdialog
}

void onAbortOrTerminate(const char* title) {
    if (!g_crashing.exchange(true)) {
        writeReport(title, true, nullptr, nullptr, 0);
    }
    TerminateProcess(GetCurrentProcess(), 3);
}

extern "C" void onAbortSignal(int) { onAbortOrTerminate("Absturz (abort)"); }

void onTerminate() {
    try {
        if (std::current_exception()) {
            std::rethrow_exception(std::current_exception());
        }
    } catch (const std::exception& error) {
        std::snprintf(g_note, sizeof g_note, "nicht abgefangene Ausnahme: %s", error.what());
    } catch (...) {
        std::snprintf(g_note, sizeof g_note, "nicht abgefangene Ausnahme unbekannten Typs");
    }
    onAbortOrTerminate("Absturz (std::terminate)");
}

void watchLoop() {
    bool reported = false;
    for (;;) {
        Sleep(static_cast<DWORD>(kPollMs));
        const ULONGLONG idle = GetTickCount64() - g_beatMs.load();
        if (idle > kStallMs && !reported && !g_crashing.load() && g_beatSeen.load()) {
            reported = true;
            prepareSymbols();  // vor dem Anhalten des Hauptthreads, damit danach nichts mehr auf seine Sperren warten kann
            DWORD64 frames[kMaxFrames];
            int count = 0;
            if (SuspendThread(g_mainThread) != static_cast<DWORD>(-1)) {
                CONTEXT context{};
                context.ContextFlags = CONTEXT_FULL;
                if (GetThreadContext(g_mainThread, &context) != FALSE) {
                    count = walkStack(context, g_mainThread, frames, kMaxFrames);
                }
                ResumeThread(g_mainThread);
            }
            char title[96];
            std::snprintf(title, sizeof title, "Stillstand: Hauptthread reagiert seit über %llu ms nicht", static_cast<unsigned long long>(idle));
            writeReport(title, false, nullptr, frames, count);
        } else if (idle <= kStallMs) {
            reported = false;
        }
    }
}

}  // namespace

bool startStallWatchdog(const char* logPath) {
    char* off = nullptr;
    size_t length = 0;
    const bool disabled = _dupenv_s(&off, &length, "WHITEHOLEFX_NO_WATCHDOG") == 0 && off != nullptr && off[0] != '\0' && off[0] != '0';
    std::free(off);
    if (disabled || g_started.exchange(true)) {
        return false;
    }
    // Berichtsordner: neben der Logdatei, Unterordner "crash"
    wchar_t base[MAX_PATH] = L"";
    if (logPath != nullptr) {
        MultiByteToWideChar(CP_ACP, 0, logPath, -1, base, MAX_PATH);
    }
    wchar_t* slash = std::wcsrchr(base, L'\\');
    if (slash == nullptr) {
        slash = std::wcsrchr(base, L'/');
    }
    if (slash != nullptr) {
        *slash = L'\0';
    }
    swprintf(g_dir, MAX_PATH + 16, L"%ls\\crash", base[0] != L'\0' ? base : L".");
    CreateDirectoryW(g_dir, nullptr);

    DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &g_mainThread, 0, FALSE, DUPLICATE_SAME_ACCESS);
    ULONG guarantee = 128 * 1024;  // Platz für den Handler, auch nach einem Stapelüberlauf
    SetThreadStackGuarantee(&guarantee);
    SetUnhandledExceptionFilter(onException);
    std::signal(SIGABRT, onAbortSignal);
    std::set_terminate(onTerminate);
    g_beatMs.store(GetTickCount64());
    std::thread(watchLoop).detach();
    return true;
}

void stallHeartbeat() {
    g_beatMs.store(GetTickCount64());
    g_beatSeen.store(true);
}

void stallCountEvent(const char*, int) {}

void stallForTest(int milliseconds) { Sleep(static_cast<DWORD>(milliseconds)); }

void stallSetInfo(const char* version, const char* gpuAdapter) {
    if (version != nullptr) {
        std::snprintf(g_version, sizeof g_version, "%s", version);
    }
    if (gpuAdapter != nullptr) {
        std::snprintf(g_adapter, sizeof g_adapter, "%s", gpuAdapter);
    }
}

}  // namespace whitehole::studio

#endif  // _WIN32
