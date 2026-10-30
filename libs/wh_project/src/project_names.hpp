// project_names.hpp (privat): die Schlüsselwörter der Projektdatei für Aufzählungen.
// Parser und Schreiber benutzen dieselben Tabellen, damit sie sich nicht auseinanderentwickeln.
#pragma once

#include "project_text.hpp"
#include "whitehole/mapping/layout.hpp"
#include "whitehole/mapping/wiring.hpp"
#include "whitehole/output/color_encoder.hpp"
#include "whitehole/output/output_plan.hpp"

namespace whitehole::project::detail {

using mapping::AxisOrder;
using mapping::FirstAxis;
using mapping::NormalizeMode;
using mapping::StartCorner;
using output::ColorOrder;
using output::Protocol;

inline constexpr NameTable<StartCorner, 4> kCorners{{{"top-left", StartCorner::TopLeft},
                                                     {"top-right", StartCorner::TopRight},
                                                     {"bottom-left", StartCorner::BottomLeft},
                                                     {"bottom-right", StartCorner::BottomRight}}};

inline constexpr NameTable<FirstAxis, 2> kFirstAxes{
    {{"rows", FirstAxis::Rows}, {"columns", FirstAxis::Columns}}};

inline constexpr NameTable<AxisOrder, 6> kAxisOrders{{{"XYZ", AxisOrder::XYZ},
                                                      {"XZY", AxisOrder::XZY},
                                                      {"YXZ", AxisOrder::YXZ},
                                                      {"YZX", AxisOrder::YZX},
                                                      {"ZXY", AxisOrder::ZXY},
                                                      {"ZYX", AxisOrder::ZYX}}};

inline constexpr NameTable<Protocol, 3> kProtocols{
    {{"artnet", Protocol::ArtNet}, {"sacn", Protocol::Sacn}, {"ddp", Protocol::Ddp}}};

inline constexpr NameTable<ColorOrder, 6> kColorOrders{{{"RGB", ColorOrder::RGB},
                                                        {"RBG", ColorOrder::RBG},
                                                        {"GRB", ColorOrder::GRB},
                                                        {"GBR", ColorOrder::GBR},
                                                        {"BRG", ColorOrder::BRG},
                                                        {"BGR", ColorOrder::BGR}}};

inline constexpr NameTable<NormalizeMode, 2> kNormalizeModes{
    {{"stretch", NormalizeMode::Stretch}, {"keep-aspect", NormalizeMode::KeepAspect}}};

/// Liest ein Wort aus einer Tabelle; bei Fehler setzt der Reader eine Meldung mit den Namen.
template <typename E, std::size_t N>
E readName(Reader& r, const NameTable<E, N>& table, std::string_view what) {
    const std::string w = r.word(what);
    if (!r.ok()) {
        return table[0].second;
    }
    if (const auto found = lookupName(table, w)) {
        return *found;
    }
    std::string names;
    for (const auto& entry : table) {
        names += (names.empty() ? "" : ", ");
        names += entry.first;
    }
    r.fail(std::string{what} + ": unbekannt \"" + w + "\" (erlaubt: " + names + ")");
    return table[0].second;
}

}  // namespace whitehole::project::detail
