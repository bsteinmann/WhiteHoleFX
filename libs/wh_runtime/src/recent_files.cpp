// recent_files.cpp: siehe recent_files.hpp.
#include "whitehole/runtime/recent_files.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>

namespace whitehole::runtime {

namespace {

/// Windows unterscheidet Groß- und Kleinschreibung in Pfaden nicht.
std::string key(const std::string& path) {
    std::string k = normalizedPath(path);
#ifdef _WIN32
    std::ranges::transform(k, k.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
#endif
    return k;
}

}  // namespace

std::string normalizedPath(const std::string& path) {
    if (path.empty()) {
        return {};
    }
    std::error_code ec;
    const std::filesystem::path absolute = std::filesystem::absolute(std::filesystem::path{path}, ec);
    if (ec) {
        return path;
    }
    return absolute.lexically_normal().generic_string();
}

std::vector<std::string> removeRecentFile(std::vector<std::string> list, const std::string& path) {
    const std::string wanted = key(path);
    std::erase_if(list, [&](const std::string& entry) { return key(entry) == wanted; });
    return list;
}

std::vector<std::string> addRecentFile(std::vector<std::string> list, const std::string& path, std::size_t max) {
    if (path.empty()) {
        return list;
    }
    list = removeRecentFile(std::move(list), path);
    list.insert(list.begin(), normalizedPath(path));
    if (list.size() > max) {
        list.resize(max);
    }
    return list;
}

}  // namespace whitehole::runtime
