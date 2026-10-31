// point_file.cpp: Umsetzung zu point_file.hpp. Datei lesen, Endung bestimmen, an wh_mapping übergeben.
#include "whitehole/runtime/point_file.hpp"

#include "whitehole/project/project.hpp"

namespace whitehole::runtime {

namespace {

std::size_t nameStart(const std::string& path) {
    const std::size_t slash = path.find_last_of("/\\");
    return slash == std::string::npos ? 0 : slash + 1;
}

}  // namespace

Result<std::vector<Vec3>> loadPointsFile(const std::string& path, const mapping::ImportOptions& options) {
    const std::string name = path.substr(nameStart(path));
    const Result<std::string> text = project::readTextFile(path);
    if (!text.ok()) {
        return text.error();
    }
    const std::size_t dot = name.find_last_of('.');
    const std::string extension = dot == std::string::npos ? std::string{} : name.substr(dot);
    Result<std::vector<Vec3>> points = mapping::parsePoints(text.value(), extension, options);
    if (!points.ok()) {
        return Error{points.error().code, name + ": " + points.error().message};
    }
    return points;
}

std::string pointsFixtureName(const std::string& path) {
    const std::string name = path.substr(nameStart(path));
    const std::size_t dot = name.find_last_of('.');
    const std::string stem = dot == std::string::npos || dot == 0 ? name : name.substr(0, dot);
    return stem.empty() ? std::string{"Punkte"} : stem;
}

}  // namespace whitehole::runtime
