// benchmark_report.cpp: CSV, Markdown und Empfehlung (siehe benchmark_report.hpp).
#include "whitehole/runtime/benchmark_report.hpp"

#include <cstdio>
#include <map>
#include <sstream>

namespace whitehole::runtime {

namespace {

std::string ms(double value) {
    char buffer[32];
    std::snprintf(buffer, sizeof buffer, "%.2f", value);
    return buffer;
}

const char* kindId(BenchmarkKind kind) noexcept {
    return kind == BenchmarkKind::Projection ? "projection" : "leds";
}

/// CSV-Feld: in Anführungszeichen, wenn Komma, Anführungszeichen oder Zeilenumbruch darin stehen.
std::string csvField(const std::string& text) {
    if (text.find_first_of(",\"\n") == std::string::npos) {
        return text;
    }
    std::string out = "\"";
    for (const char c : text) {
        out += c == '"' ? std::string{"\"\""} : std::string(1, c);
    }
    return out + "\"";
}

/// Markdown-Zelle: senkrechte Striche und Zeilenumbrüche würden die Tabelle zerreißen.
std::string mdCell(const std::string& text) {
    std::string out;
    for (const char c : text) {
        if (c == '|') {
            out += "\\|";
        } else if (c == '\n') {
            out += ' ';
        } else {
            out += c;
        }
    }
    return out;
}

bool measured(BenchmarkOutcome o) {
    return o == BenchmarkOutcome::Ok || o == BenchmarkOutcome::OverBudget;
}

}  // namespace

bool benchmarkStageStable(const BenchmarkStageResult& result, const BenchmarkLimits& limits) {
    return result.outcome == BenchmarkOutcome::Ok && !result.atLimit && result.stats.frames > 0 &&
           result.stats.p95Ms <= limits.budgetMs;
}

std::optional<BenchmarkStage> benchmarkRecommendation(const BenchmarkRun& run, BenchmarkKind kind,
                                                      const BenchmarkLimits& limits) {
    // Je Größe (Zahl der Abtastpunkte): alle Stufen dieser Größe, die überhaupt etwas ergeben haben.
    struct Size {
        BenchmarkStage first;
        bool any{false};
        bool allStable{true};
    };
    std::map<std::uint64_t, Size> sizes;
    for (const BenchmarkStageResult& r : run.stages) {
        if (r.stage.kind != kind || r.outcome == BenchmarkOutcome::NotRun) {
            continue;
        }
        auto [it, inserted] = sizes.try_emplace(r.stage.points(), Size{r.stage});
        it->second.any = true;
        if (!benchmarkStageStable(r, limits)) {
            it->second.allStable = false;
        }
    }
    std::optional<BenchmarkStage> best;
    for (const auto& [points, size] : sizes) {  // aufsteigend
        if (!size.any || !size.allStable) {
            break;  // größere Stufen schaffen es dann erst recht nicht verlässlich
        }
        best = size.first;
    }
    return best;
}

const char* benchmarkOutcomeId(BenchmarkOutcome outcome) noexcept {
    switch (outcome) {
        case BenchmarkOutcome::Ok: return "ok";
        case BenchmarkOutcome::OverBudget: return "over";
        case BenchmarkOutcome::Skipped: return "skipped";
        case BenchmarkOutcome::NotRun: return "not-run";
        case BenchmarkOutcome::Failed: return "failed";
    }
    return "?";
}

std::string benchmarkCsv(const BenchmarkRun& run, const BenchmarkSystemInfo& info) {
    std::ostringstream out;
    out << "# device," << csvField(info.device) << "\n# backend," << csvField(info.backend) << "\n# driver,"
        << csvField(info.driver) << "\n# version," << csvField(info.version) << "\n# build," << csvField(info.build)
        << "\n";
    out << "kind,stage,scene,points,outcome,frames,median_ms,p95_ms,max_ms,cpu_ms,gpu_ms,at_limit,note\n";
    for (const BenchmarkStageResult& r : run.stages) {
        const bool m = measured(r.outcome);
        out << kindId(r.stage.kind) << ',' << csvField(benchmarkStageLabel(r.stage)) << ','
            << benchmarkSceneId(r.stage.scene) << ',' << r.stage.points() << ',' << benchmarkOutcomeId(r.outcome)
            << ',' << r.stats.frames << ',' << (m ? ms(r.stats.medianMs) : "") << ','
            << (m ? ms(r.stats.p95Ms) : "") << ',' << (m ? ms(r.stats.maxMs) : "") << ','
            << (m ? ms(r.stats.cpuMedianMs) : "") << ','
            << (m && r.stats.gpuMedianMs ? ms(*r.stats.gpuMedianMs) : "") << ',' << (r.atLimit ? "1" : "0") << ','
            << csvField(r.note) << '\n';
    }
    return out.str();
}

std::string benchmarkMarkdown(const BenchmarkRun& run, const BenchmarkSystemInfo& info,
                              const BenchmarkLimits& limits) {
    std::ostringstream out;
    out << "**WhiteHoleFX-Benchmark** " << mdCell(info.version) << " (" << mdCell(info.build) << ")\n\n";
    out << "- Gerät: " << mdCell(info.device) << (info.backend.empty() ? "" : " [" + mdCell(info.backend) + "]")
        << "\n- Treiber: " << (info.driver.empty() ? std::string{"unbekannt"} : mdCell(info.driver)) << "\n\n";
    out << "| Reihe | Stufe | Szene | Ergebnis | Median ms | P95 ms | CPU ms | GPU ms | Hinweis |\n";
    out << "|---|---|---|---|---:|---:|---:|---:|---|\n";
    for (const BenchmarkStageResult& r : run.stages) {
        const bool m = measured(r.outcome);
        std::string note = r.note;
        if (r.atLimit) {
            note = note.empty() ? "am Limit" : "am Limit; " + note;
        }
        out << "| " << kindId(r.stage.kind) << " | " << benchmarkStageLabel(r.stage) << " | "
            << benchmarkSceneId(r.stage.scene) << " | " << benchmarkOutcomeId(r.outcome) << " | "
            << (m ? ms(r.stats.medianMs) : "") << " | " << (m ? ms(r.stats.p95Ms) : "") << " | "
            << (m ? ms(r.stats.cpuMedianMs) : "") << " | "
            << (m && r.stats.gpuMedianMs ? ms(*r.stats.gpuMedianMs) : "") << " | " << mdCell(note) << " |\n";
    }
    out << "\n";
    for (const BenchmarkKind kind : {BenchmarkKind::Projection, BenchmarkKind::Leds}) {
        bool ran = false;
        for (const BenchmarkStageResult& r : run.stages) {
            ran = ran || (r.stage.kind == kind && r.outcome != BenchmarkOutcome::NotRun);
        }
        if (!ran) {
            continue;
        }
        const auto best = benchmarkRecommendation(run, kind, limits);
        out << "- Stabil bei 30 fps (" << kindId(kind) << "): "
            << (best ? "bis " + benchmarkStageLabel(*best) : std::string{"keine Stufe"}) << "\n";
    }
    if (run.aborted) {
        out << "- Abgebrochen: " << mdCell(run.abortReason) << "\n";
    }
    return out.str();
}

}  // namespace whitehole::runtime
