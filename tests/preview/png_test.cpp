// Tests des PNG-Schreibers: Aufbau der Datei, Prüfsummen und Inhalt der Pixeldaten.
#include <catch2/catch_test_macros.hpp>

#include <cstring>
#include <string>
#include <vector>

#include "whitehole/preview/png.hpp"

using namespace whitehole;
using namespace whitehole::preview;

namespace {

std::uint32_t u32(const std::vector<std::uint8_t>& b, std::size_t pos) {
    return (std::uint32_t{b[pos]} << 24) | (std::uint32_t{b[pos + 1]} << 16) | (std::uint32_t{b[pos + 2]} << 8) | b[pos + 3];
}

struct Chunk {
    std::string type;
    std::vector<std::uint8_t> data;
    bool crcOk{false};
};

std::vector<Chunk> chunks(const std::vector<std::uint8_t>& png) {
    std::vector<Chunk> out;
    std::size_t pos = 8;
    while (pos + 12 <= png.size()) {
        const std::uint32_t length = u32(png, pos);
        Chunk c;
        c.type.assign(reinterpret_cast<const char*>(&png[pos + 4]), 4);
        c.data.assign(png.begin() + static_cast<std::ptrdiff_t>(pos + 8), png.begin() + static_cast<std::ptrdiff_t>(pos + 8 + length));
        const std::vector<std::uint8_t> body(png.begin() + static_cast<std::ptrdiff_t>(pos + 4),
                                             png.begin() + static_cast<std::ptrdiff_t>(pos + 8 + length));
        c.crcOk = crc32(body) == u32(png, pos + 8 + length);
        out.push_back(std::move(c));
        pos += 12 + length;
    }
    return out;
}

/// Liest einen zlib-Strom aus unkomprimierten Blöcken zurück (genau das, was der Schreiber erzeugt).
std::vector<std::uint8_t> inflateStored(const std::vector<std::uint8_t>& z, bool& adlerOk) {
    std::vector<std::uint8_t> raw;
    REQUIRE(z.size() > 6);
    REQUIRE(z[0] == 0x78);
    std::size_t pos = 2;
    bool last = false;
    while (!last) {
        REQUIRE(pos + 5 <= z.size());
        last = (z[pos] & 1U) != 0;
        REQUIRE((z[pos] & 6U) == 0);  // Typ 00 = unkomprimiert
        const std::size_t len = z[pos + 1] | (std::size_t{z[pos + 2]} << 8);
        const std::size_t nlen = z[pos + 3] | (std::size_t{z[pos + 4]} << 8);
        REQUIRE((len ^ nlen) == 0xFFFF);
        pos += 5;
        REQUIRE(pos + len <= z.size());
        raw.insert(raw.end(), z.begin() + static_cast<std::ptrdiff_t>(pos), z.begin() + static_cast<std::ptrdiff_t>(pos + len));
        pos += len;
    }
    REQUIRE(pos + 4 == z.size());
    adlerOk = adler32(raw) == u32(z, pos);
    return raw;
}

Image gradient(std::uint32_t w, std::uint32_t h) {
    Image image;
    image.width = w;
    image.height = h;
    image.rgba.resize(std::size_t{w} * h * 4);
    for (std::uint32_t y = 0; y < h; ++y) {
        for (std::uint32_t x = 0; x < w; ++x) {
            std::uint8_t* px = &image.rgba[(std::size_t{y} * w + x) * 4];
            px[0] = static_cast<std::uint8_t>(x * 7);
            px[1] = static_cast<std::uint8_t>(y * 13);
            px[2] = static_cast<std::uint8_t>(x + y);
            px[3] = 255;
        }
    }
    return image;
}

}  // namespace

