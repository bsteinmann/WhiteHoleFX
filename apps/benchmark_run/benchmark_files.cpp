// benchmark_files.cpp: siehe benchmark_files.hpp.
#include "benchmark_files.hpp"

#include <cstdio>
#include <fstream>

namespace whitehole::benchapp {

bool hasCsvExtension(const std::string& path) noexcept {
    return path.size() > 4 && path.compare(path.size() - 4, 4, ".csv") == 0;
}

Result<void> writeBenchmarkFile(const BenchmarkResult& result, const std::string& path) {
    std::ofstream file{path, std::ios::binary};
    if (file) {
        file << (hasCsvExtension(path) ? runtime::benchmarkCsv(result.run, result.info)
                                       : runtime::benchmarkMarkdown(result.run, result.info, result.limits));
        file.flush();
    }
    if (!file) {
        return Error{ErrorCode::IoError, path + " ließ sich nicht schreiben."};
    }
    return {};
}

std::string benchmarkFileStem(const std::tm& local) {
    char text[96]{};  // groß genug auch für unsinnige Werte (jedes %d höchstens 11 Zeichen)
    std::snprintf(text, sizeof text, "whitehole-benchmark-%04d%02d%02d-%02d%02d", local.tm_year + 1900,
                  local.tm_mon + 1, local.tm_mday, local.tm_hour, local.tm_min);
    return text;
}

}  // namespace whitehole::benchapp