TEST_CASE("Prüfsummen: bekannte Referenzwerte", "[png]") {
    const std::string check = "123456789";
    const std::vector<std::uint8_t> bytes(check.begin(), check.end());
    CHECK(crc32(bytes) == 0xCBF43926U);
    const std::string wiki = "Wikipedia";
    CHECK(adler32(std::vector<std::uint8_t>(wiki.begin(), wiki.end())) == 0x11E60398U);
    CHECK(crc32({}) == 0U);
    CHECK(adler32({}) == 1U);
}

TEST_CASE("PNG: Signatur, Kopf und Chunk-Reihenfolge", "[png]") {
    const auto png = encodePng(gradient(5, 3));
    REQUIRE(png.ok());
    const std::vector<std::uint8_t>& b = png.value();
    const std::uint8_t signature[8] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    REQUIRE(b.size() > 8);
    CHECK(std::memcmp(b.data(), signature, 8) == 0);

    const auto cs = chunks(b);
    REQUIRE(cs.size() == 3);
    CHECK(cs[0].type == "IHDR");
    CHECK(cs[1].type == "IDAT");
    CHECK(cs[2].type == "IEND");
    for (const Chunk& c : cs) {
        CHECK(c.crcOk);
    }
    REQUIRE(cs[0].data.size() == 13);
    CHECK(u32(cs[0].data, 0) == 5);
    CHECK(u32(cs[0].data, 4) == 3);
    CHECK(cs[0].data[8] == 8);   // Bittiefe
    CHECK(cs[0].data[9] == 6);   // RGBA
    CHECK(cs[0].data[12] == 0);  // kein Interlace
    CHECK(cs[2].data.empty());
}

TEST_CASE("PNG: die Pixeldaten kommen unverändert zurück", "[png]") {
    const Image image = gradient(17, 9);
    const auto png = encodePng(image);
    REQUIRE(png.ok());
    const auto cs = chunks(png.value());
    bool adlerOk = false;
    const std::vector<std::uint8_t> raw = inflateStored(cs[1].data, adlerOk);
    CHECK(adlerOk);
    REQUIRE(raw.size() == std::size_t{9} * (1 + 17 * 4));
    for (std::uint32_t y = 0; y < 9; ++y) {
        const std::size_t row = std::size_t{y} * (1 + 17 * 4);
        CHECK(raw[row] == 0);  // Filter "keiner"
        for (std::size_t i = 0; i < 17 * 4; ++i) {
            REQUIRE(raw[row + 1 + i] == image.rgba[std::size_t{y} * 17 * 4 + i]);
        }
    }
}

TEST_CASE("PNG: große Bilder werden auf mehrere Blöcke verteilt", "[png]") {
    const Image image = gradient(300, 200);  // 240 200 Byte Rohdaten: mehr als ein 64-KiB-Block
    const auto png = encodePng(image);
    REQUIRE(png.ok());
    const auto cs = chunks(png.value());
    bool adlerOk = false;
    const std::vector<std::uint8_t> raw = inflateStored(cs[1].data, adlerOk);
    CHECK(adlerOk);
    CHECK(raw.size() == std::size_t{200} * (1 + 300 * 4));
    CHECK(raw.size() > 65535U * 3);
    CHECK(cs[1].crcOk);
}

TEST_CASE("PNG: ungültige Bilder werden abgelehnt", "[png]") {
    Image image = gradient(4, 4);
    image.rgba.pop_back();
    CHECK_FALSE(encodePng(image).ok());
    CHECK_FALSE(encodePng(Image{}).ok());
    Image zero;
    zero.width = 0;
    zero.height = 3;
    CHECK_FALSE(encodePng(zero).ok());
}

TEST_CASE("PNG: writePng meldet unbeschreibbare Pfade", "[png]") {
    const auto r = writePng(gradient(2, 2), "/nicht/vorhanden/bild.png");
    REQUIRE_FALSE(r.ok());
    CHECK(r.error().code == ErrorCode::IoError);
    CHECK_FALSE(writePng(Image{}, "/tmp/x.png").ok());
}
